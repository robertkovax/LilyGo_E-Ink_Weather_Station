/* ESP Weather Display using an EPD 2.13" Display, obtains data from Open Weather Map, decodes it and then displays it.
   ####################################################################################################################################
   Original software, ideas and concepts Copyright (c) David Bird 2018.
   All rights reserved. See http://www.dsbird.org.uk

   License summary:
   - Private and non-commercial use only.
   - Redistribution requires attribution to David Bird.
   - Commercial use requires explicit permission.
   - Provided "AS IS", without warranty of any kind.

   Full license text is included in the original repository:
   https://github.com/G6EJD/ESP32-e-Paper-Weather-Display
*/
/*
  Modified by:
  Robert Kovacs, 2025 (info@robertkovacs.de | https://robertkovax.com/)

  --- New functionality ---
  + Cycle through "current day", "next day" and "4-day" forecast view on button press
  + Long-press shows 4-day forecast immediately
  + WiFi webserver for full setup (SSID: "weather_station_wifi")
  + Custom popup messages via /popups endpoint
  + Low-battery warning with deep sleep protection
  + Error messages for WiFi, battery, weather server, or time sync
  + Improved weather icons
  + Improved moon phase calculation
  + Fast partial screen refreshes
  + Adapted for Lilygo TTGO T5 V2.3_2.13 e-paper display

  TO DO:
  Update weather fetching via One Call API 3.0
*/

#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>        // Built-in
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "esp_wifi.h"
#include "time.h"
#include <SPI.h>
#define ENABLE_GxEPD2_display 0
// #include "forecast_record.h"
// #include "owm_credentials.h"
#include "display.h"
#include "setup_server.h"
#include "common.h"

const uint8_t fw_version_major = 2;
const uint8_t fw_version_minor = 2;

// ################ DISPLAY Lilygo TTGO T5 V2.3_2.13 #######################################
// https://github.com/lewisxhe/TTGO-EPaper-Series#board-pins
#define SCREEN_WIDTH 250
#define SCREEN_HEIGHT 122

static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_CS = 5;
static const uint8_t EPD_RST = 16;
static const uint8_t EPD_DC = 17;   // Data/Command
static const uint8_t EPD_SCK = 18;  // CLK on pinout?
static const uint8_t EPD_MISO = -1; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));
// GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));
// GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));

//  #WeAct 2.13 screen module, you need to change GxEPD2_213_B73 to GxEPD2_213_B74
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts; // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts: // u8g2_font_helvB08_tf// u8g2_font_helvB10_tf// u8g2_font_helvB12_tf// u8g2_font_helvB14_tf// u8g2_font_helvB24_tf

TaskHandle_t dispInitTaskHandle = nullptr;
volatile bool displayReady = false;
const bool partial = true;
const bool full = false;

// ################ TIME VARIABLES ##########################################################
String time_str, date_str, date_dd_mm_str; // strings to hold time and date
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;

typedef struct
{ // For current Day and Day 1, 2, 3, etc
  String Time;
  float High;
  float Low;
} HL_record_type;

// ################ PROGRAM VARIABLES and OBJECTS ##########################################
#define max_readings 40
uint8_t MaxReadings = max_readings;
float pressure_readings[max_readings] = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings] = {0};
float rain_readings[max_readings] = {0};
float snow_readings[max_readings] = {0};
Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];
HL_record_type HLReadings[max_readings];

// Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int SleepDurationPreset = 60; // default, it will be overwritten in load_config() from EEPROM;
int SleepDuration;
int SleepTime = 23; // Sleep after (23+1) 00:00 to save battery power
int WakeupTime = 0; // Don't wakeup until after 07:00 to save battery power
int random_fetch_delay_s;
int wifi_setup_portal_timeout = 15;

// ############## BUTTON, INTERRUPT, and RETAINING VARIABLES ################################
#define BUTTON_PIN 39
// #define LED_PIN    19 // this was conflicting with the display functionality, so it cannot be used
RTC_DATA_ATTR volatile int8_t popup_displayed = 255;
RTC_DATA_ATTR volatile int8_t buttonWake_cnt = 0; // Use RTC_DATA_ATTR to preserve value during deep sleep
void IRAM_ATTR handleButtonInterrupt()
{
}

// ####################################### PROGRAM ##############################################
void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);
  StartTime = millis();
  Serial.begin(115200);
  Serial.println("\n~~~~~~~~~~~~~~~~~~~~~~~");
  Serial.println("Weather station active!");
  randomSeed(esp_random());
  random_fetch_delay_s = random(0, 120); //add a random window for avoiding network congestion in case of multiple devices

  // Load all saved settings from EEPROM or program defaults
  load_config();
  Serial.print("station ID: ");
  Serial.println(device_id);
  SleepDuration = SleepDurationPreset;

  // button update logic
  const auto wakeup_cause = esp_sleep_get_wakeup_cause();
  Serial.print("Wakeup cause: ");
  switch (esp_sleep_get_wakeup_cause())
  {
  case 0: // show main weather section
    Serial.println("Power ON / reset");
    buttonWake_cnt = 0;
    break;
  case 2: // button press - cycle through weather forecast screens
    Serial.println("External interrupt (button press)");
    buttonWake_cnt++;
    break;
  case 4: // timer wakeup - actualize main weather section
    Serial.println("Timer wakeup");
    buttonWake_cnt = 0;
    break;
  }

  // init display in a separate process to speed up booting
  xTaskCreatePinnedToCore(
      DisplayInitTask, "DispInit",
      4096, // stack size; bump to 6144/8192 if needed
      NULL,
      2, // priority (lower than your time-critical tasks)
      &dispInitTaskHandle,
      1 // core 1
  );

  // any of the following functions may intrrupt normal startup, show a warning or splash screen and send the controller into deep sleep
  CeckBatteryAbovePercentage(10);
  isSetupMode();
  connect2wifi();
  getTime();
  check4popups();
#
  // ################################# Display content logic ########################################
  // advance screen on every button press
  // on long press go to 4 day forecast immediately
  delay(30); // let the button state settle after wake so long-press detection is reliable
  const bool btnPressed = !digitalRead(BUTTON_PIN);
  const bool wokeByButton = wakeup_cause == ESP_SLEEP_WAKEUP_EXT0;

  if (!wokeByButton)
  {
    buttonWake_cnt = 0;
    Serial.println("Showing today's Weather");
    get_weather_data("current");
    get_weather_data("forecast");
    StopWiFi();
    while (!displayReady)
      ;
    ShowTodaysWeather();
    if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED || wakeup_cause == ESP_SLEEP_WAKEUP_TIMER)
      display.display(full); // full refresh
    else
      display.display(partial);
    SleepDuration = SleepDurationPreset;
  }
  else if (btnPressed || buttonWake_cnt == 2)
  {
    buttonWake_cnt = 2;
    Serial.println("Showing 4 day forecast");
    get_weather_data("forecast");
    StopWiFi();
    while (!displayReady)
      ;
    Show4DayForecast();
    display.display(partial);
    SleepDuration = 5;
  }
  else if (buttonWake_cnt == 1)
  {
    Serial.println("Showing next day's forecast");
    get_weather_data("forecast");
    StopWiFi();
    while (!displayReady)
      ;
    ShowNextDayForecast();
    display.display(partial);
    SleepDuration = 5;
  }
  else
  {
    buttonWake_cnt = 0;
    Serial.println("Button wake fallback -> showing today's Weather");
    get_weather_data("current");
    get_weather_data("forecast");
    StopWiFi();
    while (!displayReady)
      ;
    ShowTodaysWeather();
    display.display(partial);
    SleepDuration = SleepDurationPreset;
  }
  delay(500);
  BeginSleep(SleepDuration);
}

// #########################################################################################
void loop()
{
  // Nothing to do here, all work is done in setup()
  // The program will go into deep sleep after setup() is completed
  // and will wake up based on the button press or timer.
}
// #########################################################################################
void ShowTodaysWeather()
{
  Draw_Heading_Section();
  DisplayWXicon(107, 44, WxConditions[0].Icon, LargeIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(0, 35, String(WxConditions[0].Temperature, 1) + "°", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(45, 35, "/ " + String(WxConditions[0].Humidity, 0) + "%", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  display.drawLine(0, 72, (5 * 43), 72, GxEPD_BLACK); // Draw width of the 5 weather forcasts
  for (int i = 0; i <= 4; i++)
  {
    Draw_3hr_Forecast(i * 43, 96, i);
  }
  DisplayAstronomySection(142, 18); // Astronomy section Sun rise/set and Moon phase plus icon
  DrawSmallWind(231, 75, WxConditions[0].Winddir, WxConditions[0].Windspeed);
  DrawPressureTrend(0, 54, WxConditions[0].Pressure, WxConditions[0].Trend);
}
// #########################################################################################
void ShowNextDayForecast()
{
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 36, "weather tomorrow:", LEFT);
  int startIndex = tomorrowStartIndex(6);
  if (startIndex < 0)
  {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(3, 65, "forecast unavailable", LEFT);
    return;
  }

  Serial.println("Forecast for " + String(WxForecast[startIndex].Period));
  int visibleSlots = MaxReadings - startIndex;
  if (visibleSlots > 5)
    visibleSlots = 5;

  for (int i = 0; i < visibleSlots; i++)
  {
    Draw_Next_Day_3hr_Forecast(i * 43, 96, startIndex + i);
  }
  display.drawLine(0, 63, (5 * 43), 63, GxEPD_BLACK); // Draw width of the 5 weather forcasts
  int windIndex = startIndex + 1;
  if (windIndex >= MaxReadings)
    windIndex = MaxReadings - 1;
  DrawSmallWind(231, 75, WxForecast[windIndex].Winddir, WxForecast[windIndex].Windspeed);
}
// #########################################################################################
void Show4DayForecast()
{
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 33, "4-day forecast:", LEFT);
  String todayDate = LocalDateKey(0);
  int dayCount = 0;

  for (int forecastIndex = 0; forecastIndex < MaxReadings && dayCount < 4;)
  {
    String forecastDate = ForecastDateKey(forecastIndex);
    if (forecastDate <= todayDate)
    {
      forecastIndex++;
      continue;
    }

    int dayStart = forecastIndex;
    int dayEnd = forecastIndex;
    while (dayEnd + 1 < MaxReadings && ForecastDateKey(dayEnd + 1) == forecastDate)
    {
      dayEnd++;
    }

    HLReadings[dayCount].High = WxForecast[dayStart].High;
    HLReadings[dayCount].Low = WxForecast[dayStart].Low;
    int maxPos = dayStart;
    for (int r = dayStart; r <= dayEnd; r++)
    {
      if (WxForecast[r].High >= HLReadings[dayCount].High)
      {
        HLReadings[dayCount].High = WxForecast[r].High;
        maxPos = r;
      }
      if (WxForecast[r].Low <= HLReadings[dayCount].Low)
      {
        HLReadings[dayCount].Low = WxForecast[r].Low;
      }
    }

    Draw_4_Day_Forecast(28, 85, maxPos, dayCount, 57); // x,y coordinates, forecast number, position, spacing width
    Serial.println("Day " + String(dayCount) + ": Max = " + String(HLReadings[dayCount].High) + " Min = " + String(HLReadings[dayCount].Low));

    dayCount++;
    forecastIndex = dayEnd + 1;
  }
}
// #########################################################################################
void Draw_Heading_Section()
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(0, 1, Location_name, LEFT);
  drawStringMaxWidth(SCREEN_WIDTH, 9, SCREEN_WIDTH, date_str, RIGHT); //+ " " + time_str
  DrawBattery(80, 12);
  display.drawLine(0, 11, SCREEN_WIDTH, 11, GxEPD_BLACK);
}
// #########################################################################################
void Draw_3hr_Forecast(int x, int y, int index)
{
  DisplayWXicon(x + 22, y + 6, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 7, y - 21, WxForecast[index].Period.substring(11, 16), LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y + 18, String(WxForecast[index].Temperature, 0) + "°", LEFT);
  display.drawLine(x + 43, y - 24, x + 43, y - 24 + 52, GxEPD_BLACK);
  display.drawLine(x, y - 24 + 52, x + 43, y - 24 + 52, GxEPD_BLACK);
}
// #########################################################################################
void Draw_Next_Day_3hr_Forecast(int x, int y, int index)
{
  DisplayWXicon(x + 22, y + 3, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 4, y - 25, WxForecast[index].Period.substring(11, 16), LEFT);
  drawString(x + 16, y + 17, String(WxForecast[index].Temperature, 0) + "°", LEFT);
  display.drawLine(x + 44, y - 32, x + 44, y - 32 + 57, GxEPD_BLACK);
}
// #########################################################################################
void Draw_4_Day_Forecast(int x, int y, int forecast, int Dposition, int fwidth)
{
  x += fwidth * Dposition;
  DisplayWXicon(x + 10, y + 5, WxForecast[forecast].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  String ForecastDay = GetForecastDay(WxForecast[forecast].Dt);
  drawString(x + 8, y - 22, ForecastDay, CENTER);
  drawString(x + 16, y + 19, String(HLReadings[Dposition].High, 0) + "°/" + String(HLReadings[Dposition].Low, 0) + "°", CENTER);
  display.drawRect(x - 18, y - 30, fwidth + 1, 65, GxEPD_BLACK);
}
// #########################################################################################
void DisplayAstronomySection(int x, int y)
{
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x, y + 16, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  UtcDateTime utc = getUtcDateTime();
  drawString(x, y + 33, MoonPhase(utc.day, utc.month, utc.year) + " " + MoonIllumination(utc.day, utc.month, utc.year, utc.hour) + "%", LEFT);
  DrawMoon(x + 62, y - 15, utc.day, utc.month, utc.year);
}
// #########################################################################################
void DisplayWXicon(int x, int y, String IconName, bool IconSize)
{
  // Serial.println("Icon name: " + IconName);
  if (IconName == "01d" || IconName == "01n")
    Sunny(x, y, IconSize, IconName); // Serial.println("Sunny");}
  else if (IconName == "02d" || IconName == "02n")
    MostlySunny(x, y, IconSize, IconName); // Serial.println("MostlySunny");}
  else if (IconName == "03d" || IconName == "03n")
    MostlyCloudy(x, y, IconSize, IconName); // Serial.println("MostlyCloudy");}
  else if (IconName == "04d" || IconName == "04n")
    Cloudy(x, y, IconSize, IconName); // Serial.println("Cloudy");}
  else if (IconName == "09d" || IconName == "09n")
    ChanceRain(x, y, IconSize, IconName); // Serial.println("ChanceRain");}
  else if (IconName == "10d" || IconName == "10n")
    Rain(x, y, IconSize, IconName); // Serial.println("Rain");}
  else if (IconName == "11d" || IconName == "11n")
    Tstorms(x, y, IconSize, IconName); // Serial.println("Tstorms");}
  else if (IconName == "13d" || IconName == "13n")
    Snow(x, y, IconSize, IconName); // Serial.println("Snow");}
  else if (IconName == "50d" || IconName == "50n")
    Fog(x, y, IconSize, IconName); // Serial.println("Fog");}
  else
    Nodata(x, y, IconSize, IconName); // Serial.println("Nodata");}
}
// #########################################################################################
void get_weather_data(String type)
{
  byte get_weather_cnt = 0;
  bool receivedOk = false;
  WiFiClient client;

  while (receivedOk == false)
  {
    if (receivedOk == false && type == "current")
    {
      Serial.println("Waiting for weather data...");
      receivedOk = obtain_wx_data(client, "weather");
    }
    if (receivedOk == false && type == "forecast")
    {
      Serial.println("Waiting for forecast data...");
      receivedOk = obtain_wx_data(client, "forecast");
    }
    if (get_weather_cnt > 3 && (!receivedOk))
    {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("Failed to get weather data..."), LEFT);
      drawString(10, 40, String("'") + weatherServer + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(full);
      StopWiFi();
      buttonWake_cnt = -1;
      Serial.println("Failed to get weather data...");
      delay(5000);
      BeginSleep(SleepDuration);
    }
    get_weather_cnt++;
    delay(500);
  }
  Serial.println("Weather data received");
}
// ##########################################################################################
void getTime()
{
  byte get_time_cnt = 0;
  Serial.println("Waiting for timeserver...");
  while (SetupTime() != true)
  {
    if (get_time_cnt > 4)
    {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("Timeserver connection error..."), LEFT);
      drawString(10, 50, String("'") + ntpServer + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(full);
      Serial.println("Connection to timeserver failed...");
      buttonWake_cnt = -1;
      delay(500);
      BeginSleep(SleepDuration);
    }
    get_time_cnt++;
    delay(500);
  }
}
// ##########################################################################################
void isSetupMode()
{
  // Check for setup mode (button held at power-on)
  if (digitalRead(BUTTON_PIN) == LOW && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED)
  { // Power on reset
    Serial.println("Entering setup mode!");
    while (!displayReady)
      ;
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Setup mode"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 60, String("connect to: 'weather_station_wifi'"), LEFT);
    drawString(10, 80, String("open settings: http://192.168.4.1/"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB08_tf);
    drawString(10, 110, String("source code: https://github.com/robertkovax"), LEFT);
    display.display(full);

    run_wifi_setup_portal(wifi_setup_portal_timeout);

    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Setup timeout"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 60, String("going to sleep..."), LEFT);
    drawString(10, 80, String("press button to wake!"), LEFT);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake only on button press
    display.display(full);
    delay(500);
    display.powerOff();
    buttonWake_cnt = -1;
    delay(500);
    esp_deep_sleep_start(); // no timer wakeup until button is pressed
  }
}
// #########################################################################################
void check4popups()
{
  const int popup_msg_addrs[4] = {POPUP1_MSG_ADDR, POPUP2_MSG_ADDR, POPUP3_MSG_ADDR, POPUP4_MSG_ADDR};
  const int popup_date_addrs[4] = {POPUP1_DATE_ADDR, POPUP2_DATE_ADDR, POPUP3_DATE_ADDR, POPUP4_DATE_ADDR};
  uint8_t popup_found = 255;
  for (int i = 0; i < 4; i++)
  {
    String popup_msg = eeprom_read_string(popup_msg_addrs[i], 48);
    String popup_date = eeprom_read_string(popup_date_addrs[i], 8);
    // Serial.println("popup check: " + popup_msg + " - " + popup_date);
    if (popup_date == String(date_dd_mm_str) && popup_msg.length() > 0)
    {
      popup_found = i;
      if (popup_displayed != popup_found)
      {
        Serial.println("Today's popup: " + popup_msg);
        u8g2Fonts.setFont(u8g2_font_helvB14_tf);
        while (!displayReady)
          ;
        popup_msg = decodeEscapes(popup_msg);
        drawStringMaxWidth(10, 20, 170, popup_msg, LEFT);
        Sunny(220, 35, Large, "01");
        u8g2Fonts.setFont(u8g2_font_helvB10_tf);
        drawString(10, 110, String("press Next to continue..."), LEFT);
        display.display(full);
        buttonWake_cnt = -1;
        popup_displayed = i;
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake only on button press
        display.powerOff();
        delay(500);
        esp_deep_sleep_start(); // no timer wakeup until button is pressed
      }
    }
  }
  if (popup_found == 255)
  {
    popup_displayed = 255;
    Serial.println("No popup message today");
  }
}
// #########################################################################################
void CeckBatteryAbovePercentage(byte check_percentage)
{
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1)
  { // Only display if there is a valid reading
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20)
      percentage = 100;
    if (voltage <= 3.50)
      percentage = 0;
    Serial.println("Battery: " + String(voltage) + "V");
    if (percentage <= check_percentage)
    {
      Serial.println("Low battery voltage detected.");
      while (!displayReady)
        ;
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      drawString(10, 30, String("Low battery."), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(10, 70, String("Please recharge and reboot!"), LEFT);
      display.drawRect(90 + 15, 60 - 12, 19, 10, GxEPD_BLACK);
      display.fillRect(90 + 34, 60 - 10, 2, 5, GxEPD_BLACK);
      display.fillRect(90 + 17, 60 - 10, 1, 6, GxEPD_BLACK);
      display.display(full);
      display.powerOff();
      buttonWake_cnt = -1;
      esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake only on button press
      delay(500);
      esp_deep_sleep_start(); // no timer wakeup until button is pressed
    }
  }
}
// #########################################################################################
uint8_t StartWiFi(const uint8_t *mac = nullptr)
{
  Serial.println("Connecting to WiFi SSID: " + String(ssid));

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  uint8_t hw_mac[6] = {};
  esp_wifi_get_mac(WIFI_IF_STA, hw_mac);
  Serial.print("hardware MAC: ");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                hw_mac[0], hw_mac[1], hw_mac[2], hw_mac[3], hw_mac[4], hw_mac[5]);

  // If a valid unicast MAC (not all 00, not all FF, LSB of first byte not multicast), set it.
  auto valid_unicast_mac = [](const uint8_t *m)
  {
    if (!m)
      return false;
    bool all_zero = true, all_ff = true;
    for (int i = 0; i < 6; ++i)
    {
      all_zero &= (m[i] == 0x00);
      all_ff &= (m[i] == 0xFF);
    }
    if (all_zero || all_ff)
      return false;
    if (m[0] & 0x01)
      return false; // multicast bit set -> not a unicast MAC
    return true;
  };

  auto mac_equal = [](const uint8_t *a, const uint8_t *b)
  {
    for (int i = 0; i < 6; i++)
    {
      if (a[i] != b[i])
        return false;
    }
    return true;
  };

  if (valid_unicast_mac(mac) && !mac_equal(mac, hw_mac))
  {
    // Must stop before changing MAC on ESP32
    esp_wifi_stop();
    Serial.printf("setting MAC to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, const_cast<uint8_t *>(mac));
    if (err != ESP_OK)
    {
      Serial.printf("esp_wifi_set_mac failed: 0x%04X\n", err);
    }
    else
    {
      Serial.print("setting MAC success: ");
    }
  }
  else if (!mac_equal(mac, hw_mac))
  {
    Serial.print("reverting to hardware MAC: ");
    //save also to eeprom
    char mac_c[18];
    snprintf(mac_c, sizeof(mac_c),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             hw_mac[0], hw_mac[1], hw_mac[2], hw_mac[3], hw_mac[4], hw_mac[5]);
    eeprom_write_string(MAC_ADDR, String(mac_c), sizeof(mac_c));
    eeprom_commit();
  }
  else{
    Serial.print("using hardware MAC: ");
  }

  // Verify MAC address
  uint8_t cur[6] = {};
  esp_wifi_get_mac(WIFI_IF_STA, cur);
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                cur[0], cur[1], cur[2], cur[3], cur[4], cur[5]);

  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK)
  {
    Serial.printf("esp_wifi_start err=0x%02X\n", err);
  }

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // Return current status WITHOUT waiting
  return WiFi.status();
}
// ##########################################################################################
// ---------------- Captive portal / Meraki network-auth ----------------

// Fallback from a captured URL. The code first tries to discover a fresh
// base_grant_url for this ESP32, then falls back to this grant endpoint.
static const char *CAPTIVE_FALLBACK_GRANT_URL = "https://n148.network-auth.com/splash/grant";
static const char *CAPTIVE_CONTINUE_URL = "http://www.msftconnecttest.com/redirect";
static const uint32_t CAPTIVE_DURATION_SEC = 3600;
static const char *CAPTIVE_PROBE_URL = "http://www.msftconnecttest.com/redirect";
static const char *ONLINE_TEST_URL = "http://www.msftconnecttest.com/connecttest.txt";
static const char *ONLINE_TEST_BODY = "Microsoft Connect Test";

static String captiveSplashUrl;
static String captiveNetworkAuthCookie;
static String captiveSocialCookie;
static String captiveLandingUrl;
static String captiveGrantReferer;

static int hexVal(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool isUrlUnreserved(char c)
{
  return (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == '.' || c == '~';
}

static bool isAsciiSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static bool isHtmlNameChar(char c)
{
  return (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == ':';
}

static String urlEncode(const String &in)
{
  String out;
  char buf[4];

  for (size_t i = 0; i < in.length(); i++)
  {
    uint8_t c = (uint8_t)in[i];

    if (isUrlUnreserved((char)c))
    {
      out += (char)c;
    }
    else
    {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }

  return out;
}

static String urlDecode(const String &in)
{
  String out;

  for (size_t i = 0; i < in.length(); i++)
  {
    char c = in[i];

    if (c == '+')
    {
      out += ' ';
    }
    else if (c == '%' && i + 2 < in.length())
    {
      int hi = hexVal(in[i + 1]);
      int lo = hexVal(in[i + 2]);

      if (hi >= 0 && lo >= 0)
      {
        out += (char)((hi << 4) | lo);
        i += 2;
      }
      else
      {
        out += c;
      }
    }
    else
    {
      out += c;
    }
  }

  return out;
}

static String getQueryParam(const String &url, const char *name)
{
  int q = url.indexOf('?');
  if (q < 0) return "";

  String wanted = String(name);
  int start = q + 1;

  while (start < url.length())
  {
    int end = url.indexOf('&', start);
    if (end < 0) end = url.length();

    int eq = url.indexOf('=', start);

    if (eq >= start && eq < end)
    {
      String key = url.substring(start, eq);
      if (key == wanted)
      {
        return urlDecode(url.substring(eq + 1, end));
      }
    }

    start = end + 1;
  }

  return "";
}

static String getUrlOrigin(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);
  if (pathStart < 0) return url;

  return url.substring(0, pathStart);
}

static String getUrlBasePath(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int pathStart = url.indexOf('/', schemeEnd + 3);
  if (pathStart < 0) return url + "/";

  int queryStart = url.indexOf('?', pathStart);
  int lastSlash = url.lastIndexOf('/', queryStart >= 0 ? queryStart : url.length());
  if (lastSlash < pathStart) return getUrlOrigin(url) + "/";

  return url.substring(0, lastSlash + 1);
}

static String resolveUrl(const String &baseUrl, const String &maybeRelative)
{
  if (maybeRelative.startsWith("http://") || maybeRelative.startsWith("https://"))
  {
    return maybeRelative;
  }

  if (maybeRelative.startsWith("//"))
  {
    int schemeEnd = baseUrl.indexOf(':');
    if (schemeEnd >= 0) return baseUrl.substring(0, schemeEnd + 1) + maybeRelative;
  }

  if (maybeRelative.startsWith("/"))
  {
    return getUrlOrigin(baseUrl) + maybeRelative;
  }

  return getUrlBasePath(baseUrl) + maybeRelative;
}

static String cookiePairFromSetCookie(const String &setCookie)
{
  int end = setCookie.indexOf(';');
  if (end < 0) end = setCookie.length();

  String pair = setCookie.substring(0, end);
  pair.trim();
  return pair;
}

static void appendCookiePairToJar(String &jar, const String &pair)
{
  String clean = pair;
  clean.trim();

  int eq = clean.indexOf('=');
  if (eq <= 0) return;

  String name = clean.substring(0, eq);

  if (jar.indexOf(name + "=") >= 0)
  {
    return;
  }

  if (jar.length())
  {
    jar += "; ";
  }

  jar += clean;
}

static void storeSetCookieInJar(String &jar, const String &setCookie)
{
  String pair = cookiePairFromSetCookie(setCookie);
  if (pair.length())
  {
    appendCookiePairToJar(jar, pair);
  }
}

static bool cookieJarHas(const String &jar, const char *cookieName)
{
  return jar.indexOf(String(cookieName) + "=") >= 0;
}

static String getUrlHost(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);

  String hostPort = pathStart < 0 ? url.substring(hostStart) : url.substring(hostStart, pathStart);

  int colon = hostPort.indexOf(':');
  if (colon >= 0)
  {
    return hostPort.substring(0, colon);
  }

  return hostPort;
}

static uint16_t getUrlPort(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return 80;

  String scheme = url.substring(0, schemeEnd);
  scheme.toLowerCase();

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);

  String hostPort = pathStart < 0 ? url.substring(hostStart) : url.substring(hostStart, pathStart);

  int colon = hostPort.indexOf(':');
  if (colon >= 0)
  {
    return (uint16_t)hostPort.substring(colon + 1).toInt();
  }

  return scheme == "https" ? 443 : 80;
}

static String getUrlPathAndQuery(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "/";

  int pathStart = url.indexOf('/', schemeEnd + 3);
  if (pathStart < 0) return "/";

  return url.substring(pathStart);
}

static String getHeaderValue(const String &line, const char *headerName)
{
  String prefix = String(headerName) + ":";

  if (line.length() < prefix.length())
  {
    return "";
  }

  String actual = line.substring(0, prefix.length());

  if (!actual.equalsIgnoreCase(prefix))
  {
    return "";
  }

  String value = line.substring(prefix.length());
  value.trim();
  return value;
}

static bool isRedirectStatus(int code)
{
  return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

static String appendQueryParam(const String &url, const char *name, const String &value)
{
  String out = url;
  out += (out.indexOf('?') >= 0) ? "&" : "?";
  out += name;
  out += "=";
  out += urlEncode(value);
  return out;
}

static String buildGrantUrl(const String &baseGrantUrl, const String &continueUrl)
{
  String grantUrl = appendQueryParam(baseGrantUrl, "continue_url", continueUrl);

  if (CAPTIVE_DURATION_SEC > 0)
  {
    grantUrl += "&duration=";
    grantUrl += String(CAPTIVE_DURATION_SEC);
  }

  return grantUrl;
}

static String getHtmlAttr(const String &tag, const char *attrName)
{
  String wanted = String(attrName);
  String lowerTag = tag;
  lowerTag.toLowerCase();
  wanted.toLowerCase();

  int pos = lowerTag.indexOf(wanted);

  while (pos >= 0)
  {
    int nameEnd = pos + wanted.length();
    bool leftOk = (pos == 0) || !isHtmlNameChar(lowerTag[pos - 1]);
    bool rightOk = nameEnd < lowerTag.length() && (lowerTag[nameEnd] == '=' || isAsciiSpace(lowerTag[nameEnd]));

    if (leftOk && rightOk)
    {
      int eq = lowerTag.indexOf('=', nameEnd);
      if (eq >= 0)
      {
        int valueStart = eq + 1;
        while (valueStart < tag.length() && isAsciiSpace(tag[valueStart])) valueStart++;

        if (valueStart < tag.length() && (tag[valueStart] == '"' || tag[valueStart] == '\''))
        {
          char quote = tag[valueStart++];
          int valueEnd = tag.indexOf(quote, valueStart);
          if (valueEnd >= 0) return tag.substring(valueStart, valueEnd);
        }
        else
        {
          int valueEnd = valueStart;
          while (valueEnd < tag.length() && !isAsciiSpace(tag[valueEnd]) && tag[valueEnd] != '>') valueEnd++;
          return tag.substring(valueStart, valueEnd);
        }
      }
    }

    pos = lowerTag.indexOf(wanted, pos + 1);
  }

  return "";
}

static String submitCaptiveLandingForm(const String &landingUrl, const String &body)
{
  String lowerBody = body;
  lowerBody.toLowerCase();

  int formStart = lowerBody.indexOf("<form");
  if (formStart < 0)
  {
    Serial.println("Captive landing form: none found.");
    return "";
  }

  int formTagEnd = lowerBody.indexOf('>', formStart);
  int formEnd = lowerBody.indexOf("</form", formTagEnd);
  if (formTagEnd < 0 || formEnd < 0)
  {
    Serial.println("Captive landing form: malformed.");
    return "";
  }

  String formTag = body.substring(formStart, formTagEnd + 1);
  String action = getHtmlAttr(formTag, "action");
  String method = getHtmlAttr(formTag, "method");
  method.toUpperCase();

  if (action.length() == 0)
  {
    action = landingUrl;
  }

  if (method.length() == 0)
  {
    method = "GET";
  }

  String actionUrl = resolveUrl(landingUrl, action);
  String formBody = "";
  int inputPos = formStart;

  while (true)
  {
    inputPos = lowerBody.indexOf("<input", inputPos);
    if (inputPos < 0 || inputPos > formEnd) break;

    int inputEnd = lowerBody.indexOf('>', inputPos);
    if (inputEnd < 0 || inputEnd > formEnd) break;

    String inputTag = body.substring(inputPos, inputEnd + 1);
    String name = getHtmlAttr(inputTag, "name");
    String value = getHtmlAttr(inputTag, "value");
    String type = getHtmlAttr(inputTag, "type");
    type.toLowerCase();

    if (name.length() && type != "button" && type != "reset")
    {
      if (type == "checkbox" && value.length() == 0)
      {
        value = "on";
      }

      if (formBody.length()) formBody += "&";
      formBody += urlEncode(name);
      formBody += "=";
      formBody += urlEncode(value);
    }

    inputPos = inputEnd + 1;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *headerKeys[] = {"Location", "Set-Cookie"};

  String requestUrl = actionUrl;
  if (method == "GET" && formBody.length())
  {
    requestUrl += (requestUrl.indexOf('?') >= 0) ? "&" : "?";
    requestUrl += formBody;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(secureClient, requestUrl))
  {
    Serial.println("Captive landing form begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 2);
  http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");
  http.addHeader("Referer", landingUrl);

  if (captiveSocialCookie.length())
  {
    http.addHeader("Cookie", captiveSocialCookie);
  }

  int code = 0;
  if (method == "POST")
  {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST(formBody);
  }
  else
  {
    code = http.GET();
  }

  String location = http.header("Location");
  if (location.length())
  {
    location = resolveUrl(requestUrl, location);
    captiveGrantReferer = requestUrl;
  }

  String setCookie = http.header("Set-Cookie");
  if (setCookie.length())
  {
    storeSetCookieInJar(captiveSocialCookie, setCookie);
  }

  http.end();
  return (code >= 200 && code < 400) ? location : "";
}

static void primeCaptiveSplashSession(const String &splashUrl)
{
  captiveNetworkAuthCookie = "";
  captiveSocialCookie = "";
  captiveLandingUrl = "";
  captiveGrantReferer = "";

  if (splashUrl.length() == 0)
  {
    return;
  }

  String host = getUrlHost(splashUrl);
  String path = getUrlPathAndQuery(splashUrl);
  uint16_t port = getUrlPort(splashUrl);

  if (host.length() == 0)
  {
    Serial.println("Could not parse splash host.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  if (!client.connect(host.c_str(), port))
  {
    Serial.println("Captive splash raw TLS connect failed.");
    return;
  }

  client.print(String("GET ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("User-Agent: Mozilla/5.0 ESP32 captive portal\r\n");
  client.print("Accept: */*\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");

  client.readStringUntil('\n'); // status line

  while (client.connected() || client.available())
  {
    String line = client.readStringUntil('\n');
    line.trim();

    if (line.length() == 0)
    {
      break;
    }

    String location = getHeaderValue(line, "Location");
    if (location.length())
    {
      captiveLandingUrl = resolveUrl(splashUrl, location);
      continue;
    }

    String setCookie = getHeaderValue(line, "Set-Cookie");
    if (setCookie.length())
    {
      storeSetCookieInJar(captiveNetworkAuthCookie, setCookie);
    }
  }

  client.stop();

  if (!cookieJarHas(captiveNetworkAuthCookie, "p_splash_session"))
  {
    Serial.println("WARNING: p_splash_session cookie was not captured.");
  }
}

static String fetchCaptiveLandingPage(const String &landingUrl)
{
  if (landingUrl.length() == 0)
  {
    return "";
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *headerKeys[] = {"Location", "Set-Cookie"};

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(secureClient, landingUrl))
  {
    Serial.println("Captive landing begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 2);
  http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");

  if (captiveSocialCookie.length())
  {
    http.addHeader("Cookie", captiveSocialCookie);
  }

  int code = http.GET();
  String location = http.header("Location");

  String setCookie = http.header("Set-Cookie");
  if (setCookie.length())
  {
    storeSetCookieInJar(captiveSocialCookie, setCookie);
  }

  String grantUrl = "";

  if (location.indexOf("/splash/grant") >= 0)
  {
    grantUrl = location;
  }
  else
  {
    String baseGrantUrl = getQueryParam(landingUrl, "base_grant_url");
    if (baseGrantUrl.length())
    {
      String continueUrl = getQueryParam(landingUrl, "user_continue_url");
      if (continueUrl.length() == 0)
      {
        continueUrl = CAPTIVE_CONTINUE_URL;
      }

      grantUrl = buildGrantUrl(baseGrantUrl, continueUrl);
    }
  }

  if (code == HTTP_CODE_OK)
  {
    String body = http.getString();
    body.replace("\r", " ");
    body.replace("\n", " ");
    body.trim();

    String formLocation = submitCaptiveLandingForm(landingUrl, body);
    if (formLocation.length())
    {
      if (formLocation.indexOf("/splash/grant") >= 0)
      {
        grantUrl = formLocation;
      }
      else
      {
        Serial.println("Captive landing form did not return a Meraki grant URL.");
      }
    }
  }

  http.end();
  return grantUrl;
}

static bool internetIsOpen()
{
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(client, ONLINE_TEST_URL))
  {
    Serial.println("Online probe begin failed.");
    return false;
  }

  int code = http.GET();

  bool ok = false;

  if (code == HTTP_CODE_OK)
  {
    String body = http.getString();
    body.trim();

    if (body.indexOf(ONLINE_TEST_BODY) >= 0)
    {
      ok = true;
    }
  }

  http.end();
  return ok;
}

static String discoverMerakiGrantUrl()
{
  WiFiClient client;
  HTTPClient http;
  const char *headerKeys[] = {"Location"};

  captiveSplashUrl = "";
  captiveNetworkAuthCookie = "";
  captiveSocialCookie = "";
  captiveLandingUrl = "";
  captiveGrantReferer = "";

  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(client, CAPTIVE_PROBE_URL))
  {
    Serial.println("Captive portal probe begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 1);

  http.GET();
  String location = http.header("Location");

  http.end();

  String baseGrantUrl = getQueryParam(location, "base_grant_url");
  if (baseGrantUrl.length() == 0)
  {
    String origin = getUrlOrigin(location);
    if (origin.length() == 0 || location.indexOf("/splash/") < 0)
    {
      return "";
    }

    captiveSplashUrl = location;
    baseGrantUrl = origin + "/splash/grant";
  }

  String continueUrl = getQueryParam(location, "user_continue_url");
  if (continueUrl.length() == 0)
  {
    continueUrl = getQueryParam(location, "continue_url");
  }

  if (continueUrl.length() == 0)
  {
    continueUrl = CAPTIVE_CONTINUE_URL;
  }

  String grantUrl = baseGrantUrl;

  return buildGrantUrl(grantUrl, continueUrl);
}

static bool callCaptiveGrantUrl(const String &grantUrl, const String &splashUrl = "")
{
  if (grantUrl.length() == 0)
  {
    Serial.println("Empty captive grant URL.");
    return false;
  }

  String finalGrantUrl = grantUrl;

  if (splashUrl.length())
  {
    primeCaptiveSplashSession(splashUrl);
  }

  String landingGrantUrl = fetchCaptiveLandingPage(captiveLandingUrl);
  if (landingGrantUrl.length())
  {
    finalGrantUrl = landingGrantUrl;
  }

  if (!cookieJarHas(captiveNetworkAuthCookie, "p_splash_session"))
  {
    Serial.println("WARNING: Grant will probably fail because p_splash_session is missing.");
  }

  String currentUrl = finalGrantUrl;
  String referer = captiveGrantReferer.length() ? captiveGrantReferer : splashUrl;

  for (byte hop = 0; hop < 5; hop++)
  {
    bool thisIsGrantUserAccess = currentUrl.indexOf("/splash/grant_user_access") >= 0;

    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    HTTPClient http;

    secureClient.setInsecure();

    http.setConnectTimeout(15000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    bool beginOk = false;

    if (currentUrl.startsWith("https://"))
    {
      beginOk = http.begin(secureClient, currentUrl);
    }
    else
    {
      beginOk = http.begin(plainClient, currentUrl);
    }

    if (!beginOk)
    {
      Serial.println("Captive grant begin failed.");
      return false;
    }

    const char *headerKeys[] = {"Location", "Set-Cookie"};
    http.collectHeaders(headerKeys, 2);

    http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");

    if (referer.length())
    {
      http.addHeader("Referer", referer);
    }

    if (currentUrl.indexOf("network-auth.com") >= 0 && captiveNetworkAuthCookie.length())
    {
      http.addHeader("Cookie", captiveNetworkAuthCookie);
    }

    int code = http.GET();

    String setCookie = http.header("Set-Cookie");
    if (setCookie.length() && currentUrl.indexOf("network-auth.com") >= 0)
    {
      storeSetCookieInJar(captiveNetworkAuthCookie, setCookie);
    }

    String location = http.header("Location");
    if (location.length())
    {
      location = resolveUrl(currentUrl, location);
    }

    if (code < 0)
    {
      String err = http.errorToString(code);
      Serial.printf("Captive grant HTTP error: %s\n", err.c_str());
      http.end();
      return false;
    }

    if (thisIsGrantUserAccess && code >= 200 && code < 400)
    {
      http.end();
      return true;
    }

    if (isRedirectStatus(code) && location.length())
    {
      http.end();

      referer = currentUrl;
      currentUrl = location;
      delay(300);
      continue;
    }

    if (currentUrl.indexOf("/splash/grant") >= 0 && currentUrl.indexOf("/splash/grant_user_access") < 0)
    {
      if (code == HTTP_CODE_OK)
      {
        http.getString();
        Serial.println("Grant returned 200 instead of redirecting to grant_user_access.");
        Serial.println("This usually means the Meraki p_splash_session cookie was missing or invalid.");

        http.end();
        return false;
      }
    }

    http.end();
    return code >= 200 && code < 400;
  }

  Serial.println("Too many captive grant redirects.");
  return false;
}

static String buildFallbackGrantUrl()
{
  return buildGrantUrl(CAPTIVE_FALLBACK_GRANT_URL, CAPTIVE_CONTINUE_URL);
}

static bool ensureCaptivePortalAuthenticated()
{
  if (internetIsOpen())
  {
    Serial.println("Internet already available. No captive portal grant needed.");
    return true;
  }

  Serial.println("Internet not open yet. Trying captive portal authentication...");

  String grantUrl = discoverMerakiGrantUrl();

  if (grantUrl.length() == 0)
  {
    Serial.println("Could not discover fresh base_grant_url. Using fallback grant URL.");
    grantUrl = buildFallbackGrantUrl();
  }

  if (!callCaptiveGrantUrl(grantUrl, captiveSplashUrl))
  {
    Serial.println("Captive grant request failed.");
    return false;
  }

  delay(1500);

  if (internetIsOpen())
  {
    Serial.println("Captive portal authentication succeeded.");
    return true;
  }

  Serial.println("Grant request completed, but internet probe still failed.");
  return false;
}

// ##########################################################################################
// Connect with retries + UI + sleep policy. ALL waiting happens here.
void connect2wifi()
{
  // Parse desired MAC
  uint8_t desiredMac[6] = {};
  if (sscanf(MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &desiredMac[0], &desiredMac[1], &desiredMac[2],
             &desiredMac[3], &desiredMac[4], &desiredMac[5]) != 6)
  {
    memset(desiredMac, 0, sizeof(desiredMac)); // use hardware MAC
  }

  byte reconnect_cnt = 0;
  const byte max_reconnect_attempts = 1;
  const unsigned long attempt_timeout_ms = 15000; // full attempt window
  const unsigned long poll_ms = 500;
  const unsigned long cooldown_ms = 800; // small pause between attempts

  for (;;)
  {
    // Kick off (non-blocking)
    StartWiFi(desiredMac);

    // Wait ONLY here: until connected OR attempt window expires
    unsigned long t0 = millis();
    uint8_t status = WL_IDLE_STATUS;
    bool auth_failed = false;

    Serial.print("Connecting..");

    do
    {
      status = WiFi.status();

      if (status == WL_CONNECTED)
      {
        Serial.println("\nWiFi associated.");
        Serial.println("local IP: " + WiFi.localIP().toString());

        if (!ensureCaptivePortalAuthenticated())
        {
          Serial.println("WiFi associated, but captive portal authentication failed.");
          auth_failed = true;
          break;
        }

        int wifi_signal = WiFi.RSSI();
        Serial.println("\nWiFi connected + captive portal authenticated!" +
                       String("\nlocal IP: ") + WiFi.localIP().toString() +
                       String("\nstrength: ") + String(wifi_signal) + " dBm");
        return; // success
      }

      delay(poll_ms);
      Serial.print(".");
    } while ((millis() - t0) < attempt_timeout_ms);

    // Attempt failed
    reconnect_cnt++;

    if (auth_failed)
    {
      Serial.printf("\nWiFi captive portal auth attempt %u failed (status=%d)\n", reconnect_cnt, status);
    }
    else
    {
      Serial.printf("\nWiFi connection attempt %u failed (status=%d)\n", reconnect_cnt, status);
    }

    if (reconnect_cnt >= max_reconnect_attempts)
    {
      Serial.println("WiFi connection abandoned. Better luck next time...");
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("WiFi connection error... "), LEFT);
      drawString(10, 50, String("ssid: '") + ssid + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update WiFi credentials:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(full);
      buttonWake_cnt = -1;
      delay(500);
      BeginSleep(SleepDuration);
      return; // device will sleep
    }

    // Cooldown + hard reset of station state before next attempt
    WiFi.disconnect(true, true); // forget connection + drop STA
    esp_wifi_stop();             // stop driver to clear state
    delay(cooldown_ms);
  }
}
// #########################################################################################
void StopWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
// #########################################################################################
void BeginSleep(long _sleepDuration)
{
  display.powerOff();
  StopWiFi();
  // Enable wakeup by timer and by button (ext0)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake on LOW (button press)
  if (_sleepDuration < 10)
    _sleepDuration = _sleepDuration * 60;
  else
    _sleepDuration = (_sleepDuration * 60 - ((CurrentMin % _sleepDuration) * 60 + CurrentSec)); // Some ESP32 are too fast to maintain accurate time
  esp_sleep_enable_timer_wakeup((_sleepDuration + random_fetch_delay_s) * 1000000LL);                              // Added 0-sec extra delay to cater for slow ESP32 RTC timers
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(_sleepDuration) + "-secs of sleep");
  //Serial.println("+ random delay: " + String(random_fetch_delay_s) + "s");

  delay(1000);
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}
// #########################################################################################
void InitialiseDisplay()
{
  display.init(0, false, 20); // don't enforce full update at every cold start
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  // Use u8g2 fonts (https://github.com/olikraus/u8g2/wiki/fntlistall)
  display.setRotation(3);                    // Use 1 or 3 for landscape modes
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  // u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.display(partial); // fast fill
}

// #########################################################################################
void DisplayInitTask(void *pv)
{

  InitialiseDisplay();
  Serial.println("\nDisplay init finished");
  displayReady = true; // signal “done”
  vTaskDelete(NULL);   // kill this task
}
