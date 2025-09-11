#pragma once
#ifndef LANG_H
#define LANG_H

#define FONT(x) x##_tf

// Temperature / Humidity / Forecast
#define TXT_FORECAST_VALUES   "3-Day Forecast Values"
#define TXT_CONDITIONS        "Conditions"
#define TXT_DAYS              "(Days)"
#define TXT_TEMPERATURES      "Temperature"
#define TXT_TEMPERATURE_C     "Temperature (*C)"
#define TXT_TEMPERATURE_F     "Temperature (*F)"
#define TXT_HUMIDITY_PERCENT  "Humidity (%)"

// Pressure
#define TXT_PRESSURE          "Pressure"
#define TXT_PRESSURE_HPA      "Pressure (hPa)"
#define TXT_PRESSURE_IN       "Pressure (in)"
#define TXT_PRESSURE_STEADY   "Steady"
#define TXT_PRESSURE_RISING   "Rising"
#define TXT_PRESSURE_FALLING  "Falling"

// Rain/Snow
#define TXT_RAINFALL_MM       "Rainfall (mm)"
#define TXT_RAINFALL_IN       "Rainfall (in)"
#define TXT_SNOWFALL_MM       "Snowfall (mm)"
#define TXT_SNOWFALL_IN       "Snowfall (in)"
#define TXT_PRECIPITATION_SOON "Prec."

// Sun
#define TXT_SUNRISE           "Sunrise"
#define TXT_SUNSET            "Sunset"

// Moon
#define TXT_MOON_NEW              "New Moon"
#define TXT_MOON_WAXING_CRESCENT  "Waxing Crescent"
#define TXT_MOON_FIRST_QUARTER    "First Quarter"
#define TXT_MOON_WAXING_GIBBOUS   "Waxing Gibbous"
#define TXT_MOON_FULL             "Full Moon"
#define TXT_MOON_WANING_GIBBOUS   "Waning Gibbous"
#define TXT_MOON_THIRD_QUARTER    "Third Quarter"
#define TXT_MOON_WANING_CRESCENT  "Waning Crescent"

// Power / WiFi
#define TXT_POWER             "Power"
#define TXT_WIFI              "WiFi"
#define TXT_UPDATED           "Updated:"

// Wind
#define TXT_WIND_SPEED_DIRECTION "Wind Speed/Direction"
#define TXT_N    "N"
#define TXT_NNE  "NNE"
#define TXT_NE   "NE"
#define TXT_ENE  "ENE"
#define TXT_E    "E"
#define TXT_ESE  "ESE"
#define TXT_SE   "SE"
#define TXT_SSE  "SSE"
#define TXT_S    "S"
#define TXT_SSW  "SSW"
#define TXT_SW   "SW"
#define TXT_WSW  "WSW"
#define TXT_W    "W"
#define TXT_WNW  "WNW"
#define TXT_NW   "NW"
#define TXT_NNW  "NNW"

const char* const weekday_D[7] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };
const char* const month_M[12]  = { "Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec" };

#endif // LANG_H
