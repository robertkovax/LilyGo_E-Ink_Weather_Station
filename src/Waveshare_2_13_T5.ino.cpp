# 1 "C:\\Users\\rober\\AppData\\Local\\Temp\\tmpoib5zsg9"
#include <Arduino.h>
# 1 "C:/Users/rober/Documents/weather_display/Waveshare_2_13_T5/src/Waveshare_2_13_T5.ino"
# 37 "C:/Users/rober/Documents/weather_display/Waveshare_2_13_T5/src/Waveshare_2_13_T5.ino"
#include "owm_credentials.h"
#include "forecast_record.h"
#include "lang.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include "time.h"
#include <SPI.h>
#define ENABLE_GxEPD2_display 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "wifi_setup.h"


#define SCREEN_WIDTH 250
#define SCREEN_HEIGHT 122

enum alignmentType {LEFT, RIGHT, CENTER};



static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_CS = 5;
static const uint8_t EPD_RST = 16;
static const uint8_t EPD_DC = 17;
static const uint8_t EPD_SCK = 18;
static const uint8_t EPD_MISO = -1;
static const uint8_t EPD_MOSI = 23;

GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74( EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
# 77 "C:/Users/rober/Documents/weather_display/Waveshare_2_13_T5/src/Waveshare_2_13_T5.ino"
String version = "6.5";


bool LargeIcon = true, SmallIcon = false;
#define Large 7
#define Small 3
String time_str, date_str, date_dd_mm_str, ForecastDay;
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;


#define max_readings 40

Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];

#include "common.h"

float pressure_readings[max_readings] = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings] = {0};
float rain_readings[max_readings] = {0};
float snow_readings[max_readings] = {0};



int SleepDurationPreset = 30;
int SleepDuration;
int SleepTime = 23;
int WakeupTime = 0;

typedef struct {
  String Time;
  float High;
  float Low;
} HL_record_type;

HL_record_type HLReadings[max_readings];


#define BUTTON_PIN 39

RTC_DATA_ATTR bool first_boot = true;
RTC_DATA_ATTR bool bday_displayed = false;
RTC_DATA_ATTR volatile int8_t buttonWake_cnt = 0;
void IRAM_ATTR handleButtonInterrupt();
void setup();
void loop();
bool is_today_birthday();
void Show4DayForecast();
void ShowNextDayForecast();
void DisplayForecastWeather(int x, int y, int forecast, int Dposition, int fwidth);
String GetForecastDay(int unix_time);
void DisplayWXicon(int x, int y, String IconName, bool IconSize);
void BeginSleep(long _sleepDuration);
void DisplayWeather();
void Draw_Grid();
void Draw_Heading_Section();
void Draw_Main_Weather_Section();
void Draw_3hr_Forecast(int x, int y, int index);
void Draw_Next_Day_3hr_Forecast(int x, int y, int index);
void DisplayAstronomySection(int x, int y);
String MoonPhase(int d, int m, int y, String hemisphere);
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere);
void DrawSmallWind(int x, int y, float angle, float windspeed);
String WindDegToDirection(float winddirection);
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength);
void DrawPressureTrend(int x, int y, float pressure, String slope);
uint8_t StartWiFi();
void StopWiFi();
boolean SetupTime();
boolean UpdateLocalTime();
void DrawBattery(int x, int y);
bool BatteryAbovePercentage(byte check_percentage);
void addcloud(int x, int y, int scale, int linesize);
void addraindrop(int x, int y, int scale);
void addrain(int x, int y, int scale, bool IconSize);
void addsnow(int x, int y, int scale, bool IconSize);
void addtstorm(int x, int y, int scale);
void addsun(int x, int y, int scale, bool IconSize);
void addfog(int x, int y, int scale, int linesize, bool IconSize);
void Sunny(int x, int y, bool IconSize, String IconName);
void MostlySunny(int x, int y, bool IconSize, String IconName);
void MostlyCloudy(int x, int y, bool IconSize, String IconName);
void Cloudy(int x, int y, bool IconSize, String IconName);
void Rain(int x, int y, bool IconSize, String IconName);
void ExpectRain(int x, int y, bool IconSize, String IconName);
void ChanceRain(int x, int y, bool IconSize, String IconName);
void Tstorms(int x, int y, bool IconSize, String IconName);
void Snow(int x, int y, bool IconSize, String IconName);
void Fog(int x, int y, bool IconSize, String IconName);
void Haze(int x, int y, bool IconSize, String IconName);
void CloudCover(int x, int y, int CCover);
void Visibility(int x, int y, String Visi);
void addmoon(int x, int y, int scale, bool IconSize);
void addmoononly(int x, int y, int scale, bool IconSize);
void draw4Star(int16_t x, int16_t y, int16_t radius, uint16_t color);
void Nodata(int x, int y, bool IconSize, String IconName);
void drawString(int x, int y, String text, alignmentType alignment);
void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignmentType alignment);
void InitialiseDisplay();
#line 123 "C:/Users/rober/Documents/weather_display/Waveshare_2_13_T5/src/Waveshare_2_13_T5.ino"
void IRAM_ATTR handleButtonInterrupt() {

}


void setup() {
  StartTime = millis();
  Serial.begin(115200);
  Serial.println("Weather station active");
  Serial.println("Wakeup cause: " + String(esp_sleep_get_wakeup_cause()));
  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    buttonWake_cnt++;
  }else{
    buttonWake_cnt = 0;
  }
  Serial.println("wake button cnt: ");
  Serial.println(buttonWake_cnt);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);


  load_wifi_config();
  SleepDuration = SleepDurationPreset;


  if (digitalRead(BUTTON_PIN) == LOW && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("entering setup mode...");
    InitialiseDisplay();
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Setup mode"), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 60, String("connect to: 'weather_station_wifi'"), LEFT);
    drawString(10, 80, String("open settings page:"), LEFT);
    drawString(10, 100, String("http://192.168.4.1/"), LEFT);
    display.display(false);
    run_wifi_setup_portal();

    while (true) delay(1000);
  }

  InitialiseDisplay();

  if (BatteryAbovePercentage(10) == false) {
    Serial.println("Battery too low, stopping");
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Critical battery level..."), LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 70, String("please recharge and reboot!"), LEFT);
    display.drawRect(90 + 15, 60 - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(90 + 34, 60 - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(90 + 17, 60 - 10, 1, 6, GxEPD_BLACK);
    display.display(false);
    display.powerOff();
    buttonWake_cnt = -1;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
    delay(500);
    esp_deep_sleep_start();
  }

  byte reconnect_cnt = 0;
  bool RxWeather = false, RxForecast = false;
  Serial.println("Attempt to get weather");
  while (StartWiFi() != WL_CONNECTED) {
    Serial.println("waiting for WiFi connection...");
    if(reconnect_cnt > 1) {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("WiFi connection failed... "), LEFT);
      drawString(10, 50, String("ssid: '") + ssid + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update WiFi credentials:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(false);
      Serial.println("WiFi connection failed...");
      buttonWake_cnt = -1;
      delay(500);
      BeginSleep(SleepDuration);
    }
    reconnect_cnt++;
    delay(500);
  }
  Serial.println("WiFi connected");

  byte get_time_cnt = 0;
  while (SetupTime() != true) {
    Serial.println("waiting for timeserver...");
    if(get_time_cnt > 3) {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("Connecting to timeserver failed..."), LEFT);
      drawString(10, 50, String("'") + ntpServer + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(false);
      Serial.println("Connecting to timeserver failed...");
      buttonWake_cnt = -1;
      delay(500);
      BeginSleep(SleepDuration);
    }
    get_time_cnt++;
    delay(500);
  }
  Serial.println("Time set");

  byte get_weather_cnt = 0;
  WiFiClient client;
  while ((RxWeather == false || RxForecast == false)) {
    if (RxWeather == false) RxWeather = obtain_wx_data(client, "weather");
    if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
    Serial.println("waiting for weather data...");
    if(get_weather_cnt > 3 && (!RxWeather || !RxForecast)) {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("Failed to get weather data..."), LEFT);
      drawString(10, 40, String("'") + weatherServer + String("'"), LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"), LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"), LEFT);
      display.display(false);
      StopWiFi();
      buttonWake_cnt = -1;
      Serial.println("Failed to get weather data...");
      delay(5000);
      BeginSleep(SleepDuration);
    }
    get_weather_cnt++;
    delay(500);
  }
  StopWiFi();
  Serial.println("Weather data received");


  Serial.println("Bday check: " + String(eeprom_read_string(BDAY_NAME_ADDR, 32)) + " - " + String(eeprom_read_string(BDAY_DATE_ADDR, 32)));
  if (is_today_birthday() == true && bday_displayed == false) {
    bday_displayed = true;
    Serial.println("Today is a birthday of: " + String(eeprom_read_string(BDAY_NAME_ADDR, 32)));
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(20, 20, String("Sunny B-day " + eeprom_read_string(BDAY_NAME_ADDR, 32)) + "!!!", LEFT);
    Sunny(115, 70, Large, "01");
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(20, 105, String("press Next to continue..."), LEFT);
    display.display(false);
    buttonWake_cnt = -1;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
    delay(500);
    esp_deep_sleep_start();
  }else {
    Serial.println("No birthday today");
  }


  if ((((esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) || first_boot == true || buttonWake_cnt >= 3) && digitalRead(BUTTON_PIN))
  || (( buttonWake_cnt == 3) && !digitalRead(BUTTON_PIN))) {
    first_boot = false;
    buttonWake_cnt = 0;
    Serial.println("Show today's Weather");
    DisplayWeather();
    display.display(false);
    SleepDuration = SleepDurationPreset;
    display.powerOff();
  }else if (buttonWake_cnt == 1 && digitalRead(BUTTON_PIN)) {
    Serial.println("Show next day's forecast");
    ShowNextDayForecast();
    display.display(false);
    SleepDuration = 5;
    display.powerOff();
  } else if (buttonWake_cnt == 2 || !digitalRead(BUTTON_PIN)) {
    buttonWake_cnt = 2;
    Serial.println("Show 4 day forecast");
    Show4DayForecast();
    display.display(false);
    SleepDuration = 5;
    display.powerOff();
  }
  delay(500);
  BeginSleep(SleepDuration);
}


void loop() {



}

bool is_today_birthday() {
  String bday = eeprom_read_string(BDAY_DATE_ADDR, 8);
  if (bday.length() != 5) return false;




  return bday == String(date_dd_mm_str);
}

void Show4DayForecast() {
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 33, "4-day forecast:", LEFT);

  int forecastStart = 0, Dposition = 0;
  for (int i = 2; i < max_readings; i++) {
    if (WxForecast[i].Period.substring(11, 13) == "09") {
      forecastStart = i;
      break;
    }
  }

  int maxPos = 0, minPos = 0;
  for (int Day = 0; Day < 4; Day++) {
    HLReadings[Day].High = WxForecast[forecastStart + (8 * Day)].High;
    HLReadings[Day].Low = WxForecast[forecastStart + (8 * Day)].Low;
    for (int r = forecastStart + (8 * Day); r < forecastStart + (8 * (Day + 1)); r++) {
      if (WxForecast[r].High >= HLReadings[Day].High) {
        HLReadings[Day].High = WxForecast[r].High;
        maxPos = r;
      }
      if (WxForecast[r].Low <= HLReadings[Day].Low) {
        HLReadings[Day].Low = WxForecast[r].Low;
        minPos = r;
      }
    }
    DisplayForecastWeather(28, 85, maxPos, Day, 57);
    Serial.println("Day " + String(Day) + ": Max = " + String(HLReadings[Day].High) + " Min = " + String(HLReadings[Day].Low));
  }
}

void ShowNextDayForecast() {
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 36, "weather tomorrow:", LEFT);
  for (int i = 3; i < max_readings - 4; i++) {
    if (WxForecast[i].Period.substring(11, 13) == "09") {
      Serial.println("Forecast for " + String(WxForecast[i].Period) + " = " + String(WxForecast[i].Temperature) + "C" + "pos = " + i);
      Draw_Next_Day_3hr_Forecast(-4, 96, i);
      Draw_Next_Day_3hr_Forecast(38, 96, i + 1);
      Draw_Next_Day_3hr_Forecast(83, 96, i + 2);
      Draw_Next_Day_3hr_Forecast(127, 96, i + 3);
      Draw_Next_Day_3hr_Forecast(170, 96, i + 4);
      DrawSmallWind(231, 75, WxForecast[i + 1].Winddir, WxForecast[i + 1].Windspeed);
      return;
    }
  }
}

void DisplayForecastWeather(int x, int y, int forecast, int Dposition, int fwidth) {
  GetForecastDay(WxForecast[forecast].Dt);
  x += fwidth * Dposition;
  DisplayWXicon(x + 10, y + 5, WxForecast[forecast].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 8, y - 22, ForecastDay, CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 16, y + 19, String(HLReadings[Dposition].High, 0) + "°/" + String(HLReadings[Dposition].Low, 0) + "°", CENTER);
  display.drawRect(x - 18, y - 30, fwidth + 1, 65, GxEPD_BLACK);
}

String GetForecastDay(int unix_time) {

  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40], FDay[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%p %m/%d/%y", now_tm);
    strftime(FDay, sizeof(FDay), "%w", now_tm);
  }
  ForecastDay = weekday_D[String(FDay).toInt()];
  return output;
}


void DisplayWXicon(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if (IconName == "01d" || IconName == "01n") {Sunny(x, y, IconSize, IconName); Serial.println("Sunny");}
  else if (IconName == "02d" || IconName == "02n") {MostlySunny(x, y, IconSize, IconName); Serial.println("MostlySunny");}
  else if (IconName == "03d" || IconName == "03n") {MostlyCloudy(x, y, IconSize, IconName); Serial.println("MostlyCloudy");}
  else if (IconName == "04d" || IconName == "04n") {Cloudy(x, y, IconSize, IconName); Serial.println("Cloudy");}
  else if (IconName == "09d" || IconName == "09n") {ChanceRain(x, y, IconSize, IconName); Serial.println("ChanceRain");}
  else if (IconName == "10d" || IconName == "10n") {Rain(x, y, IconSize, IconName); Serial.println("Rain");}
  else if (IconName == "11d" || IconName == "11n") {Tstorms(x, y, IconSize, IconName); Serial.println("Tstorms");}
  else if (IconName == "13d" || IconName == "13n") {Snow(x, y, IconSize, IconName); Serial.println("Snow");}
  else if (IconName == "50d" || IconName == "50n") {Fog(x, y, IconSize, IconName); Serial.println("Fog");}
  else {Nodata(x, y, IconSize, IconName); Serial.println("Nodata");}
}


void BeginSleep(long _sleepDuration) {
  display.powerOff();
  StopWiFi();

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
  long SleepTimer = (_sleepDuration * 60 - ((CurrentMin % _sleepDuration) * 60 + CurrentSec));
  esp_sleep_enable_timer_wakeup((SleepTimer+30) * 1000000LL);
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT);
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Starting deep-sleep period...");
  delay(1000);
  esp_deep_sleep_start();
}

void DisplayWeather() {
#if DRAW_GRID
  Draw_Grid();
#endif
  UpdateLocalTime();
  Draw_Heading_Section();
  Draw_Main_Weather_Section();





  Draw_3hr_Forecast(-3, 96, 1);
  Draw_3hr_Forecast(42, 96, 2);
  Draw_3hr_Forecast(86, 96, 3);
  Draw_3hr_Forecast(132, 96, 4);
  Draw_3hr_Forecast(174, 96, 5);
  DisplayAstronomySection(142, 18);



}


void Draw_Grid() {
  int x, y;
  const int grid_step = 10;


  display.drawLine(0, 0, SCREEN_WIDTH-1, 0, GxEPD_BLACK);
  display.drawLine(0, SCREEN_HEIGHT-1, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK);
  display.drawLine(0, 0, 0, SCREEN_HEIGHT-1, GxEPD_BLACK);
  display.drawLine(SCREEN_WIDTH-1, 0, SCREEN_WIDTH-1, SCREEN_HEIGHT-1, GxEPD_BLACK);

  for( x=grid_step; x<SCREEN_WIDTH; x+=grid_step ) {
    for( y=grid_step; y<SCREEN_HEIGHT; y+=grid_step ) {
      display.drawLine(x-1, y, x+1, y, GxEPD_BLACK);
      display.drawLine(x, y-1, x, y+1, GxEPD_BLACK);
    }
  }
}

void Draw_Heading_Section() {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);

  drawString(0, 1, City, LEFT);

  drawString(SCREEN_WIDTH, 1, date_str, RIGHT);
  DrawBattery(80, 12);
  display.drawLine(0, 11, SCREEN_WIDTH, 11, GxEPD_BLACK);
}

void Draw_Main_Weather_Section() {
  DisplayWXicon(107, 44, WxConditions[0].Icon, LargeIcon);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(0, 35, String(WxConditions[0].Temperature, 1) + "°", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(45, 35, "/ " + String(WxConditions[0].Humidity, 0) + "%", LEFT);

  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  String Wx_Description = WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "") Wx_Description += " & " + WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "" && WxConditions[0].Forecast1 != WxConditions[0].Forecast2) Wx_Description += " & " + WxConditions[0].Forecast2;

  display.drawLine(0, 72, (5 * 43), 72, GxEPD_BLACK);

  DrawSmallWind(231, 75, WxConditions[0].Winddir, WxConditions[0].Windspeed);

  DrawPressureTrend(0, 54, WxConditions[0].Pressure, WxConditions[0].Trend);
}

void Draw_3hr_Forecast(int x, int y, int index) {
  DisplayWXicon(x + 22, y + 6, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 7, y - 21, WxForecast[index].Period.substring(11, 16), LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y + 18, String(WxForecast[index].Temperature, 0) + "°", LEFT);
  display.drawLine(x + 43, y - 24, x + 43, y - 24 + 52 , GxEPD_BLACK);
  display.drawLine(x, y - 24 + 52, x + 43, y - 24 + 52 , GxEPD_BLACK);
}

void Draw_Next_Day_3hr_Forecast(int x, int y, int index) {
  DisplayWXicon(x + 22, y + 3, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 4, y - 28, WxForecast[index].Period.substring(11, 16), LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 16, y + 17, String(WxForecast[index].Temperature, 0) + "°", LEFT);
  display.drawLine(x + 44, y - 30, x + 44, y - 30 + 57 , GxEPD_BLACK);
  display.drawLine(x, y - 30 + 57, x + 44, y - 30 + 57 , GxEPD_BLACK);
}


void DisplayAstronomySection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, (Units == "M"?5:7)) + " " + TXT_SUNRISE, LEFT);
  drawString(x, y + 16, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, (Units == "M"?5:7)) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm * now_utc = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString(x, y + 33, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x+60, y-15, day_utc, month_utc, year_utc, Hemisphere);
}

String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c = 365.25 * y;
  e = 30.6 * m;
  jd = c + e + d - 694039.09;
  jd /= 29.53059;
  b = jd;
  jd -= b;
  b = jd * 8 + 0.5;
  b = b & 7;
  if (hemisphere == "south") b = 7 - b;
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

void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 28;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;

  display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_BLACK);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);

    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }

    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
    display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
  }
  display.drawCircle(x + diameter - 1, y + diameter, diameter / 2, GxEPD_BLACK);
}



void DrawSmallWind(int x, int y, float angle, float windspeed) {
#define Cradius 0
  float dx = Cradius * cos((angle - 90) * PI / 180) + x;
  float dy = Cradius * sin((angle - 90) * PI / 180) + y;
  arrow(dx, dy, Cradius - 15, angle, 10, 20);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y+15, WindDegToDirection(angle), CENTER);
  drawString(x+5, y+25, String(windspeed * 3.6, 1), CENTER);
  drawString(x+5, y+35, String(Units == "M" ? " km/h" : " mph"), CENTER);
}
# 630 "C:/Users/rober/Documents/weather_display/Waveshare_2_13_T5/src/Waveshare_2_13_T5.ino"
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
  return Ord_direction[(dir % 16)];
}

void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {




  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;
  float x1 = 0; float y1 = plength;
  float x2 = pwidth / 2; float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, GxEPD_BLACK);
}

void DrawPressureTrend(int x, int y, float pressure, String slope) {
  drawString(x, y, String(pressure, (Units == "M"?0:1)) + (Units == "M" ? "hPa" : "in"), LEFT);
  x = x + 48 - (Units == "M"?0:15); y = y + 3;
  if (slope == "+") {
    display.drawLine(x, y, x + 4, y - 4, GxEPD_BLACK);
    display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "0") {
    display.drawLine(x + 3, y - 4, x + 8, y, GxEPD_BLACK);
    display.drawLine(x + 3, y + 4, x + 8, y, GxEPD_BLACK);
  }
  else if (slope == "-") {
    display.drawLine(x, y, x + 4, y + 4, GxEPD_BLACK);
    display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
  }
}


uint8_t StartWiFi() {
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) {
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI();
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}

void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}

boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");
  setenv("TZ", Timezone, 1);
  tzset();
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}

boolean UpdateLocalTime() {
  struct tm timeinfo;
  char time_output[30], day_output[30], dd_mm_output[10], update_time[30];
  while (!getLocalTime(&timeinfo, 3000)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;

  Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");
  Serial.println(&timeinfo, "%02d.%02m");
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "NL") || (Language == "PL") || (Language == "GR")) {
      sprintf(day_output, "%s, %02u. %s %02u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) % 100);
    }
    else
    {
      sprintf(day_output, "%s  %02u-%s-%02u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) % 100);
    }
    strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);
    strftime(dd_mm_output, sizeof(dd_mm_output), "%d.%m", &timeinfo);
    sprintf(time_output, "%s", update_time);
  }
  else
  {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%y", &timeinfo);
    strftime(dd_mm_output, sizeof(dd_mm_output), "%d.%m", &timeinfo);
    strftime(update_time, sizeof(update_time), "%H:%M", &timeinfo);
    sprintf(time_output, "%s", update_time);
  }
  date_str = day_output;
  date_dd_mm_str = dd_mm_output;
  time_str = time_output;
  return true;
}

void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) {

    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
    drawString(x + 60, y - 11, String(percentage) + "%", RIGHT);

  }
}

bool BatteryAbovePercentage(byte check_percentage){
uint8_t percentage = 100;
float voltage = analogRead(35) / 4096.0 * 7.46;
  if (voltage > 1 ) {
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    Serial.println("Voltage = " + String(voltage));
    if (percentage <= check_percentage) {
      Serial.println("critical battery level, please charge!");
      return false;
    }
  }
return true;
}


void addcloud(int x, int y, int scale, int linesize) {

  display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);
  display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);
  display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK);
  display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK);

  display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);
  display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);
  display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);
  display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE);
  display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE);
}

void addraindrop(int x, int y, int scale) {
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
  x = x + scale * 1.6; y = y + scale / 3;
  display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
  display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y , GxEPD_BLACK);
}

void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 4; d++) {
    addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}

void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
    }
  }
}

void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
      display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
    }
    display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
    if (scale != Small) {
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
      display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
    }
  }
}

void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_BLACK);
  display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
  display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  if (IconSize == LargeIcon) {
    display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_BLACK);
    display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
    display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_BLACK);
  }
  display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
  display.fillCircle(x, y, scale, GxEPD_BLACK);
  display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
}

void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    linesize = 1;
    y = y - 1;
  } else y = y - 3;
  for (int i = 0; i < 6; i++) {
    display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
    display.fillRect(x - scale * 3, y + scale * 2.6, scale * 6, linesize, GxEPD_BLACK);
  }
}

void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = 4;
  if (IconSize == LargeIcon) {
    scale = Large;
    y = y - 4;
  }
  else y = y + 2;

  if (IconName.endsWith("n"))
  {
    addmoononly(x + 10, y-1, scale, IconSize);
     if (IconSize == LargeIcon) {
      draw4Star(x + 17, y-10, 8, GxEPD_BLACK);
      draw4Star(x + 5, y-18, 4, GxEPD_BLACK);
     }else
       draw4Star(x + 9, y-12, 4, GxEPD_BLACK);
   }else if(IconSize == LargeIcon) {
    scale = scale * 1.6;
    addsun(x, y, scale, IconSize);
  }else {
    scale = scale * 1.4;
    addsun(x, y-7, scale, IconSize);
  }
}

void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 0;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  }
  if (scale == Small) linesize = 1;

  if (IconName.endsWith("n")) {
    addmoononly(x, y + (IconSize ? -8 : 0), scale, IconSize);
    addcloud(x, y + offset, scale, linesize);
  }else{
    addcloud(x, y + offset, scale, linesize);
    addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
  }
}

void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
    if (IconName.endsWith("n")) {
      addmoononly(x, y-4, scale, IconSize);
      addcloud(x, y-5, scale, linesize);
      addcloud(x-5, y, scale, linesize);
    }else{
      addsun(x - 10, y - 10, scale, IconSize);
      addcloud(x, y-5, scale, linesize);
      addcloud(x-5, y, scale, linesize);
    }
  }else{
    if (IconName.endsWith("n")) {
      addmoononly(x, y-8, scale, IconSize);
      addcloud(x+3, y, scale, linesize);
      addcloud(x, y+10, scale, linesize);
    }else{
      addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
      addcloud(x+3, y, scale, linesize);
      addcloud(x, y+10, scale, linesize);
    }
  }
}

void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;

    linesize = 1;
    addcloud(x-6, y-7, scale, linesize);
    addcloud(x, y-5, scale, linesize);
    addcloud(x-5, y, scale, linesize);
  }
  else {
    y += 12;

    addcloud(x - 11, y - 10, 5, linesize);
    addcloud(x + 7, y - 21, 5, linesize);
    addcloud(x, y, scale, linesize);
  }
}

void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }

  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addrain(x, y + (IconSize ? 2 : -4), scale, IconSize);
}

void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}

void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) {
    addmoononly(x - (IconSize ? 2 : 0), y - (IconSize ? 12 : 4), scale, IconSize);
  } else {
    addsun(x - scale * 1.8, y - scale * 1.8 - (IconSize ? 0 : 4), scale, IconSize);
  }
  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addrain(x, y + (IconSize ? 2 : -4), scale, linesize);
}

void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }

  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addtstorm(x, y + (IconSize ? 2 : -4), scale);
}

void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }

  addcloud(x, y + (IconSize ? 2 : -4), scale, linesize);
  addsnow(x, y + (IconSize ? 2 : -4), scale, IconSize);
}

void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
    y = y + 5;
  }

  addcloud(x, y + (IconSize ? -2 : -7), scale, linesize);
  addfog(x, y + (IconSize ? 1 : -4), scale, linesize, IconSize);
}

void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }

  addsun(x, y - 2, scale * 1.4, IconSize);
  addfog(x, y + 3 - (IconSize ? 12 : 0), scale * 1.4, linesize, IconSize);
}

void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.6, 2);
  addcloud(x + 3, y - 3, Small * 0.6, 2);
  addcloud(x, y, Small * 0.6, 2);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
}

void Visibility(int x, int y, String Visi) {
  y = y - 3;
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
  }
  start_angle = 3.61; end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
    display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
  }
  display.fillCircle(x, y, r / 4, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 12, y - 3, Visi, LEFT);
}

void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x - 5; y = y + 5;
    display.fillCircle(x - 21, y - 23, scale, GxEPD_BLACK);
    display.fillCircle(x - 14, y - 23, scale * 1.7, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 16, y - 11, scale, GxEPD_BLACK);
    display.fillCircle(x - 11, y - 11, scale * 1.7, GxEPD_WHITE);
  }
}

void addmoononly(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    x = x + 10; y = y + 25;

    display.fillCircle(x - 20, y - 23, scale*2.3, GxEPD_BLACK);
    display.fillCircle(x - 2, y - 25, scale*3.5, GxEPD_WHITE);
  }
  else
  {
    display.fillCircle(x - 8, y - 6, scale * 1.8, GxEPD_BLACK);
    display.fillCircle(x - 3, y - 6, scale * 2, GxEPD_WHITE);
  }
}

void draw4Star(int16_t x, int16_t y, int16_t radius, uint16_t color) {
  float angle_step = 360.0 / 8.0;
  int16_t r_in = round(radius * 0.3);

  int16_t points_x[9];
  int16_t points_y[9];

  points_x[0] = x;
  points_y[0] = y + radius;
  points_x[1] = x + r_in;
  points_y[1] = y + r_in;
  points_x[2] = x + radius;
  points_y[2] = y;
  points_x[3] = x + r_in;
  points_y[3] = y - r_in;
  points_x[4] = x;
  points_y[4] = y - radius;
  points_x[5] = x - r_in;
  points_y[5] = y - r_in;
  points_x[6] = x - radius;
  points_y[6] = y;
  points_x[7] = x - r_in;
  points_y[7] = y + r_in;
  points_x[8] = points_x[0];
  points_y[8] = points_y[0];

  for (int i = 0; i < 8; i++) {
    display.fillTriangle(x, y, points_x[i], points_y[i], points_x[i+1], points_y[i+1], color);
  }
}

void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) u8g2Fonts.setFont(u8g2_font_helvB24_tf); else u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x - 3, y - 8, "?", CENTER);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
}

void drawString(int x, int y, String text, alignmentType alignment) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextWrap(false);
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT) x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y + h);
  u8g2Fonts.print(text);
}

void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignmentType alignment) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
  if (alignment == RIGHT) x = x - w;
  if (alignment == CENTER) x = x - w / 2;
  u8g2Fonts.setCursor(x, y);
  if (text.length() > text_width * 2) {
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    text_width = 42;
    y = y - 3;
  }
  u8g2Fonts.println(text.substring(0, text_width));
  if (text.length() > text_width) {
    u8g2Fonts.setCursor(x, y + h + 15);
    String secondLine = text.substring(text_width);
    secondLine.trim();
    u8g2Fonts.println(secondLine);
  }
}

void InitialiseDisplay() {


  display.init(0);
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(3);
  u8g2Fonts.begin(display);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setFontDirection(0);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();

}