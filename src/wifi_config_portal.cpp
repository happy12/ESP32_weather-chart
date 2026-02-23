#include "wifi_config_portal.h"


#include <AsyncTCP.h>
#include <WiFiUdp.h>//to handle some disconnect events shit
#include <ESPmDNS.h> //to setup custom name instead of using local IP address, once the wifi is setup
#include <Preferences.h>//manages preference files
#include <LittleFS.h>//LittleFS file system
#include <ArduinoJson.h>
#include <ping/ping_sock.h> // Native ESP-IDF Ping headers
#include <lwip/inet.h>// Native ESP-IDF Ping headers
#include <lwip/netdb.h>// Native ESP-IDF Ping headers
#include <lwip/sockets.h>// Native ESP-IDF Ping headers


#define MAX_SSID_LIST 20


static TaskHandle_t saveTaskHandle = nullptr;

DNSServer dnsServer;
static AsyncWebServer server(80);
Preferences preferences;

WiFiUDP udp;
static bool udpStarted = false;
//static bool mdnsStarted = false;
const uint16_t localPort = 42103;

static EventGroupHandle_t wifiEventGroup = nullptr;
#define WIFI_CONNECTED_BIT BIT0

char mdnsHostname[32]; //will contain the mdns hostname

void setWifiHostname(const char* name) {
    strncpy(mdnsHostname, name, sizeof(mdnsHostname) - 1);
    mdnsHostname[sizeof(mdnsHostname) - 1] = '\0'; // Ensure null termination
}



void initWifiEventGroup() {
    if (!wifiEventGroup) {
        wifiEventGroup = xEventGroupCreate();
    }
};
void initWiFi() {
    static bool registered = false;
    initWifiEventGroup();
    if (!registered) {
        WiFi.onEvent(WiFiEvent);
        registered = true;
    }
};


void startUDP() {
    if (!udpStarted) {
        udp.begin(localPort);
        udpStarted = true;
    }
}
void stopUDP() {
    if (udpStarted) {
        udp.stop();
        udpStarted = false;
    }
}
void startServer() {
    server.begin();
}
void stopServer() {
    server.end();
}

void startMDNS() {
  if (mdnsHostname[0] != '\0') {
    MDNS.end();
    if (MDNS.begin(mdnsHostname)) { // Set the hostname
      MDNS.addService("http", "tcp", 80);//being explicit to start the http
      MDNS.addServiceTxt("http", "tcp", "board", ESP.getChipModel());//add info for the router to know
      MDNS.addServiceTxt("http", "tcp", "version", charFirmwareVersion);//add info for the router to know
      MDNS.addServiceTxt("http", "tcp", "description", charDescription);
      MDNS.addServiceTxt("http", "tcp", "author", charAuthor);
      DEBUG_PRINTF("mDNS responder started: %s.local",mdnsHostname);
    }
  }//mdnsHostname    
}//startMDNS
void stopMDNS() {
    MDNS.end();
}

void triggerTaskStartMDNS(uint32_t delayMS) {
  // If a save task is already scheduled, cancel it
  //if (mdnsStarted) return;//nothing to do, do not start twice
  //note that mdnsStarted is never set to true nor false. Is it uselesss?

    TimerHandle_t t = xTimerCreate(
      "udp_delay",
      pdMS_TO_TICKS(delayMS), // Give the network stack the time to start
      pdFALSE,
      nullptr,
      [](TimerHandle_t self){
        startUDP();
        startMDNS();
        xTimerDelete(self, 0);//delete itself to avoid leaks
      });
    xTimerStart(t, 0);
}//triggerTaskStartMDNS

//handle event such as disconnect
void WiFiEvent(WiFiEvent_t event){
  static bool servicesRunning = false;
  switch(event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      DEBUG_PRINT("WiFi connected, IP: ");
      DEBUG_PRINTLN(WiFi.localIP());
      xEventGroupSetBits(wifiEventGroup, WIFI_CONNECTED_BIT);
      if (!servicesRunning) {
        triggerTaskStartMDNS(500);
        servicesRunning = true;
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        DEBUG_PRINTLN("WiFi IP lost");
        xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
        if (servicesRunning) {
          stopUDP();
          stopMDNS();
          servicesRunning = false;
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      DEBUG_PRINTLN("WiFi lost connection (Arduino event). Reconnecting...");
      xEventGroupClearBits(wifiEventGroup, WIFI_CONNECTED_BIT);
      /*if (servicesRunning) {
        stopUDP();
        stopServer();//never stop a server?
        stopMDNS();
        servicesRunning = false;
      }*/
      break;
    default:
      break;
  }//switch
}//WiFiEvent


void triggerTaskSavePreference(uint32_t delayMS){
  
  // If a save task is already scheduled, cancel it
  if (saveTaskHandle != nullptr) {
    vTaskDelete(saveTaskHandle);
    saveTaskHandle = nullptr;
  }

  // 1. We pass '[]' (no capture). A lambda with no capture CAN be converted to a function pointer.
  // 2. We cast delayMS to (void*) so FreeRTOS can carry it.
  xTaskCreate([](void* arg){
    uint32_t d = (uint32_t)arg;
    vTaskDelay(pdMS_TO_TICKS(d));// Give the network stack 1000ms to clear the transmission buffer

    // CRITICAL: Protect the save process with your Mutex
    DEBUG_PRINTLN("Saving preferences in background...");
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
        savePreferences();
        xSemaphoreGive(xMutex);
        vTaskDelay(pdMS_TO_TICKS(1000));//wait one more second
    } else {
        DEBUG_PRINTLN("Save preferences failed: Mutex timeout.");
    }//else
    saveTaskHandle = nullptr;
    vTaskDelete(NULL); // Task self-terminates to free memory
  }, "deferred_save", 3072, (void*)delayMS, 1, &saveTaskHandle);
}//triggerTaskSavePreference

bool savePreferences() {
  bool ok = true;
  const float err = 0.0001f;
  if ( preferences.begin("settings", false) ){// "false" means Read/Write. FIlename must be 15 characters or fewer
    //some embeded template functions, the idea is to save the writing cycles (only write when data is)
    auto saveBool = [&](const char* key, bool value) -> bool {
      if (preferences.getBool(key, !value) != value) {
        return preferences.putBool(key, value);
      }
      return true;// nothing to do, but still OK
    };

    auto saveUInt8 = [&](const char* key, uint8_t number) -> bool {
      if (preferences.getUChar(key, 0xFF) != number) {
        return (preferences.putUChar(key, number) == sizeof(uint8_t));
      }
      return true;// nothing to do, but still OK
    };

    auto saveFloat = [&](const char* key, float number) -> bool {
      float old = preferences.getFloat(key, NAN);
      if (isnan(old) || fabs(old - number) > err) {
        return (preferences.putFloat(key, number) == sizeof(float));
      }
      return true;// nothing to do, but still OK
    };

    auto saveColor = [&](const char* key, uint16_t color) -> bool {
        uint16_t old = preferences.getUShort(key, color ^ 0xFFFF);
        if (old != color) {
          return (preferences.putUShort(key, color) == sizeof(uint16_t));
        }
        return true;// nothing to do, but still OK
    };

    auto saveString = [&](const char* key, const char* value) -> bool {
      //if (strcmp(preferences.getString(key, "").c_str(), value) != 0) {
      String stored = preferences.getString(key, "");//unfortunately, need to create a string variable
      if (strcmp(stored.c_str(), value) != 0) {
        return (preferences.putString(key, value) == strlen(value));
      }
      return true;// nothing to do, but still OK
    };

    //then go to town

    ok &= saveUInt8("timezone", i_timeZone);
    ok &= saveBool("apiComma", isApiUseComma);
    ok &= saveString("apiUrl", api_url);
    ok &= saveString("apiKeyCat", api_keyFlightCategory);
    ok &= saveString("apiICAOid", api_keyICAOid);
    ok &= saveBool("resetDaily", isResetDaily);
    ok &= saveUInt8("resetHour", reset_localHour);
    ok &= saveUInt8("resetMinute", reset_localMinute);

    size_t dataSize = airportlist.size() * sizeof(Airport);
    uint8_t* buffer = new uint8_t[dataSize];
    for (size_t i = 0; i < airportlist.size(); i++) {
      memcpy(buffer + (i * sizeof(Airport)), &airportlist[i], sizeof(Airport));
    }//for
    size_t written = preferences.putBytes("list", buffer, dataSize);
    delete[] buffer;

    /*ok &= saveBool("isAxisValues", isShowAxisValues);
    ok &= saveUInt8("tempUnit", temperatureUnit);
    ok &= saveFloat("AxisHumidMAX", chartAxisHumidMAX);
    ok &= saveColor("colDew", COLOR16_chartLineDew);*/

    preferences.end();
    return ok;
  }//if  
  return false;
}//savePreferences

bool loadPreferences()
{

  if ( preferences.begin("settings", true) ){// "true" means Read-Only
    size_t len = 0;
    i_timeZone      = preferences.getUChar("timezone", 0);//0=UTC 20=pacific time
    isApiUseComma   = preferences.getBool("apiComma", true);
    isResetDaily   = preferences.getBool("resetDaily", true);
    reset_localHour   = preferences.getUChar("resetHour", 0);
    reset_localMinute = preferences.getUChar("resetMinute", 0);
    len = preferences.getString("apiUrl", api_url, sizeof(api_url));
    if (len == 0) strlcpy(api_url, "https://aviationweather.gov/api/data/metar?format=json&taf=false&ids=", sizeof(api_url));
    len = preferences.getString("apiKeyCat", api_keyFlightCategory, sizeof(api_keyFlightCategory));
    if (len == 0) strlcpy(api_keyFlightCategory, "fltCat", sizeof(api_keyFlightCategory));
    len = preferences.getString("apiICAOid", api_keyICAOid, sizeof(api_keyICAOid));
    if (len == 0) strlcpy(api_keyICAOid, "icaoId", sizeof(api_keyICAOid));

    size_t dataSize = preferences.getBytesLength("list");
    if (dataSize > 0) {
      uint8_t* buffer = new uint8_t[dataSize];
      preferences.getBytes("list", buffer, dataSize);
      size_t numAirports = dataSize / sizeof(Airport);
      airportlist.reserve(numAirports);
      for (size_t i = 0; i < numAirports; i++) {
        Airport a;
        memcpy(&a, buffer + (i * sizeof(Airport)), sizeof(Airport));
        airportlist.push_back(a);
      }//for
      delete[] buffer;
    }//if dataSize > 0

    /*temperatureUnit = preferences.getUChar("tempUnit", 1);//1=celcius, 2=farenheit, 3=kelvin, 4=rankin
    period_sampling = preferences.getUChar("samplingMode", 2);//1=worst, 2=end
    significantDigitHumidity = preferences.getUChar("digitHumid", 0);
    significantDigitTemperature = preferences.getUChar("digitTemp", 0);



    chartAxisHumidMAX = preferences.getFloat("AxisHumidMAX", 75.0f);//default: 75


    COLOR16_chartLineDew    = preferences.getUShort("colDew", 0x7030);//default=purple*/

    preferences.end();
    return true;
  }//if
  return false;
}//loadPreferences


void CaptiveRequestHandler::handleRequest(AsyncWebServerRequest *request) {
    request->send(200, "text/html", contentHTMLportal);
}//handleRequest

void handleSaveWifiCredentials(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {

        char str_ssid[256];
        char str_pwd[256];
        snprintf(str_ssid, sizeof(str_ssid), "%s", request->getParam("ssid", true)->value().c_str());
        snprintf(str_pwd, sizeof(str_pwd), "%s", request->getParam("password", true)->value().c_str());


        DEBUG_PRINT("Received SSID: "); DEBUG_PRINTLN(str_ssid);
        DEBUG_PRINT("Received Password: "); DEBUG_PRINTLN(str_pwd);

        // Open "wifi-config" namespace in read/write mode
        DEBUG_PRINT("Saving credentials to Flash...");
        if (strlen(str_ssid)>0) {

          if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            preferences.begin("wifi-config", false);
            preferences.putBytes("ssid",    str_ssid, strlen(str_ssid) + 1);//should we check that len is more than 0?
            preferences.putBytes("password",str_pwd,  strlen(str_pwd) + 1);
            preferences.end();
            xSemaphoreGive(xMutex);
            vTaskDelay(pdMS_TO_TICKS(1000));//wait one more second
          }//mutex


          DEBUG_PRINTLN("Saved!");
        }
        else{
          DEBUG_PRINTLN("Nothing to save.. empty SSID");
        }        

        request->send(200, "text/html", "Settings saved! ESP32 is restarting to connect...");
        
        // Give the ESP32 a second to send the response before restarting
        //delay(2000);
        vTaskDelay(pdMS_TO_TICKS(2000));
        DEBUG_PRINTLN("Requesting a restart");
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
          shouldRestart = true;
          timer_previousRestart = millis();
          xSemaphoreGive(xMutex);
        }
        
    } else {
        request->send(400, "text/plain", "Missing SSID or Password");
    }
}//handleSave

void Preferences_deleteWifiCredential(const bool isRestart) {

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    preferences.begin("wifi-config", false); // Open namespace in read/write
    preferences.clear();
    preferences.end();
    xSemaphoreGive(xMutex);
    vTaskDelay(pdMS_TO_TICKS(1000));//wait one more second
  };//mutex

  
  DEBUG_PRINTLN("WiFi Config Deleted!");
  vTaskDelay(pdMS_TO_TICKS(1000));
  if (isRestart) {
    DEBUG_PRINTLN("Restarting...bye");
    ESP.restart();
  }
}//Preferences_deleteWifiCredential
template <size_t Nssid, size_t Npwd>
void Preferences_readWifiCredential(char (&str_ssid)[Nssid] , char (&str_pwd)[Npwd]){
  size_t len = 0;
  str_ssid[Nssid - 1] = '\0';//clear the last character as a safety net
  str_pwd[Npwd - 1] = '\0';//clear the last character as a safety net
  DEBUG_PRINT("Reading WiFi credentials from Flash...");

  if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
  preferences.begin("wifi-config", true); // Open in read-only mode
    len = preferences.getBytes("ssid", str_ssid, Nssid-1);
    if (len == 0) str_ssid[0] = '\0';//nothing was found, so must ensure the array is empty
    len = preferences.getBytes("password", str_pwd, Npwd-1);
    if (len == 0) str_pwd[0] = '\0';//nothing was found, so must ensure the array is empty
  preferences.end();
  xSemaphoreGive(xMutex);
  };//mutex

  DEBUG_PRINTLN("Done Reading!");
}//Preferences_readWifiCredential




void Wifi_connect(const char* str_ssid, const char* str_pwd){
  if (((str_ssid != nullptr)&&(str_ssid[0] != '\0'))&&((str_pwd != nullptr)&&(str_pwd[0] != '\0'))) {
    DEBUG_PRINT("Attempting to connect to: [");
    DEBUG_PRINT(str_ssid); DEBUG_PRINTLN("]");
    dnsServer.stop();//stop the dns server, so to stop the access point mode
    WiFi.mode(WIFI_STA);//put in station mode (and attempt normal wifi connection on next line)
    WiFi.setAutoReconnect(true); // This tells the ESP32 to automatically reconnect to the last saved SSID
    WiFi.persistent(false);// false=use preference for credentials; true= tells the ESP32 to automatically reconnect to the last saved SSID
    WiFi.setSleep(false);//this would prevent random drop, should set it to true on battery mode
    WiFi.setHostname(charHostName);//for the router to recognize the device
    WiFi.begin(str_ssid, str_pwd);
    // Wait 10 seconds for connection
    constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000L;//10 seconds
    uint32_t start = millis();
    while ( (WiFi.status() != WL_CONNECTED) && (millis() - start < WIFI_CONNECT_TIMEOUT_MS) ) {
        vTaskDelay(pdMS_TO_TICKS(200));
        DEBUG_PRINT(".");
    }//while
  }//if
}//Wifi_connect

void GetESPid(char *cname, size_t nameLEN){
  uint64_t chipid = ESP.getEfuseMac();
  char buf[13];
  sprintf(buf, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  snprintf(cname, nameLEN, "%s", buf);
}//GetESPid

void SsidScanner(AsyncResponseStream *stream) {
  char strBarRSSI[16];
  int n = WiFi.scanComplete();
  if (n <= 0) { //-2 = WIFI_SCAN_RUNNING, -1: fail, 0 found, just do it again
     n = WiFi.scanNetworks(); 
  }
  if (n <= 0) return;
  if (n > MAX_SSID_LIST) n = MAX_SSID_LIST; // Cap to prevent buffer overflow
  // Create an array of indices [0, 1, 2, ... n-1]
  int indices[MAX_SSID_LIST];
  for (int i = 0; i < n; i++) indices[i] = i;//for
  // Sort the indices based on RSSI (Bubble Sort for simplicity)
  //we have at least 2 ssid at this point
  for (int i = 0; i < n; i++) {
    for (int j = i + 1; j < n; j++) {
      if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
        // Swap indices if the next one has a stronger signal
        int temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
      }//if
    }//for
  }//for
  // Build the HTML using the sorted indices
  for (int i = 0; i < n; ++i) {
    int id = indices[i]; // Get the index of the i-th strongest network
    dbm_to_SignalBars(strBarRSSI,WiFi.RSSI(id));
    stream->printf("<option value='%s'>%s (%s)%s</option>", 
            WiFi.SSID(id).c_str(), WiFi.SSID(id).c_str(), strBarRSSI,
            (WiFi.encryptionType(id) == WIFI_AUTH_OPEN ? "" : " 🔒") );
  }//for
  WiFi.scanDelete(); // Free memory
  return;
}//SsidScanner

void StartWifiCaptivePortal(const char *cstr_basic, char* hostname, size_t hostnameLEN) {
  char str_espID[13];
  DEBUG_PRINT("Getting ESP ID...");
  GetESPid(str_espID, sizeof(str_espID));
  DEBUG_PRINTLN(str_espID);

  char str_withID[32];
  snprintf(str_withID, sizeof(str_withID), "%s_%s", cstr_basic, str_espID);

  WiFi.mode(WIFI_AP);//access point mode WIFI_AP (access point) WIFI_AP_STA (hybrid both access point and normal station)
#if ASYNCWEBSERVER_WIFI_SUPPORTED
  if (!WiFi.softAP(str_withID)) {
    DEBUG_PRINTLN("Soft Access Point creation failed.");
    while (1);
  }//if
  WiFi.scanNetworks(true);
  DEBUG_PRINT("Captive Portal: ["); DEBUG_PRINT(str_withID); DEBUG_PRINTLN("]");
  //start dns server only in access point mode
  dnsServer.start(53, "*", WiFi.softAPIP()); //The "Wildcard" DNS Ensure your DNS is actually catching everything
#endif

  server.on("/save", HTTP_POST, handleSaveWifiCredentials);

  //add routes for specific phone so the wifi portal can show up quickly:
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Android / Chrome
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Apple / iOS
  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){ request->redirect("/"); });// Windows
  server.onNotFound([](AsyncWebServerRequest *request) { request->redirect("/"); });// Generic catch-all (to force the captive portal to show up)

  server.on("/scanWifi", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream *stream = request->beginResponseStream("text/html");
    SsidScanner(stream);
    request->send(stream);
  });

  //this handler should be last, it is the captive portal to setup the wifi
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);  // only when requested from AP
  
  
  snprintf(hostname, hostnameLEN, "%s", str_withID);
  //return str_withID;
}//StartWifiCaptivePortal

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    AirportList localCopy;
    bool temp_isApiUseComma = false;
    char temp_api_url[128];
    char temp_api_keyFlightCategory[8];
    char temp_api_keyICAOid[8];
    
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
      // copy shared data while locked
      temp_isApiUseComma = isApiUseComma;
      strlcpy(temp_api_url, api_url, sizeof(temp_api_url));
      strlcpy(temp_api_keyFlightCategory, api_keyFlightCategory, sizeof(temp_api_keyFlightCategory));
      strlcpy(temp_api_keyICAOid, api_keyICAOid, sizeof(temp_api_keyICAOid));
      localCopy = airportlist;
      xSemaphoreGive(xMutex);
    }//mutex
    if (temp_isApiUseComma == false){//use single fetch
      for (auto& airport : localCopy) {
        airport.category = fetchWeatherDataSingle(airport.code, temp_api_url, temp_api_keyFlightCategory);
      }//for
    }
    else{//use bundled fetch
      fetchWeatherDataBundled(localCopy, temp_api_url, temp_api_keyICAOid, temp_api_keyFlightCategory);
    }

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
      airportlist = std::move(localCopy);
      xSemaphoreGive(xMutex);
    }//mutex

  }//wifi connected
}//fetchWeatherData
void printStreamToSerial(HTTPClient &http)
{
  int totalLimit = http.getSize();
  int bytesRead = 0;
  WiFiClient* stream = http.getStreamPtr();
  DEBUG_PRINTF("--- Response Body Start --- (size %d bytes) ---\n", totalLimit);
  unsigned long startTimeout = millis();

  while (bytesRead < totalLimit && (millis() - startTimeout < 3000)) {
        while (stream->available() && bytesRead < totalLimit) {
            char c = stream->read();
            DEBUG_PRINT(c);
            bytesRead++;
            startTimeout = millis(); // Reset timeout as long as data flows
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Give the background WiFi tasks a moment to breathe
    }
  DEBUG_PRINTLN("\n--- Response Body End ---");  
}
uint8_t fetchWeatherDataSingle(const char*code, const char*baseurl, const char*keyFlightCategory)
{
  uint8_t return_category=0;
  char urlBuffer[512];
  snprintf(urlBuffer, sizeof(urlBuffer), "%s%s", baseurl, code);
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    DEBUG_PRINTLN("Failed to allocate WiFiClientSecure");
    return return_category;
  }
  //client->setInsecure();// Tell the client NOT to check the SSL certificate
  client->setCACert(rootCACertificate);
  //client->setCACertBundle(rootcert_crt_bundle_start);//uses predefined cert in NetworkClientSecure.h
  HTTPClient https;
  https.setConnectTimeout(5000);
  https.setTimeout(10000); // Add read timeout
  if (!https.begin(*client, urlBuffer)) {
    DEBUG_PRINTLN("HTTPS begin failed");
    delete client;
    return return_category;
  }
  int httpCode = https.GET();
  //DEBUG_PRINTF("HTTP GET response for %s: %s\n", code, https.errorToString(httpCode).c_str());
  if (httpCode == HTTP_CODE_OK) {
#if MATDEBUG
      //printStreamToSerial(https);
#endif
    JsonDocument *filter = new JsonDocument;
    if (!filter) {
      DEBUG_PRINTLN("Failed to allocate filter");
      https.end();
      delete client;
      return return_category;
    }
    (*filter)[0][keyFlightCategory] = true;

    yield(); 
    vTaskDelay(pdMS_TO_TICKS(10));

    JsonDocument *doc = new JsonDocument;
    if (!doc) {
      DEBUG_PRINTLN("Failed to allocate document");
      delete filter;
      https.end();
      delete client;
      return return_category;
    }
    DeserializationError error = deserializeJson(*doc, https.getStream(), DeserializationOption::Filter(*filter));
    delete filter;

    if (!error) {
      const char* cat = nullptr;
      if (doc->is<JsonArray>()) {
        // Response is array: [{"flightCategory": "VFR"}]
        JsonArray arr = doc->as<JsonArray>();
        if (arr.size() > 0 && arr[0].containsKey(keyFlightCategory)) {
          cat = arr[0][keyFlightCategory];
        }
      } else if (doc->is<JsonObject>()) {
        // Response is object: {"flightCategory": "VFR"}
        JsonObject obj = doc->as<JsonObject>();
        if (obj.containsKey(keyFlightCategory)) {
          cat = obj[keyFlightCategory];
        }
      }

      if (cat) {
        return_category = GetFlightCategory(cat);
        //DEBUG_PRINTF("%s Category [%s]: %s\n", code, keyFlightCategory, cat);
        DEBUG_PRINTF("%s is category [%d] (%s)\n", code, return_category, cat);
      }
      else{
        DEBUG_PRINTF("%s: Key '%s' not found in response\n", code, keyFlightCategory);
      }
    }//no error
    else {
      DEBUG_PRINTF("JSON deserialization failed for %s: %s\n", code, error.c_str());
    }
    delete doc; // Clean up document
  }//http OK
  else{
    DEBUG_PRINTF("HTTP GET failed for %s: %d (%s)\n", code, httpCode, https.errorToString(httpCode).c_str());
  }
  https.end();
  delete client; // Clean up memory
  return return_category;
}//fetchWeatherDataSingle
void fetchWeatherDataBundled(AirportList &list, const char*baseurl, const char*keyICAOid, const char*keyFlightCategory)
{
  size_t airportCount = list.size();
  if (airportCount < 1) return;
  //build big boy url
  char urlBuffer[512]; 
  int pos = snprintf(urlBuffer, sizeof(urlBuffer), "%s", baseurl);
  for (size_t i = 0; i < airportCount; i++) {
    pos += snprintf(urlBuffer + pos, sizeof(urlBuffer) - pos, "%s%s", 
                    list[i].code, (i < airportCount - 1) ? "," : "");
    if (pos >= (int)sizeof(urlBuffer) - 1) break; // Safety check
    list[i].category = 0; //reset category before fetching new data
  }//for

  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    DEBUG_PRINTLN("Failed to allocate WiFiClientSecure");
    return;
  }
  //client->setInsecure();// Tell the client NOT to check the SSL certificate
  client->setCACert(rootCACertificate);
  //client->setCACertBundle(rootcert_crt_bundle_start);//uses predefined cert in NetworkClientSecure.h
  HTTPClient https;
  https.setConnectTimeout(5000);
  https.setTimeout(10000); // Add read timeout
  if (!https.begin(*client, urlBuffer)) {
    DEBUG_PRINTLN("HTTPS begin failed");
    delete client;
    return;
  }
  int httpCode = https.GET();
  if (httpCode == HTTP_CODE_OK) {
#if MATDEBUG
      //printStreamToSerial(https);
#endif
    JsonDocument *filter = new JsonDocument;// Allocate filter on heap to avoid stack overflow with large airport lists
    (*filter)[0][keyICAOid] = true;
    (*filter)[0][keyFlightCategory] = true;

    yield(); 
    vTaskDelay(pdMS_TO_TICKS(10));

    JsonDocument *doc = new JsonDocument;// Allocate main document on heap with specific size
    DeserializationError error = deserializeJson(*doc, https.getStream(), DeserializationOption::Filter(*filter));
    delete filter; // Clean up filter memory

    if (!error) {
      if (doc->is<JsonArray>()) {
        JsonArray array = doc->as<JsonArray>();
        for (JsonObject incoming : array) {
          yield();// Feed watchdog during long loops
          if (!incoming.containsKey(keyICAOid) || !incoming.containsKey(keyFlightCategory)) {
            continue;
          }
          const char* incomingId = incoming[keyICAOid];
          const char* incomingCat = incoming[keyFlightCategory];
          processSingleAirport(incomingId, incomingCat, list);
          
        }//for JsonObject
      } else if (doc->is<JsonObject>()) {
        // Handle single airport object (one result)
        JsonObject obj = doc->as<JsonObject>();
        if (obj.containsKey(keyICAOid) && obj.containsKey(keyFlightCategory)) {
          const char* incomingId = obj[keyICAOid];
          const char* incomingCat = obj[keyFlightCategory];
          processSingleAirport(incomingId, incomingCat, list);
        } else {
          DEBUG_PRINTLN("JsonObject missing required fields");
        }//else
      } else {
        DEBUG_PRINTLN("Unexpected JSON type (neither array nor object)");
      }//else
    }
    else {
      DEBUG_PRINTF("JSON deserialization failed: %s\n", error.c_str());
    }
    delete doc;
  }//http OK
  https.end();
  delete client; // Clean up memory
}//fetchWeatherDataBundled



void processSingleAirport(const char* incomingId, const char* incomingCat, AirportList &list) 
{
  if (!incomingId || !incomingCat) return;
  for (size_t i = 0; i < list.size(); i++) {
    if (strcmp(incomingId, list[i].code) == 0) {
      list[i].category = GetFlightCategory(incomingCat);
      DEBUG_PRINTF("%s is category [%d] (%s)\n", list[i].code, list[i].category, incomingCat);
      break;
    }
  }
}//processSingleAirport



///dashboard main webpage, can be ommitted if the device does not need to show data or take input from/to user
void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", contentHTMLroot);
}//handleRoot

void SetupWifiNormalMode(const char *cstr_basic, char* hostname, size_t hostnameLEN) {
  char ipBuffer[16];
  IPAddress ip = WiFi.localIP();
  snprintf(ipBuffer, sizeof(ipBuffer), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  DEBUG_PRINT("Wifi connection succesful. IP: "); DEBUG_PRINTLN(ipBuffer);
  snprintf(hostname, hostnameLEN, "%s", ipBuffer);

  //set name instead of ip address (can be omitted if we don't care to have a dashboard)
  setWifiHostname(cstr_basic);
  snprintf(hostname, hostnameLEN, "%s.local", cstr_basic);

  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *request){
    DEBUG_PRINTLN("PING HIT");
    request->send(200, "text/plain", "pong");
  });

  
  //the following line is for a dashboard, and can be ommitted if the device does not need to show data or take input from/to user:
  server.on("/fetchAirports", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *stream = request->beginResponseStream("application/json");
      JsonDocument doc;
      AirportList localCopy;
      if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
        // copy shared data while locked
        char deviceName[32];
        strlcpy(deviceName, charDeviceName, sizeof(deviceName));
        char fw[16];
        strlcpy(fw, charFirmwareVersion, sizeof(fw));
        int8_t temp_timeZone = i_timeZone;
        bool temp_isApiUseComma = isApiUseComma;
        bool temp_isResetDaily = isResetDaily;
        uint8_t temp_resetHour = reset_localHour;
        uint8_t temp_resetMinute = reset_localMinute;
        char temp_api_url[128];
        char temp_api_keyFlightCategory[8];
        char temp_api_keyICAOid[8];
        strlcpy(temp_api_url, api_url, sizeof(temp_api_url));
        strlcpy(temp_api_keyFlightCategory, api_keyFlightCategory, sizeof(temp_api_keyFlightCategory));
        strlcpy(temp_api_keyICAOid, api_keyICAOid, sizeof(temp_api_keyICAOid));
        localCopy = airportlist;
        xSemaphoreGive(xMutex);

        stream->print("{");
        stream->printf("\"deviceName\":\"%s\",", deviceName);
        stream->printf("\"fwVersion\":\"%s\",", fw);
        stream->printf("\"timezone\":\"%d\",", temp_timeZone);
        stream->printf("\"isApiUseComma\":%s,", temp_isApiUseComma ? "true" : "false");
        stream->printf("\"isResetDaily\":%s,", temp_isResetDaily ? "true" : "false");
        stream->printf("\"resetHour\":%d,", temp_resetHour);
        stream->printf("\"resetMinute\":%d,", temp_resetMinute);
        stream->printf("\"timezone\":\"%d\",", temp_timeZone);
        stream->printf("\"api_url\":\"%s\",", temp_api_url);
        stream->printf("\"api_keyFlightCategory\":\"%s\",", temp_api_keyFlightCategory);
        stream->printf("\"api_keyICAOid\":\"%s\",", temp_api_keyICAOid);
        stream->print("\"codes\":[");
        for (size_t i = 0; i < localCopy.size(); i++) {
            stream->printf("\"%s\"", localCopy[i].code);
            if (i < localCopy.size() - 1) stream->print(","); 
        }//for
        stream->print("]}");
        request->send(stream);
      }
      else{
        request->send(503, "text/plain", "Busy");
      }
      
    });

  //the following line is for a dashboard, and can be ommitted if the device does not need to show data or take input from/to user:
  server.on("/saveAirports", HTTP_POST, 
            [](AsyncWebServerRequest *request) {},
            NULL, // No upload handler
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {

      // Manage the buffer without using 'String'
      if (index == 0) {
          request->_tempObject = malloc(total + 1); // Allocate once
          if (request->_tempObject == nullptr) {
              request->send(507, "text/plain", "Insufficient Storage");
              return;
          }
      }
      //Safety Check: Don't write if malloc failed
      if (request->_tempObject == nullptr) return;

      // Copy this chunk into buffer
      memcpy((uint8_t*)(request->_tempObject) + index, data, len);
      if (index + len == total) {
        char* fullBody = (char*)request->_tempObject;
        fullBody[total] = '\0'; // Null-terminate for the parser

        int8_t temp_timeZone = 0;
        int8_t temp_resetHour = 0;
        int8_t temp_resetMinute = 0;
        bool temp_isApiUseComma = false;
        bool temp_isResetDaily = false;
        char temp_api_url[128] = {0};
        char temp_api_keyFlightCategory[8] = {0};
        char temp_api_keyICAOid[8] = {0};
        AirportList localCopy;

        bool parseSuccess = false;

        JsonDocument doc; 
        DeserializationError error = deserializeJson(doc, fullBody);
        if (!error) {
          JsonArray codes = doc["codes"];
          if (!codes.isNull()) {
            localCopy.reserve(codes.size());
            for (JsonVariant v : codes) {
                const char* codeStr = v.as<const char*>();// Pull as const char* to avoid creating a temporary String object
                if (codeStr && strlen(codeStr) > 0) {
                    localCopy.emplace_back(codeStr);
                }
            }//for
            RemoveRedundantAirports(localCopy);

            temp_isApiUseComma = doc["apiIsComma"] | false;
            temp_isResetDaily = doc["isResetDaily"] | false;
            temp_timeZone = doc["timezone"] | 0;
            temp_resetHour = doc["resetHour"] | 0;
            temp_resetMinute = doc["resetMinute"] | 0;

            const char* url = doc["apiUrl"] | "";
            strlcpy(temp_api_url, url, sizeof(temp_api_url));

            const char* keyFC = doc["apiKeyFlightCategory"] | "";
            strlcpy(temp_api_keyFlightCategory, keyFC, sizeof(temp_api_keyFlightCategory));

            const char* keyICAO = doc["apiKeyICAOid"] | "";
            strlcpy(temp_api_keyICAOid, keyICAO, sizeof(temp_api_keyICAOid));

            parseSuccess = true;

        }//if
        }//if no error

        free(request->_tempObject);
        request->_tempObject = nullptr;

        if (!parseSuccess) {
          request->send(400, "text/plain", "Invalid JSON or missing codes");
          return;
        }

        DEBUG_PRINT("Data received succesfully. Now store into variables...");
        if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {//increase from 50 to 100 if too many 'Busy' response
          airportlist = std::move(localCopy); // No copying of the list
          isApiUseComma = temp_isApiUseComma;
          isResetDaily = temp_isResetDaily;
          i_timeZone = temp_timeZone;
          reset_localHour = temp_resetHour;
          reset_localMinute = temp_resetMinute;
          strlcpy(api_url, temp_api_url, sizeof(api_url));
          strlcpy(api_keyFlightCategory, temp_api_keyFlightCategory, sizeof(api_keyFlightCategory));
          strlcpy(api_keyICAOid, temp_api_keyICAOid, sizeof(api_keyICAOid));
          xSemaphoreGive(xMutex);
          DEBUG_PRINTLN("Done!");

          DEBUG_PRINT("Refreshing Time Server...");
          InitTimeRTC();
          DEBUG_PRINTLN("Done!");

          triggerTaskSavePreference(1500);
          //ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1500));//Wait for save to complete. No need since save is async already
          request->send(200, "text/plain", "Settings saved");

        }//mutex
        else {
          DEBUG_PRINTLN("Failed to acquire mutex");
          request->send(503, "text/plain", "Server busy, try again");
        }//else
      }//if

      });

  // Handle disconnects/leaks
server.onNotFound([](AsyncWebServerRequest *request) {
    // If the request is destroyed but _tempObject isn't null, free it
    if (request->_tempObject != nullptr) {
        free(request->_tempObject);
        request->_tempObject = nullptr;
    }
});
  

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request){//method just to test the time function and display time on the webpage
        struct tm timeinfo;
        if(!getLocalTime(&timeinfo, 10)) {// Use a 0ms or very small (10ms) timeout. If the time is set, it returns instantly. If it's NOT set, it won't hang your server for 10 seconds.
            request->send(200, "text/plain", "Time Syncing Error...");
            return;
        }
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%H:%M:%S %Z", &timeinfo);
        AsyncResponseStream *stream = request->beginResponseStream("text/html"); 
        stream->printf(buffer);
        request->send(stream);
      });

  
  server.on("/api/check-crash", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", "{\"crashed\":false}");// Just send a "no crash" response for now
    return;

    AsyncResponseStream *stream = request->beginResponseStream("application/json");
    stream->print("{");// 1. Start the main JSON Object

    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (LittleFS.exists(filename_crash_report)) {
        File file = LittleFS.open(filename_crash_report, "r");
        if (file && file.size() >= sizeof(CrashData)) {
          file.seek(file.size() - sizeof(CrashData));// Seek to the very last record in the file
          CrashData lastRecord;
          file.read((uint8_t*)&lastRecord, sizeof(CrashData));
          file.close();

          stream->print("\"crashed\":true,");
          stream->printf("\"reason\":%d,", lastRecord.reasonCode);
          stream->printf("\"frag\":%.1f,", lastRecord.fragmentation);
          stream->printf("\"time\":%u,", lastRecord.uptimeMillis);
          stream->printf("\"message\":\"%s\",", lastRecord.message);
          stream->printf("\"taskName\":\"%s\"", lastRecord.taskName);//no coma for the last
        }//if
        else{
          stream->print("\"crashed\":false");//no coma for the last
        }
      }//if file
      else {
        stream->print("\"crashed\":false");//no coma for the last
      }//else file
      xSemaphoreGive(xMutex);
    }//mutex
    else {
      stream->print("\"error\":\"Mutex Timeout\"");//no coma for the last
    }
    stream->print("}");
    request->send(stream);
  });///api/check-crash

  server.on("/", HTTP_GET, handleRoot);//the main page

  server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });
  
}//SetupWifiNormalMode

//return true means it is connected to the internet, return false means it created the captive portal
//the hostname argument will contains either the captive portal name, or the ip address if connected
bool SetupWifiConnect(const char *cstr_basic, char* hostname, size_t hostnameLEN){
  static bool serverStarted = false;
  char strSSID[256]="";
  char strPWD[256]="";
  bool isConnected=false;

  initWiFi();

  //Preferences_deleteWifiCredential(false);//delete credential for test

  Preferences_readWifiCredential(strSSID,strPWD);
  Wifi_connect(strSSID, strPWD);

  EventBits_t bits = xEventGroupWaitBits(
      wifiEventGroup,
      WIFI_CONNECTED_BIT,
      pdFALSE,
      pdTRUE,
      pdMS_TO_TICKS(10000)
  );

  if (WiFi.status() == WL_CONNECTED){
    SetupWifiNormalMode(cstr_basic, hostname, hostnameLEN);
    DEBUG_PRINT("Begin server: "); DEBUG_PRINTLN(hostname);
    isConnected=true;
  }
  else{
    DEBUG_PRINTLN("Connection to Wifi failed. Starting Captive Portal...");
    StartWifiCaptivePortal(cstr_basic,hostname,hostnameLEN);
    isConnected=false;
  }
  if (!serverStarted) {
    server.begin();//this should be called only once
    serverStarted = true;
  }
  return isConnected;
}//SetupWifiConnect
