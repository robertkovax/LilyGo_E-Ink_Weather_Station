#include "owm_credentials.h"


// char ssid[64] = "HP_Deskjet";
// char password[64] = "11112222";
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
String City             = "";
String Country          = "";
String Language         = "EN";
String Hemisphere       = "north";
String Units            = "M";

// Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
const char* Timezone    = "CET-1CEST,M3.5.0,M10.5.0/3"; 
//choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
// then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
// EU "0.europe.pool.ntp.org"
// US "0.north-america.pool.ntp.org"
// See: https://www.ntppool.org/en/
const char* ntpServer   = "pool.ntp.org";
long gmtOffset_sec      = 3600; // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int daylightOffset_sec  = 3600; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset