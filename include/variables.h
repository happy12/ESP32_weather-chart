#ifndef VARIABLES_h
#define VARIABLES_h

#pragma once

#include <Arduino.h>
#include <cstring>
#include <unordered_set>
#include <algorithm> // for std::remove_if
#include <inttypes.h>
#include <rom/rtc.h>
#include <LittleFS.h>//LittleFS file system
#include <time.h>
#include <esp_sntp.h> //some time status function utilities
#include <UtilTimeZone.h>
#include <UtilVector.h>
#include <UtilNeoPixel.h>

#define MATDEBUG 1 // Change to 0 to disable all Serial output

#if MATDEBUG
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
  #define DEBUG_BEGIN(baud) Serial.begin(baud)
  #define DEBUG_FLUSH Serial.flush()
  #define DEBUG_VERBOSE //do nothing, meaning it will allow the normal debug verbose
  #define DEBUG_SERIALWAIT while(!Serial) { delay(10); } // Wait for Serial to be ready (for native USB devices)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...) // Becomes nothing when DEBUG is 0
  #define DEBUG_BEGIN(baud)
  #define DEBUG_FLUSH
   //silence system logs:
  #define DEBUG_VERBOSE esp_log_level_set("*", ESP_LOG_NONE)
  #define DEBUG_SERIALWAIT
#endif

extern SemaphoreHandle_t xMutex;
extern SemaphoreHandle_t filesystemMutex;

extern const char charFirmwareVersion[16];//this saves RAM instead of const String
extern const char* charDescription;//used for the wifi router
extern const char* charAuthor;//used for the wifi router
extern const char* charHostName; //name for captive portal access point 
extern char charDeviceName[32]; //will contain the hostname plus the EID appended to it for unique identifier

extern unsigned long timer_previousRestart;
extern unsigned long timer_previousWifi;
extern unsigned long timer_previousWifiRSSI;
extern unsigned long timer_previousWeatherFetch;

extern bool shouldRestart;
extern bool firstFetch;

#define LED_STATUS 23
#define LED_CONTROL 25
#define PIN_SWITCH 16
#define PIN_DIMMER 34

enum {
	SECTIONAL_VFR=100, /// VFR=Green color
	SECTIONAL_MVFR=101, /// MVFR=Blue color
	SECTIONAL_IFR=102, /// IFR=Red color
	SECTIONAL_LIFR=103, /// LIFR=Purple color; less than a miles vis
	SECTIONAL_TFR=104, /// TFR=Orange/Yellow color
	AIRSPACE_A=90,
	AIRSPACE_B=91,
	AIRSPACE_C=92,
	AIRSPACE_D=93,
	AIRSPACE_E=94,
	AIRSPACE_G=95,
	SUN_DAY=200, //for tracking if current time is day
	SUN_NIGHT=210, //for tracking if current time is night
};

struct Airport{
    char code[6];
    uint8_t category;
    Airport(const char* c = "", const uint8_t cat=0) : code{}, category(cat) {
        std::memset(code, 0, sizeof(code));
        if (c) {
            std::strncpy(code, c, sizeof(code) - 1);
        }
    }
};
using AirportList = std::vector<Airport>;

extern AirportList airportlist;
extern uint8_t i_timeZone;
extern char api_url[128];
extern char api_keyFlightCategory[8];
extern char api_keyICAOid[8];
extern bool isApiUseComma;
extern bool isResetDaily;
extern uint8_t reset_localHour;
extern uint8_t reset_localMinute;

void RemoveRedundantAirports(AirportList &list);

uint8_t GetFlightCategory(const char *cstCat);
uint8_t GetColorCode(const uint8_t category);
void hasResetTimePassed(time_t &elapsed, uint8_t resetHour, uint8_t resetMinute, uint8_t tzoneIndex);

template <size_t N>
void GetCharFlightCategory(char (&buffer)[N], const uint8_t category)
{
  switch(category){
    case SECTIONAL_VFR: //VFR=Green color
      snprintf(buffer, N, "VFR");
      break;
    case SECTIONAL_MVFR: //MVFR=Blue color
      snprintf(buffer, N, "MVFR");
      break;
    case SECTIONAL_IFR: //IFR=Red color
      snprintf(buffer, N, "IFR");
      break;
    case SECTIONAL_LIFR: //LIFR=Purple color; less than a miles vis
      snprintf(buffer, N, "LIFR");
      break;
    case SECTIONAL_TFR: //TFR=Orange/Yellow color
      snprintf(buffer, N, "TFR");
      break;
    default:
      snprintf(buffer, N, "UNK");
      break;
  }//switch
}


inline void capInt8(uint8_t &val, const uint8_t imin, const uint8_t imax){
  if (val < imin) val = imin;
  if (val > imax) val = imax;
};
inline void capInt(int &val, const int imin, const int imax){
  if (val < imin) val = imin;
  if (val > imax) val = imax;
};
inline void capFloat(float &val, const float fmin, const float fmax){
  if (val < fmin) val = fmin;
  if (val > fmax) val = fmax;
};
inline void capDouble(double &val, const double fmin, const double fmax){
  if (val < fmin) val = fmin;
  if (val > fmax) val = fmax;
};


// %H	Hour (00-23)
// %M	Minute (00-59)
// %S	Second (00-59)
// %d	Day of month
// %y	Year (last two digits)
// %p	AM or PM
template <size_t N>
void formatTime(char (&formattedtime)[N], time_t rawTime, const char *format) //"%Y-%m-%d %H:%M:%S" = Format: "YYYY-MM-DD HH:MM:SS" 
{
    struct tm ts;
    char buf[28];
    ts = *localtime(&rawTime);
    strftime(buf, sizeof(buf), format, &ts);
    snprintf(formattedtime, N, "%s", buf);//this is the output char
}

template <size_t N>
void GetCharTimezone(char (&buffer)[N], const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  snprintf(buffer, N, "UTC0");
  if ((index >= 0)&&(index < len)){
    snprintf(buffer, N, "%s", TZone[index].tzString);
  }
}//GetCharTimezone
template <size_t N>
void GetCharTimeServer(char (&buffer)[N], const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  snprintf(buffer, N, "time.google.com");
  if ((index >= 0)&&(index < len)){
    snprintf(buffer, N, "%s", TZone[index].ntpServer);
  }
}//GetCharTimeServer
inline int8_t GetOffsetTimezone(const uint8_t index){
  size_t len = sizeof(TZone) / sizeof(Timezone_t);
  if ((index >= 0)&&(index < len)){
    return TZone[index].tzoff;
  }
  return 0;//default UTC
}//GetOffsetTimezone

void InitTimeRTC();

void checkSyncStatus();
void checkTimezone();
bool checkIsTimeValid();

#endif