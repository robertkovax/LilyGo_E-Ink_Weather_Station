#include "common.h"
#include "setup_server.h"


//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[1].Rainfall   = mm_to_inches(WxForecast[1].Rainfall);
  WxForecast[1].Snowfall   = mm_to_inches(WxForecast[1].Snowfall);
}

//#########################################################################################
// Problems with structuring JSON decodes, see: https://arduinojson.org/assistant/
bool DecodeWeather(WiFiClient& json, const String& type) {
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }

  JsonObject root = doc.as<JsonObject>();

  if (type == "weather") {
    WxConditions[0].lon         = root["coord"]["lon"].as<float>();
    WxConditions[0].lat         = root["coord"]["lat"].as<float>();
    WxConditions[0].Main0       = root["weather"][0]["main"].as<const char*>();
    WxConditions[0].Forecast0   = root["weather"][0]["description"].as<const char*>();
    WxConditions[0].Forecast1   = root["weather"][1]["description"].as<const char*>();
    WxConditions[0].Forecast2   = root["weather"][2]["description"].as<const char*>();
    WxConditions[0].Icon        = root["weather"][0]["icon"].as<const char*>();
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();
    WxConditions[0].Pressure    = root["main"]["pressure"].as<float>();
    WxConditions[0].Humidity    = root["main"]["humidity"].as<float>();
    WxConditions[0].Low         = root["main"]["temp_min"].as<float>();
    WxConditions[0].High        = root["main"]["temp_max"].as<float>();
    WxConditions[0].Windspeed   = root["wind"]["speed"].as<float>();
    WxConditions[0].Winddir     = root["wind"]["deg"].as<float>();
    WxConditions[0].Cloudcover  = root["clouds"]["all"].as<int>();
    WxConditions[0].Visibility  = root["visibility"].as<int>();
    WxConditions[0].Rainfall    = root["rain"]["1h"].as<float>();
    WxConditions[0].Snowfall    = root["snow"]["1h"].as<float>();
    WxConditions[0].Country     = root["sys"]["country"].as<const char*>();
    WxConditions[0].Sunrise     = root["sys"]["sunrise"].as<int>();
    WxConditions[0].Sunset      = root["sys"]["sunset"].as<int>();
    WxConditions[0].Timezone    = root["timezone"].as<int>();
  }

  if (type == "forecast") {
    JsonArray list = root["list"];
    for (byte r = 0; r < MaxReadings; r++) {
      WxForecast[r].Dt          = list[r]["dt"].as<int>();
      WxForecast[r].Temperature = list[r]["main"]["temp"].as<float>();
      WxForecast[r].Low         = list[r]["main"]["temp_min"].as<float>();
      WxForecast[r].High        = list[r]["main"]["temp_max"].as<float>();
      WxForecast[r].Pressure    = list[r]["main"]["pressure"].as<float>();
      WxForecast[r].Humidity    = list[r]["main"]["humidity"].as<float>();
      WxForecast[r].Forecast0   = list[r]["weather"][0]["main"].as<const char*>();
      WxForecast[r].Icon        = list[r]["weather"][0]["icon"].as<const char*>();
      WxForecast[r].Description = list[r]["weather"][0]["description"].as<const char*>();
      WxForecast[r].Cloudcover  = list[r]["clouds"]["all"].as<int>();
      WxForecast[r].Windspeed   = list[r]["wind"]["speed"].as<float>();
      WxForecast[r].Winddir     = list[r]["wind"]["deg"].as<float>();
      WxForecast[r].Rainfall    = list[r]["rain"]["3h"].as<float>();
      WxForecast[r].Snowfall    = list[r]["snow"]["3h"].as<float>();
      WxForecast[r].Pop         = list[r]["pop"].as<float>();

      long tz_offset = root["city"]["timezone"].as<long>();  // offset in seconds from UTC
      time_t t = (time_t)list[r]["dt"].as<long>() + tz_offset;
      struct tm tm_info;
      gmtime_r(&t, &tm_info);
      char buffer[20];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
      WxForecast[r].Period = String(buffer);
    }

    float pressure_trend = WxForecast[2].Pressure - WxForecast[0].Pressure; // slope between now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // quantize to 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (String(Units) == "I") Convert_Readings_to_Imperial();
  }

  return true;
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_hour * 3600, daylightOffset_hour * 3600, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
//convert Unicx time to UTC time
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm* now_tm = gmtime(&tm);
  char output[40];
  if (String(Units) == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  } else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return String(output);
}
// #######################################################################################
String GetForecastDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40], FDay[40];
  if (String(Units) == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%p %m/%d/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  return weekday_D[String(FDay).toInt()];
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char   time_output[30], day_output[30], dd_mm_output[10], update_time[30];
  while (!getLocalTime(&timeinfo, 3000)) { // Wait for 5-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin  = timeinfo.tm_min;
  CurrentSec  = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  Serial.print("Time set: ");
  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");      // Displays: Saturday, June 24 2017 14:05:49
  //Serial.println(&timeinfo, "%02d.%02m"); // Displays: 24.06
  if (String(Units) == "M") {
    sprintf(day_output, "%s, %02u. %s %02u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) % 100); // day_output >> So., 23. Juni 19 <<
    strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);  // Creates: '@ 14:05', 24h, no am or pm or seconds.   and change from 30 to 8 <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
    strftime(dd_mm_output, sizeof(dd_mm_output), "%d.%m", &timeinfo);  // Creates '31.05'
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%y", &timeinfo); // Creates  'Sat May-31-2019'
    strftime(dd_mm_output, sizeof(dd_mm_output), "%d.%m", &timeinfo);  // Creates '31.05'
    strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);        // Creates: '@ 02:05' - 24h, no seconds or am/pm
    sprintf(time_output, "%s", update_time);
  }
  date_str = day_output;
  date_dd_mm_str = dd_mm_output; //only for popup check
  time_str = time_output;
  return true;
}
// ########################################################################################
int tomorrowStartIndex(int preferHourStart = 6){
  String currentDate = WxForecast[0].Period.substring(0, 10);

  for (int i = 0; i < MaxReadings; i++) {
    String date = WxForecast[i].Period.substring(0, 10);   // YYYY-MM-DD
    int hour    = WxForecast[i].Period.substring(11, 13).toInt();
    if (date != currentDate && hour >= preferHourStart) {
        // First entry of the *next day* morning
        return i;
    }
  }
  return -1;
}
//#########################################################################################
bool obtain_wx_data(WiFiClient& client, const String& requestType) {
  const String units = (String(Units) == "M" ? "metric" : "imperial");
  client.stop(); // close before new request
  HTTPClient http;

  // API v2.5 endpoint (OneCall v3.0 comment kept from original)
  String uri = "/data/2.5/" + requestType + "?lat=" + LAT + "&lon=" + LON +
               "&appid=" + apikey + "&mode=json&units=" + units + "&lang=EN";

  if (requestType != "weather") {
    uri += "&cnt=" + String(MaxReadings);
  }
  // Serial.print(weatherServer);
  // Serial.println(uri.c_str());

  http.begin(client, weatherServer, 80, uri);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    bool ok = DecodeWeather(http.getStream(), requestType);
    client.stop();
    http.end();
    return ok;
  } else {
    Serial.printf("connection failed, error: %s",
                  http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
}
//#########################################################################################
float mm_to_inches(float value_mm)
{
  return 0.0393701 * value_mm;
}
//#########################################################################################
float hPa_to_inHg(float value_hPa)
{
  return 0.02953 * value_hPa;
}
//#########################################################################################
float SumOfPrecip(float DataArray[], int readings) {
  float sum = 0;
  for (int i = 0; i < readings; i++) {
    sum += DataArray[i];
  }
  return sum;
}
//#########################################################################################
String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  else return text;
}
//#########################################################################################
int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
}
//#########################################################################################
UtcDateTime getUtcDateTime() {
  time_t now = time(NULL);
  struct tm * now_utc = gmtime(&now);

  UtcDateTime dt;
  dt.year   = now_utc->tm_year + 1900;
  dt.month  = now_utc->tm_mon + 1;
  dt.day    = now_utc->tm_mday;
  dt.hour   = now_utc->tm_hour;
  dt.minute = now_utc->tm_min;
  dt.second = now_utc->tm_sec;
  return dt;
}
//#########################################################################################

double NormalizedMoonPhase(int d, int m, int y, int h) {
  int jNoon = JulianDate(d, m, y);   // JDN at 12:00 UT

  // Fraction of a day relative to 12:00 UT
  double fracDay = (h - 12) / 24.0;
  double J = jNoon + fracDay;

  // Normalize to [0,1)
  double phase = fmod((J + C_NEW) / P, 1.0);
  if (phase < 0) phase += 1.0;

  return phase;
}
//#########################################################################################
int MoonIllumination(int d, int m, int y, int h){
  double phase = NormalizedMoonPhase(d, m, y, h);
  double illum = (1 - cos(2 * M_PI * phase)) / 2;   // 0.0..1.0
  return (int)round(illum * 100);
}
//#########################################################################################
String MoonPhase(int d, int m, int y, int h) {
  // Get fractional phase [0.0 .. 1.0)
  double phase = NormalizedMoonPhase(d, m, y, h);

  // Map to 0–7 (8 buckets)
  int b = (int)(phase * 8 + 0.5) & 7;

  if (b == 0) return TXT_MOON_NEW;
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;
  if (b == 2) return TXT_MOON_FIRST_QUARTER;
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;
  if (b == 4) return TXT_MOON_FULL;
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;
  if (b == 6) return TXT_MOON_THIRD_QUARTER;
  if (b == 7) return TXT_MOON_WANING_CRESCENT;
  return "";
}

//#########################################################################################
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
