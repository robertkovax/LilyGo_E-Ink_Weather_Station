#pragma once
#ifndef COMMON_H_
#define COMMON_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "lang.h"
#include "forecast_record.h"

// Synodic month (days)
static constexpr double P = 29.53059;
// Calibrated so phase = 0.0 at New Moon: 2025-09-21 19:54 UTC
static constexpr double C_NEW = 19.9190733334;

struct UtcDateTime
{
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

// ==== Externs (declared in main)
extern Forecast_record_type WxForecast[];   // from forecast_record.h
extern Forecast_record_type WxConditions[]; // from forecast_record.h
extern uint8_t MaxReadings;
extern int CurrentHour, CurrentMin, CurrentSec;
extern String time_str, date_str, date_dd_mm_str;

// ==== Public API ====
void Convert_Readings_to_Imperial();
bool DecodeWeather(WiFiClient &json, const String &type);
String ConvertUnixTime(int unix_time);
bool obtain_wx_data(WiFiClient &client, const String &requestType);
boolean SetupTime();
boolean UpdateLocalTime();
String GetForecastDay(int unix_time);
int tomorrowStartIndex(int preferHourStart);

float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
UtcDateTime getUtcDateTime();
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);
double NormalizedMoonPhase(int d, int m, int y, int h = 12);
String MoonPhase(int d, int m, int y, int h = 12);
int MoonIllumination(int d, int m, int y, int h = 12);
String WindDegToDirection(float winddirection);

#endif // COMMON_H_
