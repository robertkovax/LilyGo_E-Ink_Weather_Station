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
#include "esp_wifi.h"
#include "time.h"
#include <SPI.h>
#define ENABLE_GxEPD2_display 0
// #include "forecast_record.h"
// #include "owm_credentials.h"
#include "display.h"
#include "setup_server.h"
#include "common.h"

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

  // Load all saved settings from EEPROM or program defaults
  load_config();
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
  const bool btnPressed = !digitalRead(BUTTON_PIN);
  if (((wakeup_cause == ESP_SLEEP_WAKEUP_TIMER ||
        buttonWake_cnt <= 0 ||
        buttonWake_cnt >= 3) &&
       !btnPressed) ||
      (buttonWake_cnt == 3 && btnPressed))
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
  else if (buttonWake_cnt == 1 && !btnPressed)
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
  else if (buttonWake_cnt == ESP_SLEEP_WAKEUP_EXT0 || btnPressed)
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
  Serial.println("Forecast for " + String(WxForecast[startIndex].Period));
  for (int i = 0; i <= 4; i++)
  {
    Draw_Next_Day_3hr_Forecast(i * 43, 96, startIndex + i);
  }
  display.drawLine(0, 63, (5 * 43), 63, GxEPD_BLACK); // Draw width of the 5 weather forcasts
  DrawSmallWind(231, 75, WxForecast[startIndex + 1].Winddir, WxForecast[startIndex + 1].Windspeed);
}
// #########################################################################################
void Show4DayForecast()
{
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 33, "4-day forecast:", LEFT);
  int forecastStart = tomorrowStartIndex(8); // we get the low at the end of the day, possibly next morning
  int maxPos = 0, minPos = 0;
  for (int DayIndex = 0; DayIndex < 4; DayIndex++)
  {
    HLReadings[DayIndex].High = WxForecast[forecastStart + (8 * DayIndex)].High;
    HLReadings[DayIndex].Low = WxForecast[forecastStart + (8 * DayIndex)].Low;
    for (int r = forecastStart + (8 * DayIndex); r < forecastStart + (8 * (DayIndex + 1)); r++)
    {
      if (WxForecast[r].High >= HLReadings[DayIndex].High)
      {
        HLReadings[DayIndex].High = WxForecast[r].High;
        maxPos = r;
      }
      if (WxForecast[r].Low <= HLReadings[DayIndex].Low)
      {
        HLReadings[DayIndex].Low = WxForecast[r].Low;
        minPos = r; // not used
      }
    }
    Draw_4_Day_Forecast(28, 85, maxPos, DayIndex, 57); // x,y coordinates, forecast number, position, spacing width
    Serial.println("Day " + String(DayIndex) + ": Max = " + String(HLReadings[DayIndex].High) + " Min = " + String(HLReadings[DayIndex].Low));
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
    Serial.println("entering setup mode...");
    while (!displayReady)
      ;
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Setup mode"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 60, String("connect to: 'weather_station_wifi'"), LEFT);
    drawString(10, 80, String("open settings page:"), LEFT);
    drawString(10, 100, String("http://192.168.4.1/"), LEFT);
    display.display(full);

    run_wifi_setup_portal(wifi_setup_portal_timeout);

    display.fillScreen(GxEPD_WHITE);
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Setup timeout"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 60, String("going to sleep..."), LEFT);
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
      Serial.println("critical battery level, please charge!");
      Serial.println("Battery too low, stopping");
      while (!displayReady)
        ;
      u8g2Fonts.setFont(u8g2_font_helvB14_tf);
      drawString(10, 30, String("Critical battery level..."), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB10_tf);
      drawString(10, 70, String("please recharge and reboot!"), LEFT);
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

  // If a valid unicast MAC (not all 00, not all FF, LSB of first byte not multicast), set it.
  auto valid_unicast_mac = [](const uint8_t *m) {
    if (!m) return false;
    bool all_zero = true, all_ff = true;
    for (int i = 0; i < 6; ++i) {
      all_zero &= (m[i] == 0x00);
      all_ff   &= (m[i] == 0xFF);
    }
    if (all_zero || all_ff) return false;
    if (m[0] & 0x01) return false; // multicast bit set -> not a unicast MAC
    return true;
  };

  if (valid_unicast_mac(mac)) {
    // Must stop before changing MAC on ESP32
    esp_wifi_stop();
    Serial.printf("setting MAC to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, const_cast<uint8_t*>(mac));
    if (err != ESP_OK) {
      Serial.printf("esp_wifi_set_mac failed: 0x%04X\n", err);
    } else {
      Serial.print("Set MAC success: ");
    }
  } else {
    Serial.print("Using hardware MAC: ");
  }

  // Verify MAC address
  uint8_t cur[6] = {};
  esp_wifi_get_mac(WIFI_IF_STA, cur);
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                cur[0], cur[1], cur[2], cur[3], cur[4], cur[5]);

  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK) {
    Serial.printf("esp_wifi_start err=0x%02X\n", err);
  }

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  // Return current status WITHOUT waiting
  return WiFi.status();
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
  const unsigned long attempt_timeout_ms = 15000; // full attempt window
  const unsigned long poll_ms            = 500;
  const unsigned long cooldown_ms        = 800;   // small pause between attempts

  for (;;)
  {
    // Kick off (non-blocking)
    StartWiFi(desiredMac);

    // Wait ONLY here: until connected OR attempt window expires
    unsigned long t0 = millis();
    uint8_t status   = WL_IDLE_STATUS;
    Serial.print("Connecting..");
    do {
      status = WiFi.status();
      if (status == WL_CONNECTED) {
        int wifi_signal = WiFi.RSSI();
        Serial.println("\nWiFi connected! \nlocal IP:" + WiFi.localIP().toString() +
                       "\nstrength: " + String(wifi_signal) + " dBm");
        return; // success
      }
      delay(poll_ms);
      Serial.print(".");
    } while ((millis() - t0) < attempt_timeout_ms);

    // Attempt failed (timed out)
    reconnect_cnt++;
    Serial.printf("\nWiFi connection attempt %u failed (status=%d)\n", reconnect_cnt, status);

    if (reconnect_cnt > 2) {
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
    WiFi.disconnect(true, true);   // forget connection + drop STA
    esp_wifi_stop();               // stop driver to clear state
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
  esp_sleep_enable_timer_wakeup((_sleepDuration + 0) * 1000000LL);                              // Added 0-sec extra delay to cater for slow ESP32 RTC timers
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(_sleepDuration) + "-secs of sleep");
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