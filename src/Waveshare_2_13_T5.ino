/* ESP Weather Display using an EPD 2.13" Display, obtains data from Open Weather Map, decodes it and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk
*/


//modified by: Robert Kovacs, 2025, info@robertkovacs.de, https://robertkovax.com/
//cloned from: https://github.com/G6EJD/ESP32-e-Paper-Weather-Display

//**New functionality:**
// + Cycle through "current day", "next day" and "4-day" forecast view on button press. 
// + Long-press brings up the 4-day forecast right away. 
// + update wifi credentials via wifi webserver "weather_station_wifi" http://192.168.4.1
// + birthday greeting setup: http://192.168.4.1/popups
// + improved weather icons
// + display low battery warning and enables deep sleep to prevent depleeting the battery
// + display wifi connection failure msg
// + display weather server failure msg
// + customized for Lilygo TTGO T5 V2.3_2.13 e-paper display


#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
#include <WiFi.h>            // Built-in
#include "esp_wifi.h"
#include "time.h"
#include <SPI.h>
#define  ENABLE_GxEPD2_display 0
//#include "forecast_record.h"
//#include "owm_credentials.h"
#include "display.h"
#include "wifi_setup.h"
#include "common.h"
 
//#define DRAW_GRID 1   //Help debug layout changes
#define SCREEN_WIDTH   250
#define SCREEN_HEIGHT  122

// Connections for Lilygo TTGO T5 V2.3_2.13 from
// https://github.com/lewisxhe/TTGO-EPaper-Series#board-pins
static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_CS   = 5;
static const uint8_t EPD_RST  = 16; 
static const uint8_t EPD_DC   = 17; //Data/Command
static const uint8_t EPD_SCK  = 18;   //CLK on pinout?
static const uint8_t EPD_MISO = -1; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(/*CS=D8*/ EPD_CS, /*DC=D3*/ EPD_DC, /*RST=D4*/ EPD_RST, /*BUSY=D2*/ EPD_BUSY));
// #WeAct 2.13 screen module, you need to change GxEPD2_213_B73 to GxEPD2_213_B74
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
// Using fonts: // u8g2_font_helvB08_tf// u8g2_font_helvB10_tf// u8g2_font_helvB12_tf// u8g2_font_helvB14_tf// u8g2_font_helvB24_tf

//################ VARIABLES ###########################
String  time_str, date_str, date_dd_mm_str, ForecastDay; // strings to hold time and date
int     wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long    StartTime = 0;

//################ PROGRAM VARIABLES and OBJECTS ##########################################
#define max_readings 40
uint8_t MaxReadings = max_readings;
float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};
float snow_readings[max_readings]        = {0};

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];


//this is here the initial value, it will be updated from the EEPROM  after setting a value in the web interface
// Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int SleepDurationPreset = 30; 
int SleepDuration;
int  SleepTime     = 23; // Sleep after (23+1) 00:00 to save battery power
int  WakeupTime    = 0;  // Don't wakeup until after 07:00 to save battery power

typedef struct { // For current Day and Day 1, 2, 3, etc
  String Time;
  float  High;
  float  Low;
} HL_record_type;

HL_record_type  HLReadings[max_readings];

//##################################################################################
// Add these defines for button and LED pins
#define BUTTON_PIN 39
//#define LED_PIN    19 //this was conflicting with the display functionality, so it cannot be used
RTC_DATA_ATTR bool first_boot = true;
RTC_DATA_ATTR volatile int8_t popup_displayed = 255;
RTC_DATA_ATTR volatile int8_t buttonWake_cnt = 0; // Use RTC_DATA_ATTR to preserve value during deep sleep

void IRAM_ATTR handleButtonInterrupt() {
  // Button wake
}

//#########################################################################################
void setup() {
  StartTime = millis();
  Serial.begin(115200);
  Serial.println("Weather station active!");
  Serial.print("Wakeup cause: ");
  switch(esp_sleep_get_wakeup_cause()){
    case 0: 
      Serial.println("Power on / reset");
      buttonWake_cnt = 0;
      break;
    case 2: 
      Serial.println("External interrupt (button press)");
      buttonWake_cnt++;
      break;
    case 4: Serial.println("Timer wakeup");
      buttonWake_cnt = 0;
      break;
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  // Load WiFi credentials from EEPROM or defaults
  load_wifi_config();
  SleepDuration = SleepDurationPreset;

  // Check for setup mode (button held at power-on)
  if (digitalRead(BUTTON_PIN) == LOW && esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) { // Power on reset
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
    // Will restart after setup
    while (true) delay(1000);
  }

  InitialiseDisplay();

  if (BatteryAbovePercentage(10)  == false) {
    Serial.println("Battery too low, stopping");
    u8g2Fonts.setFont(u8g2_font_helvB14_tf);
    drawString(10, 30, String("Critical battery level..."),  LEFT);
    u8g2Fonts.setFont(u8g2_font_helvB10_tf);
    drawString(10, 70, String("please recharge and reboot!"), LEFT);
    display.drawRect(90 + 15, 60 - 12, 19, 10, GxEPD_BLACK);
    display.fillRect(90 + 34, 60 - 10, 2, 5, GxEPD_BLACK);
    display.fillRect(90 + 17, 60 - 10, 1, 6, GxEPD_BLACK);
    display.display(false);
    display.powerOff();
    buttonWake_cnt = -1;
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake only on button press
    delay(500);
    esp_deep_sleep_start();
  }

  byte reconnect_cnt = 0;
  uint8_t desiredMac[6] = {0x96,0xe1,0x33,0xe9,0x02,0xf4};
  bool RxWeather = false, RxForecast = false;
  while (StartWiFi(desiredMac) != WL_CONNECTED) { 
    Serial.println("waiting for WiFi connection...");   
    if(reconnect_cnt > 1) {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("WiFi connection failed... "), LEFT);
      drawString(10, 50, String("ssid: '") + ssid + String("'"),  LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update WiFi credentials:"),  LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"),  LEFT);
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
      drawString(10, 20, String("Timeserver connection failed..."), LEFT);
      drawString(10, 50, String("'") + ntpServer + String("'"),  LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"),  LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"),  LEFT);
      display.display(false);
      Serial.println("Connecting to timeserver failed...");
      buttonWake_cnt = -1;
      delay(500);
      BeginSleep(SleepDuration);
    }  
    get_time_cnt++;
    delay(500);
  }
 
  byte get_weather_cnt = 0;
  WiFiClient client;
  while ((RxWeather == false || RxForecast == false)) {
    if (RxWeather  == false) RxWeather  = obtain_wx_data(client, "weather");
    if (RxForecast == false) RxForecast = obtain_wx_data(client, "forecast");
    Serial.println("waiting for weather data...");   
    if(get_weather_cnt > 3 && (!RxWeather || !RxForecast)) {
      u8g2Fonts.setFont(u8g2_font_helvB12_tf);
      drawString(10, 20, String("Failed to get weather data..."), LEFT);
      drawString(10, 40, String("'") + weatherServer + String("'"),  LEFT);
      u8g2Fonts.setFont(u8g2_font_helvB08_tf);
      drawString(10, 90, String("Update Settings:"),  LEFT);
      drawString(10, 105, String("turn Off-->On while holding the 'Next' button"),  LEFT);
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

  //check for up to 4 popup meassages
  const int popup_msg_addrs[4] = {POPUP1_MSG_ADDR, POPUP2_MSG_ADDR, POPUP3_MSG_ADDR, POPUP4_MSG_ADDR};
  const int popup_date_addrs[4] = {POPUP1_DATE_ADDR, POPUP2_DATE_ADDR, POPUP3_DATE_ADDR, POPUP4_DATE_ADDR};
  uint8_t popup_found = 255;
  for (int i = 0; i < 4; i++) {
    String popup_msg = eeprom_read_string(popup_msg_addrs[i], 48);
    String popup_date = eeprom_read_string(popup_date_addrs[i], 8);
    Serial.println("popup check: " + popup_msg + " - " + popup_date);
    if (popup_date== String(date_dd_mm_str) && popup_msg.length() > 0) {
      popup_found = i;
      if(popup_displayed != popup_found){
        Serial.println("Today's popup: " + popup_msg);
        u8g2Fonts.setFont(u8g2_font_helvB14_tf);
       //drawString(10, 20, String(popup_msg), LEFT);
        drawStringMaxWidth(10, 20, 170, popup_msg, LEFT);
        Sunny(220, 35, Large, "01");         
        u8g2Fonts.setFont(u8g2_font_helvB10_tf);
        drawString(10, 110, String("press Next to continue..."), LEFT);
        display.display(false);
        buttonWake_cnt = -1;
        popup_displayed = i;
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake only on button press
        delay(500);
        esp_deep_sleep_start();
      }
    }
  }
  if (popup_found == 255) {
    popup_displayed = 255;
    Serial.println("No popup msg today");
  }
  

  //update display on wakeup
  if ((((esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) || first_boot == true || buttonWake_cnt <= 0 || buttonWake_cnt >= 3) && digitalRead(BUTTON_PIN)) 
  || (( buttonWake_cnt == 3) && !digitalRead(BUTTON_PIN))) {
    first_boot = false;
    buttonWake_cnt = 0;
    Serial.println("Show today's Weather");
    DisplayWeather();
    display.display(false);
    SleepDuration = SleepDurationPreset;
    display.powerOff();
  }else if (buttonWake_cnt == 1 && digitalRead(BUTTON_PIN))  {
    Serial.println("Show next day's forecast");
    ShowNextDayForecast();
    display.display(false);
    SleepDuration = 5; //after 5 minutes go back to current day display
    display.powerOff();
  } else if (buttonWake_cnt == 2 || !digitalRead(BUTTON_PIN))  {
    buttonWake_cnt = 2;
    Serial.println("Show 4 day forecast");
    Show4DayForecast();
    display.display(false);
    SleepDuration = 5; //after 5 minutes go back to current day display
    display.powerOff();
  }
  delay(500);
  BeginSleep(SleepDuration);
}

//#########################################################################################
void loop() {
  // Nothing to do here, all work is done in setup()
  // The program will go into deep sleep after setup() is completed
  // and will wake up based on the button press or timer.
}

//#########################################################################################
void Show4DayForecast() {
  Draw_Heading_Section();
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 33, "4-day forecast:", LEFT);

  int forecastStart = 0, Dposition = 0;
  for (int i = 2; i < MaxReadings; i++) {
    if (WxForecast[i].Period.substring(11, 13) == "09") { //find the start of the next day
      forecastStart = i; 
      break;
    }
  }
  //get HIGH and LOW for each day
  int maxPos = 0, minPos = 0;
  for (int Day = 0; Day < 4; Day++) { //show 4 days
    HLReadings[Day].High  = WxForecast[forecastStart + (8 * Day)].High; //init
    HLReadings[Day].Low  = WxForecast[forecastStart + (8 * Day)].Low; //init
    for (int r = forecastStart + (8 * Day); r < forecastStart + (8 * (Day + 1)); r++) { // 00:00 to 21:00 is 8 readings
      if (WxForecast[r].High >= HLReadings[Day].High) {
        HLReadings[Day].High = WxForecast[r].High;
        maxPos = r;
      }
      if (WxForecast[r].Low <= HLReadings[Day].Low)  {
        HLReadings[Day].Low = WxForecast[r].Low;
        minPos = r; //not used
      }
    }
    DisplayForecastWeather(28, 85, maxPos, Day, 57); // x,y coordinates, forecast number, position, spacing width
    Serial.println("Day " + String(Day) + ": Max = " + String(HLReadings[Day].High) + " Min = " + String(HLReadings[Day].Low));
  }
}
//#########################################################################################
void ShowNextDayForecast() {
  Draw_Heading_Section(); 
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(3, 36, "weather tomorrow:", LEFT);
  for (int i = 3; i < MaxReadings - 4; i++) { 
    //find the start of the next day
    if ((WxForecast[i].Period.substring(11, 13) == "07") || 
    (WxForecast[i].Period.substring(11, 13) == "08") ||
    (WxForecast[i].Period.substring(11, 13) == "09"))  { 
      Serial.println("Forecast for " + String(WxForecast[i].Period) + " = " + String(WxForecast[i].Temperature) + "C" + "pos = " + i);
      Draw_Next_Day_3hr_Forecast(-4, 96, i);    // First  3hr forecast box
      Draw_Next_Day_3hr_Forecast(38, 96, i + 1);    // Second 3hr forecast box
      Draw_Next_Day_3hr_Forecast(83, 96, i + 2);   // Third  3hr forecast box
      Draw_Next_Day_3hr_Forecast(127, 96, i + 3);   // Fourth  3hr forecast box
      Draw_Next_Day_3hr_Forecast(170, 96, i + 4);   // Fifth 3hr forecast box 
      DrawSmallWind(231, 75, WxForecast[i + 1].Winddir, WxForecast[i + 1].Windspeed);
      return;
    }
  }
}
//######################################################################################### 
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
//#########################################################################################
String GetForecastDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
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

//#########################################################################################
void DisplayWXicon(int x, int y, String IconName, bool IconSize) {
  //Serial.println("Icon name: " + IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);        //Serial.println("Sunny");}
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);   //Serial.println("MostlySunny");}
  else if (IconName == "03d" || IconName == "03n")  MostlyCloudy(x, y, IconSize, IconName);  //Serial.println("MostlyCloudy");}
  else if (IconName == "04d" || IconName == "04n")  Cloudy(x, y, IconSize, IconName);        //Serial.println("Cloudy");}
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);    //Serial.println("ChanceRain");}
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);          //Serial.println("Rain");}
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);       //Serial.println("Tstorms");}  
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);          //Serial.println("Snow");} 
  else if (IconName == "50d" || IconName == "50n")  Fog(x, y, IconSize, IconName);           //Serial.println("Fog");}
  else                                              Nodata(x, y, IconSize, IconName);        //Serial.println("Nodata");}
}
//#########################################################################################

void BeginSleep(long _sleepDuration) {
  display.powerOff();
  StopWiFi();
  // Enable wakeup by timer and by button (ext0)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // Wake on LOW (button press)
  long SleepTimer = (_sleepDuration * 60 - ((CurrentMin % _sleepDuration) * 60 + CurrentSec)); //Some ESP32 are too fast to maintain accurate time
  esp_sleep_enable_timer_wakeup((SleepTimer+0) * 1000000LL); // Added 0-sec extra delay to cater for slow ESP32 RTC timers
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT); // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep");
  delay(1000);
  esp_deep_sleep_start();  // Sleep for e.g. 30 minutes
}
//#########################################################################################
void DisplayWeather() {             // 2.13" e-paper display is 250x122 useable resolution
#if DRAW_GRID
  Draw_Grid();
#endif
  UpdateLocalTime();
  Draw_Heading_Section();           // Top line of the display
  Draw_Main_Weather_Section();      // Centre section of display for Location, temperature, Weather report, Wx Symbol and wind direction
  //Index from 0, gets us more 'near' data.
  Draw_3hr_Forecast(-3, 96, 0);    // First  3hr forecast box
  Draw_3hr_Forecast(42, 96, 1);    // Second 3hr forecast box
  Draw_3hr_Forecast(86, 96, 2);   // Third  3hr forecast box
  Draw_3hr_Forecast(132, 96, 3);   // Fourth  3hr forecast box
  Draw_3hr_Forecast(174, 96, 4);   // Fifth 3hr forecast box
  DisplayAstronomySection(142, 18); // Astronomy section Sun rise/set and Moon phase plus icon
  // Not really enough space for these
  //if (WxConditions[0].Visibility > 0) Visibility(110, 40, String(WxConditions[0].Visibility) + "M");
  //if (WxConditions[0].Cloudcover > 0) CloudCover(110, 55, WxConditions[0].Cloudcover);
}
//#########################################################################################
void Draw_Heading_Section() {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  //display.drawRect(0,0,SCREEN_WIDTH,SCREEN_HEIGHT,GxEPD_BLACK);
  drawString(0, 1, City, LEFT);
  //drawString(0, 1, time_str, LEFT);
  drawString(SCREEN_WIDTH, 1, date_str, RIGHT);
  DrawBattery(80, 12);
  display.drawLine(0, 11, SCREEN_WIDTH, 11, GxEPD_BLACK);
}
//#########################################################################################
void Draw_Main_Weather_Section() {
  DisplayWXicon(107, 44, WxConditions[0].Icon, LargeIcon); //WxConditions[0].Icon
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawString(0, 35, String(WxConditions[0].Temperature, 1) + "°", LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(45, 35, "/ " + String(WxConditions[0].Humidity, 0) + "%", LEFT);
  
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  String Wx_Description = WxConditions[0].Forecast0;
  if (WxConditions[0].Forecast1 != "") Wx_Description += " & " +  WxConditions[0].Forecast1;
  if (WxConditions[0].Forecast2 != "" && WxConditions[0].Forecast1 != WxConditions[0].Forecast2) Wx_Description += " & " +  WxConditions[0].Forecast2;
  //drawString(2, 62, TitleCase(Wx_Description), LEFT);
  display.drawLine(0, 72, (5 * 43), 72, GxEPD_BLACK); //Draw width of the 5 weather forcasts
  //Squeeze in a small wind indication in the space we cannot quite squeeze a 3h prediction into
  DrawSmallWind(231, 75, WxConditions[0].Winddir, WxConditions[0].Windspeed);
  //Pressure just getting in the way and very small right now.
  DrawPressureTrend(0, 54, WxConditions[0].Pressure, WxConditions[0].Trend);
}
//#########################################################################################
void Draw_3hr_Forecast(int x, int y, int index) {
  DisplayWXicon(x + 22, y + 6, WxForecast[index].Icon, SmallIcon); //WxForecast[index].Icon
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x + 7, y - 21, WxForecast[index].Period.substring(11, 16), LEFT);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 13, y + 18, String(WxForecast[index].Temperature, 0) + "°", LEFT); //+ "°/" + String(WxForecast[index].Low, 0)
  display.drawLine(x + 43, y - 24, x + 43, y - 24 + 52 , GxEPD_BLACK);
  display.drawLine(x, y - 24 + 52, x + 43, y - 24 + 52 , GxEPD_BLACK);
}
//#########################################################################################
void Draw_Next_Day_3hr_Forecast(int x, int y, int index) {
  DisplayWXicon(x + 22, y + 3, WxForecast[index].Icon, SmallIcon);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  drawString(x + 4, y - 28, WxForecast[index].Period.substring(11, 16), LEFT);
  drawString(x + 16, y + 17, String(WxForecast[index].Temperature, 0) + "°", LEFT); //+ "°/" + String(WxForecast[index].Low, 0)
  display.drawLine(x + 44, y - 30, x + 44, y - 30 + 57 , GxEPD_BLACK);
  display.drawLine(x, y - 30 + 57, x + 44, y - 30 + 57 , GxEPD_BLACK);
}

//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawString(x, y, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE, LEFT);
  drawString(x, y + 16, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET, LEFT);
  time_t now = time(NULL);
  struct tm * now_utc = gmtime(&now);
  const int day_utc   = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc  = now_utc->tm_year + 1900;
  drawString(x, y + 33, MoonPhase(day_utc, month_utc, year_utc, Hemisphere), LEFT);
  DrawMoon(x+60, y-15, day_utc, month_utc, year_utc, Hemisphere);
}

//#########################################################################################
uint8_t StartWiFi(uint8_t mac[6]) {
  Serial.print("\r\nConnecting to: "); Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8); // Google DNS
  WiFi.persistent(false);
  WiFi.mode(WIFI_MODE_STA);
  if (mac[0] != 0x00 && mac[5] != 0x00){
    esp_wifi_stop();         
    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (err != ESP_OK) {
      Serial.printf("esp_wifi_set_mac failed: 0x%04X\n", err);
    } else {
      Serial.printf("STA MAC set to: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    }
  }
  
  esp_err_t err = esp_wifi_start();
  if (err != ESP_OK) {
    Serial.printf("esp_wifi_start err=0x%02X\n", err);
  }

  //Verify MAC
  // uint8_t cur[6]; 
  // esp_wifi_get_mac(WIFI_IF_STA, cur);
  // Serial.printf("Current STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
  //               cur[0],cur[1],cur[2],cur[3],cur[4],cur[5]);

  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  uint8_t connectionStatus;
  bool AttemptConnection = true;
  while (AttemptConnection) {
    connectionStatus = WiFi.status();
    if (millis() > start + 15000) { // Wait 15-secs maximum
      AttemptConnection = false;
    }
    if (connectionStatus == WL_CONNECTED || connectionStatus == WL_CONNECT_FAILED) {
      AttemptConnection = false;
    }
    delay(50);
  }
  if (connectionStatus == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI(); // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  }
  else Serial.println("WiFi connection *** FAILED ***");
  return connectionStatus;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_h * 3600, daylightOffset_h * 3600, ntpServer, "time.nist.gov"); //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable
  delay(100);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//#########################################################################################
void InitialiseDisplay() {
  //display.init(115200, true, 0, false);
  display.init(0); //for older Waveshare HAT's
  SPI.end();
  SPI.begin(EPD_SCK, EPD_MISO, EPD_MOSI, EPD_CS);
  display.setRotation(3);                    // Use 1 or 3 for landscape modes
  u8g2Fonts.begin(display);                  // connect u8g2 procedures to Adafruit GFX
  u8g2Fonts.setFontMode(1);                  // use u8g2 transparent mode (this is default)
  u8g2Fonts.setFontDirection(0);             // left to right (this is default)
  u8g2Fonts.setForegroundColor(GxEPD_BLACK); // apply Adafruit GFX color
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE); // apply Adafruit GFX color
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);   // Explore u8g2 fonts from here: https://github.com/olikraus/u8g2/wiki/fntlistall
  display.fillScreen(GxEPD_WHITE);
  display.setFullWindow();
}