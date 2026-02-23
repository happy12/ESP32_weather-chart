#include <Arduino.h>

#ifndef ARDUINO_ARCH_ESP32
#error "This project requires ESP32"
#endif

/* TODO
 dimmer pin is not implemented, should be used to dimmer the LED
 switch is not implemented, not sure what to do with it. perhaps consider TFR if active, and turn LED orange
*/

//core 1 will be for all update, sendor, display
//core 0 will be for web page parameters, and time sync
TaskHandle_t MainCode1Task;
TaskHandle_t HeapCode0Task;
TaskHandle_t RSSICode0Task;

const unsigned long timerWifi_delay = (30UL * 1000UL);//30000UL;//30sec delay between wifi connection check, in ms (5UL * 60UL * 1000UL)
const unsigned long timerWifiRSSI_delay = (5UL * 1000UL);//5sec delay between wifi connection check, in ms
const unsigned long timerRestart_delay = 1500UL;//1.5sec delay to wait for a restart
const unsigned long timerWeatherFetch_delay = (31UL * 1000UL);//31 second delay between weather data fetch, in ms

#include "wifi_config_portal.h"
#include <DNSServer.h>
#include <ping/ping_sock.h> // Native ESP-IDF Ping headers
#include <lwip/inet.h>// Native ESP-IDF Ping headers
#include <lwip/netdb.h>// Native ESP-IDF Ping headers
#include <lwip/sockets.h>// Native ESP-IDF Ping headers

//prototyping
void MainCore1(void *pvParameters);
void heapMonitorTask(void *pvParameters);
void wifiRSSiMonitorTask(void *pvParameters);

#include "variables.h"
#include "utilCrash.h"
#include "utilFile.h"
#include "wifi_config_portal.h"


UtilNeoPixel ledStatus(1, LED_STATUS, NEO_RGB + NEO_KHZ800);
UtilNeoPixel ws2818_leds(1, LED_CONTROL, NEO_RGB + NEO_KHZ800);//nb can be updated on the go


void setup() {
  DEBUG_BEGIN(115200);
  DEBUG_SERIALWAIT; // Wait for Serial to be ready (for native USB devices)
  delay(500);// Give the hardware a second to stabilize
  DEBUG_PRINTLN("\n");
  DEBUG_PRINTLN("* * * --> INITIALIZATION <-- * * *");
  DEBUG_VERBOSE; //this will silence the output monitor of extra bug messages from library like webserver and wifi

#if MATDEBUG
  DEBUG_PRINT(charDescription); DEBUG_PRINT(" "); DEBUG_PRINT(charFirmwareVersion); DEBUG_PRINT(" For "); DEBUG_PRINTLN(ESP.getChipModel());
  char cstrCompileDate[32];
  snprintf(cstrCompileDate, sizeof(cstrCompileDate), "%s, %s", __DATE__, __TIME__);
  DEBUG_PRINT("Compiled on "); DEBUG_PRINTLN(cstrCompileDate);
  DEBUG_PRINTLN("\n");
  DEBUG_PRINT("ESP-IDF version: ");
  DEBUG_PRINTLN(esp_get_idf_version());
#endif

  ws2818_leds.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  ws2818_leds.turnOFF();

  ledStatus.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  ledStatus.turnOFF();
  ledStatus.setBrightness(64);//0 to 255

  DEBUG_PRINTLN("Initializing Mutex...");
  xMutex = xSemaphoreCreateMutex();//initialize as soon as possible, but after the serial
  configASSERT(xMutex);
  if (xMutex == NULL) {
    DEBUG_PRINTLN("Error: xMutex could not be created");
    ledStatus.SetColor(0,RGB_PURPLE);
    ledStatus.setBrightness(255);
    ledStatus.turnON();
    while(1) { delay(1000); }
  }

  filesystemMutex = xSemaphoreCreateMutex();//initialize as soon as possible, but after the serial
  configASSERT(filesystemMutex);
  if (filesystemMutex == NULL) {
    DEBUG_PRINTLN("Error: filesystemMutex could not be created");
    ledStatus.SetColor(0,RGB_PURPLE);
    ledStatus.setBrightness(255);
    ledStatus.turnON();
    while(1) { delay(1000); }
  }

#if MATDEBUG
  esp_reset_reason_t reason = esp_reset_reason();
  DEBUG_PRINTF("(For debug) Reset reason: %d ", reason);
  if (reason == ESP_RST_TASK_WDT) DEBUG_PRINT("(ESP_RST_TASK_WDT)");
  else if (reason == ESP_RST_WDT) DEBUG_PRINT("(ESP_RST_WDT)");
  DEBUG_PRINT("\n");
#endif

  //init crash files
  setupCrashInitBoot();

  //adds a default HTTP response header to all outgoing HTTP responses sent by the ESP32â€™s web server
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  DEBUG_PRINTLN("Mounting LittleFS");
  if (!mountLittleFS()){
    ledStatus.SetColor(0,RGB_PURPLE);
    ledStatus.setBrightness(255);
    ledStatus.turnON();
    DEBUG_PRINTLN("Error mounting LittleFS");
    delay(10000);
    //ESP.restart();
  }


#if MATDEBUG
  listDir("/");//check files, chart.umd.min.js
#endif

  shouldRestart=false;
  firstFetch=false;
  timer_previousWifi=0L;
  timer_previousRestart=0L;
  timer_previousWifiRSSI=0L;
  timer_previousWeatherFetch=0L;

  pinMode(PIN_DIMMER, INPUT_PULLUP);
  pinMode(PIN_SWITCH, INPUT);
  analogSetAttenuation(ADC_11db);//less accurate but also les noisy.
  
  //airportlist.emplace_back("SFO");//add an empty one at the end for the user to edit and add new ones
  //airportlist.emplace_back("LAX");//add an empty one at the end for the user to edit and add new ones
  

  //here load user preferences
  DEBUG_PRINT("Read Preferences...");
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    loadPreferences();
    xSemaphoreGive(xMutex);
  };//mutex
  DEBUG_PRINTLN("Done!");

  DEBUG_PRINT("Remove redundancies...");
  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    RemoveRedundantAirports(airportlist);
    xSemaphoreGive(xMutex);
  };//mutex
  DEBUG_PRINTLN("Done!");
  

  DEBUG_PRINT("Setup Wifi...");
  DEBUG_PRINTLN(charHostName);
  char devicename[64];//will contain values
  ledStatus.SetColor(0,RGB_BLUE);
  ledStatus.setBrightness(64);
  ledStatus.turnON();
  if (!SetupWifiConnect(charHostName, devicename, sizeof(devicename))){
    DEBUG_PRINTLN("Wifi not succesful.. :(");
    //notify the user of the html link so they can change some parameters through the web interface.
    //matDisplay.glDraw_Text(devicename,matDisplay.GetMiddleX(),-9,ORIENTATION_0,HORIZ_JUSTIFY_CENTER,VERT_JUSTIFY_BOTTOM,col_white,1);
  }
  else{
    ledStatus.turnOFF();
    DEBUG_PRINT("Wifi succesful! ["); DEBUG_PRINT(devicename); DEBUG_PRINTLN("]");
    //matDisplay.glDraw_Text(devicename,matDisplay.GetMiddleX(),-9,ORIENTATION_0,HORIZ_JUSTIFY_CENTER,VERT_JUSTIFY_BOTTOM,col_white,1);
  }

  DEBUG_PRINT("Init Timezone and time server...");
  InitTimeRTC();
  DEBUG_PRINTLN("Done");
  checkTimezone();

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    snprintf(charDeviceName, sizeof(charDeviceName), "%s", devicename);
    xSemaphoreGive(xMutex);
  }

  // Create a task that will run the main logic function
  DEBUG_PRINTLN("|||>->-> Creating Task for CORE-1... ");
  xTaskCreatePinnedToCore(
    MainCore1,          // Function to implement the task 
    "Core1Task",       // Name of the task 
    8192,            // Stack size in words 8192=8kb 16384=16kb 
    NULL,             // Task input parameter 
    2,                // Priority of the task (higher is more important) 
    &MainCode1Task,    // Task handle 
    1);               // Core where the task should run (Core 1) 
  DEBUG_PRINTF("MainCode1Task stack free/requested: %u/8192\n", uxTaskGetStackHighWaterMark(MainCode1Task));//if < 500bytes, increase stack
  

  DEBUG_PRINTLN("|||>->-> Creating Task for CORE-0...");
    xTaskCreatePinnedToCore(
    heapMonitorTask,          // Function to implement the task 
    "HeapGuard",       // Name of the task 
    3072,            // Stack size in words 8192=8kb 16384=16kb; increase to 4096 if 3072 fails 
    NULL,             // Task input parameter 
    0,                // Priority of the task (higher is more important) 
    &HeapCode0Task,           // Task handle 
    0);               // Core where the task should run (Core 0, which is the same as WiFi and that's ok for this) 
  DEBUG_PRINTF("HeapCode0Task stack free/requested: %u/3072\n", uxTaskGetStackHighWaterMark(HeapCode0Task));//if < 500bytes, increase stack

  xTaskCreatePinnedToCore(
    wifiRSSiMonitorTask,          // Function to implement the task 
    "RSSImonitor",       // Name of the task 
    4096,            // Stack size in words 8192=8kb 16384=16kb; increase to 4096 if 3072 fails 
    NULL,             // Task input parameter 
    0,                // Priority of the task (higher is more important) 
    &RSSICode0Task,           // Task handle 
    0);               // Core where the task should run (Core 0, which is the same as WiFi and that's ok for this) 
  DEBUG_PRINTF("RSSICode0Task stack free/requested: %u/4096\n", uxTaskGetStackHighWaterMark(RSSICode0Task));//if < 500bytes, increase stack


  DEBUG_PRINTLN("* * * --> INITIALIZATION COMPLETE <-- * * *");

}//setup

// Simple averaging for stability (dimmer)
/*int smoothRead(int pin) {
  long sum = 0;
  for(int i = 0; i < 20; i++) {
    sum += analogRead(pin);
  }
  return sum / 20;
}*/

//loop of core1, for sensors and display
void MainCore1(void * pvParameters) {
  const unsigned long idleCheckMainCore1 = (15UL * 60UL * 1000UL);//15 minute, was 15UL * 60UL * 1000UL
  time_t last_reset_time = 0;//initial time of zero
  int dim_pin=0;
  int dimmer_value=0;
  int dimmer_old=0;
  int switch_option=0;
  int switch_old=0;
  bool gotFreshWeatherData=false;
  uint8_t flightCode = 0;
  uint8_t colorCode = 0;

  unsigned long timer1_previousDimmer = millis();
  unsigned long timer1_previousResetChecker = millis();

  const unsigned long timer1Dimmer_delay = 100UL;//delay between encoder request, in ms
  const unsigned long timer1ResetChecker_delay = (1UL * 60UL * 1000UL);//1 minute

  ledStatus.SetColor(0,RGB_CYAN);

  for(;;) {
    unsigned long timer1_now = millis();

    if ( (unsigned long)(timer1_now - timer1_previousResetChecker) >= timer1ResetChecker_delay){
      timer1_previousResetChecker = timer1_now;
      //alright let's check if we have to reset
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        bool temp_isResetDaily = isResetDaily;
        uint8_t temp_timezone = i_timeZone;
        uint8_t temp_resetMinute = reset_localMinute;
        uint8_t temp_resetHour = reset_localHour;
        xSemaphoreGive(xMutex);
        time_t elapsedTime=0;
        hasResetTimePassed(elapsedTime, temp_resetHour, temp_resetMinute, temp_timezone);
        //DEBUG_PRINTF("is Reset daily?: %d (%02dH:%02dM)\n", temp_isResetDaily, temp_resetHour, temp_resetMinute);
        if (elapsedTime<0){
          int32_t seconds_until = -elapsedTime;
          int hours = seconds_until / 3600;
          int minutes = (seconds_until % 3600) / 60;
          if (temp_isResetDaily&&(seconds_until<61)){//turn on the led for a minute before reset.
            ledStatus.SetColor(0,RGB_BLUE);
            ledStatus.turnON();
          }
          DEBUG_PRINTF("Time before reset: T minus %ld seconds (%d hour, %ld minutes)\n", seconds_until, hours, minutes);
        } else {//reset should have happened, now is the time if it was not reset yet
          int hours = elapsedTime / 3600;
          int minutes = (elapsedTime % 3600) / 60;
          DEBUG_PRINTF("Ellapsed time since last reset: %ld seconds (%d hour, %d minutes)\n", elapsedTime, hours, minutes);
          if (temp_isResetDaily && elapsedTime < 65){//do it
            DEBUG_PRINTLN("Gonna reset myself up. Tomorrow is a new day. See you soon.");
            
            vTaskDelay(pdMS_TO_TICKS(500));
            ESP.restart();
          }
          else{
            DEBUG_PRINTLN("Skipping reset.");
            ledStatus.turnOFF();
          }
        }
      }//mutex
    }

    if ( (unsigned long)(timer1_now - timer1_previousDimmer) >= timer1Dimmer_delay){
      timer1_previousDimmer = timer1_now;

      dim_pin = analogRead(PIN_DIMMER); dimmer_value = map(dim_pin, 0, 4095, 20, 255); // 0 4095 DIM_MIN, DIM_MAX

      switch_option = digitalRead(PIN_SWITCH);
      if ( switch_old != switch_option){
        switch_old = switch_option;
        DEBUG_PRINTF("Switch: %d\n", switch_option);
      }

      //ws2818_leds.setBrightness(dimmer_value);
    }//if

    


    //fetch weather data
  if ((firstFetch==false)||((unsigned long)(timer1_now - timer_previousWeatherFetch) >= timerWeatherFetch_delay)){
    timer_previousWeatherFetch = timer1_now;
    firstFetch=true;
    DEBUG_PRINTLN("Good time for a fetch ain't it...");
    fetchWeatherData();
    gotFreshWeatherData=true;
    DEBUG_PRINTLN("Done fetching weather data.");
  }//weather fetch delay check


  

  if (gotFreshWeatherData){
    gotFreshWeatherData=false;
    //here we can do something with the fresh data, like update a display or notify other tasks
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      AirportList localCopy = airportlist;
      xSemaphoreGive(xMutex);
      size_t airportCount = localCopy.size();
      //DEBUG_PRINTF("Updating LED map for %d airports...", airportCount);
      //ws2818_leds.turnOFF();
      ws2818_leds.updateLength(airportCount);//update the number of led in the project
      for (size_t i = 0; i < airportCount; i++) {
        flightCode = localCopy[i].category;
        colorCode = GetColorCode(flightCode);
        ws2818_leds.SetColor(i, colorCode);//0-based index for the led

        char charFlightCategory[6];
        GetCharFlightCategory(charFlightCategory,flightCode);
        DEBUG_PRINTF("Updating LED %d for airport %s: category [%s]\n", i, localCopy[i].code, charFlightCategory);
      }//for
      ws2818_leds.turnON();
      //DEBUG_PRINTLN("Done.");
    }
  }



    mainCoreHeartbeat++;// SIGNAL: "Core 1 is alive"
    checkHeapGuardianHealth(idleCheckMainCore1);//function to check if the heapMonitorTask task has frozen, this is like the supervisor to check if the task is asleep

    vTaskDelay(pdMS_TO_TICKS(10));//Always yield to let the system/watchdog breathe
  }//for
}//MainCore1


//the heapMonitorTask is a task to check for memory integrity, etc and reboot if passed a tresholh
void heapMonitorTask(void *pvParameters) {
  const TickType_t xDelayHEAP = pdMS_TO_TICKS(5UL * 60UL * 1000UL);//5 minute, was 5UL * 60UL * 1000UL
  const unsigned long idleCheckHEAP = (15UL * 60UL * 1000UL);//15 minute, was 15UL * 60UL * 1000UL
    for (;;) {
        //check fragmentation
        checkMemoryHealth(86.f);//85 or 88 is recommended, beyond 90 the esp32 can freeze

        //check heap fragmentation
        if (!heap_caps_check_integrity_all(false)) {
            if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
              DEBUG_PRINTLN("!!! HEALTH MONITOR: Corruption Detected !!!");
              //heap_caps_check_integrity_all(true); // Re-run with true to print specific debug info to Serial
              xSemaphoreGive(xMutex);
            }

            // Trigger crash report and reset
            triggerCrashReport(2, "Background Corruption");
        }//failed check

        // Delay for 10 seconds. 
        heapGuardHeartbeat++;
        checkMainCoreHealth(idleCheckHEAP);
        vTaskDelay(xDelayHEAP); // This puts the task into a "Blocked" state, using 0% CPU.
    }//for
}//heapMonitorTask


// Global to track success
static bool lastPingSuccess = false;
static uint32_t lastResponseTimeMS = 0;
void on_ping_success(esp_ping_handle_t hdl, void *args) { 
  uint32_t elapsed_time;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &elapsed_time, sizeof(elapsed_time));// Get the round-trip time for the last packet
  lastResponseTimeMS = elapsed_time;
  lastPingSuccess = true; 
}
void on_ping_timeout(esp_ping_handle_t hdl, void *args) { 
  lastResponseTimeMS = 0;
  lastPingSuccess = false; 
}
bool nativePing(const char* host) {
    lastPingSuccess = false;

    ip_addr_t target_addr;
    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve hostname (convert 8.8.8.8 to IP structure)
    if (getaddrinfo(host, NULL, &hints, &res) != 0) { return false; }
    
    struct in_addr addr4 = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ip_addr_set_ip4_u32(&target_addr, addr4.s_addr);
    freeaddrinfo(res);

    

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.count = 1; 

    esp_ping_callbacks_t callbacks = {
        .cb_args = NULL,
        .on_ping_success = on_ping_success,
        .on_ping_timeout = on_ping_timeout,
        .on_ping_end = NULL
    };

    esp_ping_handle_t ping;
    esp_ping_new_session(&ping_config, &callbacks, &ping);
    esp_ping_start(ping);
    
    // Block the task for 1 second to wait for the result
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    esp_ping_stop(ping);
    esp_ping_delete_session(ping);
    
    return lastPingSuccess;
}//nativePing

void adjustPower(float avgRssi) {
  // We use "Hysteresis" logic here to prevent bouncing between states
  static wifi_power_t currentPower = WIFI_POWER_19_5dBm;
  wifi_power_t nextPower = currentPower;
  const float HYSTERESIS_THRESHOLD = 5.0;

  // Hysteresis constants (the "buffer" zones)
  const float thresholdHigh = -50 + HYSTERESIS_THRESHOLD; // Must be better than -48 to drop power
  const float thresholdLow  = -50 - HYSTERESIS_THRESHOLD; // Must be worse than -52 to raise power

  if (avgRssi > thresholdHigh) {
    nextPower = WIFI_POWER_8_5dBm; 
  } else if (avgRssi < thresholdLow && avgRssi > -72) {
    nextPower = WIFI_POWER_13dBm;
  } else if (avgRssi < -78) { //else if (avgRssi < -78)
    nextPower = WIFI_POWER_19_5dBm;//max power
  }

  if (nextPower != currentPower) {
    currentPower = nextPower;
    DEBUG_PRINT("Avg RSSI: "); DEBUG_PRINT(avgRssi);
    DEBUG_PRINT(" -> Adjusting TX Power...");
    WiFi.setTxPower(currentPower);
    DEBUG_PRINTF("Power changed to %d dBm\n", currentPower);
  }//power
  
}


//the wifiRSSiMonitorTask is a task to check for wifi signal strenght, and adjust the ESP32 power to adequate level (to save power/battery)
void wifiRSSiMonitorTask(void *pvParameters) {
  const TickType_t xDelayRSSI = pdMS_TO_TICKS(1UL * 60UL * 1000UL);//1 minute
  static uint32_t disconnectedMinutes = 0;// a "stuck disconnected" counter
  const int SAMPLE_SIZE = 10;
  int rssiSamples[SAMPLE_SIZE];
  int sampleIndex = 0;
  int pingFailCount = 0;
  const char* host_for_pingCheck = "8.8.8.8"; // Google DNS to check internet
  unsigned long lastCheck = 0;
  int initialRSSI = WiFi.RSSI();

  for(int i = 0; i < SAMPLE_SIZE; i++) rssiSamples[i] = initialRSSI;//initialize the buffer to -70 which means weak

    for (;;) {
        
        if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {//make sure it is not in Access Point mode, and we have connection
          rssiSamples[sampleIndex] = WiFi.RSSI();//check signal strenght
          sampleIndex = (sampleIndex + 1) % SAMPLE_SIZE;

          float sumRSSI = 0;
          float sumTemp = 0;
          for(int i = 0; i < SAMPLE_SIZE; i++) {
            sumRSSI += rssiSamples[i];
          }
          float avgRssi = sumRSSI / SAMPLE_SIZE;//Calculate average

          if (avgRssi < -78) {//low rssi, initiate the ping
            bool pingSuccess = nativePing(host_for_pingCheck);
            if (!pingSuccess) pingFailCount++;
            else pingFailCount = 0;
            if (pingFailCount >= 3) { // EMERGENCY: We have signal but no data flow. Max out power!
              WiFi.setTxPower(WIFI_POWER_19_5dBm);
              DEBUG_PRINTLN("Ping failed 3x! Boosting power to MAX for stability.");
            }
            else {
              if (lastResponseTimeMS > 150) {//good ping but high latency
                Serial.println("High Latency detected. Boosting power for stability.");
                WiFi.setTxPower(WIFI_POWER_17dBm);
              }//if lastResponseTimeMS
            }//good ping, proceed to adjust wifi power
          } else{
            adjustPower(avgRssi);//Adjust based on average
          }
        }//if connected
        else if (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED) {
          disconnectedMinutes++;
          if (disconnectedMinutes >= 3) { // disconnected for 3+ minutes
            disconnectedMinutes = 0;
            WiFi.disconnect();      // reset the driver state cleanly
            //WiFi.reconnect();  // or re-call WiFi.begin(ssid, pwd)
            WiFi.begin();           // re-attempt with cached credentials
          } 
        }//if not connected
        else if (WiFi.getMode() == WIFI_AP){
          pingFailCount = 0;
          disconnectedMinutes = 0;
          WiFi.setTxPower(WIFI_POWER_19_5dBm);//max power since we are in AP mode, or we are disconnected and the reconnect will have better chanve if higher power
          if (WiFi.scanComplete() < 0) {//-2 = WIFI_SCAN_RUNNING, -1= fail; just so that we scan new wifi network and be ready to go for the portal page
            WiFi.scanNetworks(true); // async
          }
        }//if in AP mode
        else{
          pingFailCount = 0;
          disconnectedMinutes = 0;
        }

        vTaskDelay(xDelayRSSI); // This puts the task into a "Blocked" state, using 0% CPU.
    }//for
}//wifiRSSiMonitorTask


void loop() {
  if (WiFi.getMode() & WIFI_AP) {
    dnsServer.processNextRequest();
  }
  unsigned long timer_now = millis();//milliseconds

  //check if needs restart
  if (shouldRestart && ( (unsigned long)(timer_now - timer_previousRestart) >= timerRestart_delay)){
    ESP.restart();
  }//restart delay check

  

}//loop

