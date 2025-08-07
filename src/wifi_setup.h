#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <Arduino.h>

#define EEPROM_SIZE 1024
#define SSID_ADDR   0
#define PASS_ADDR   64
#define APIKEY_ADDR 128
#define LAT_ADDR    192
#define LON_ADDR    224
#define CITY_ADDR   256
#define COUNTRY_ADDR 288
#define LANGUAGE_ADDR 320
#define HEMISPHERE_ADDR 352
#define UNITS_ADDR 384
#define TIMEZONE_ADDR 416
#define NTPSERVER_ADDR 448
#define GMTOFFSET_ADDR 480
#define DAYLIGHT_ADDR 484
#define BUTTON_PIN 39
#define LONG_PRESS_MS 2000

#define EEPROM_MARKER_ADDR 550
#define EEPROM_MARKER_VALUE 0xA5

#define BDAY_NAME_ADDR 492
#define BDAY_DATE_ADDR 524


// Starts the WiFi setup portal (webserver)
void run_wifi_setup_portal();

// Loads credentials and config from EEPROM or owm_credentials.h
void load_wifi_config();

String eeprom_read_string(int addr, int maxlen); 

#endif // WIFI_SETUP_H
