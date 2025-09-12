//these variables will be loaded into the EEPROM at the first boot after programming (in "wifi_setup.cpp"). 
//When EEPROM is already populated (EEPROM_MARKER_VALUE is set to 0xA5), then only the EEPROM values will be used.
//Values can be set or erased in the web interface.
//Full erase of EEPROM is done by: http://192.168.4.1/erase_eeprom

#include "owm_credentials.h"

char ssid[64]     = "";
char password[64] = "";

String apikey       = ""; // Use your own API key by signing up for a free developer account at https://openweathermap.org/

//http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40
//http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1
//https://api.openweathermap.org/data/3.0/onecall?lat={lat}&lon={lon}&appid={API key}
const char weatherServer[] = "api.openweathermap.org"; 

// Set your location according to OWM locations
String LAT              = "";
String LON              = "";
String Location_name    = "";
String Language         = "EN";
String Hemisphere       = "north";
String Units            = "M"; //M = metric, else imperial

// Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
const char* Timezone    = "CET-1CEST,M3.5.0,M10.5.0/3"; //central EU
//choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
// then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
// EU "0.europe.pool.ntp.org"
// US "0.north-america.pool.ntp.org"
// See: https://www.ntppool.org/en/
const char* ntpServer   = "pool.ntp.org";
long gmtOffset_h      = 1; //  in hous
int daylightOffset_h  = 1; // in hours