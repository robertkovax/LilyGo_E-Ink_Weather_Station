#include "common.h"
#include "owm_credentials.h"


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

      //adjust to local time (default is UTC)
      setenv("TZ", "PST8PDT,M3.2.0/2,M11.1.0/2", 1); tzset();
      time_t t = (time_t)list[r]["dt"].as<long>();
      struct tm tm_info;
      localtime_r(&t, &tm_info);
      char buffer[20];
      strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
      WxForecast[r].Period = String(buffer);
      //Serial.println("forecast period " + WxForecast[r].Period);
    }

    float pressure_trend = WxForecast[2].Pressure - WxForecast[0].Pressure; // slope between now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // quantize to 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();
  }

  return true;
}

//#########################################################################################
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm* now_tm = gmtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  } else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return String(output);
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
  if (Units == "M") {
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

//#########################################################################################
bool obtain_wx_data(WiFiClient& client, const String& requestType) {
  const String units = (Units == "M" ? "metric" : "imperial");

  client.stop(); // close before new request
  HTTPClient http;

  // API v2.5 endpoint (OneCall v3.0 comment kept from original)
  String uri = "/data/2.5/" + requestType + "?lat=" + LAT + "&lon=" + LON +
               "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;

  // Serial.print(weatherServer);
  // Serial.println(uri.c_str());

  if (requestType != "weather") {
    uri += "&cnt=" + String(MaxReadings);
  }

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
double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  //Calculate the approximate phase of the moon
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}
