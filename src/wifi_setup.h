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
#define SLEEPDURATION_ADDR 488

#define POPUP1_MSG_ADDR 492
#define POPUP1_DATE_ADDR 524
#define POPUP2_MSG_ADDR 532
#define POPUP2_DATE_ADDR 564
#define POPUP3_MSG_ADDR 572
#define POPUP3_DATE_ADDR 604
#define POPUP4_MSG_ADDR 612
#define POPUP4_DATE_ADDR 644

#define EEPROM_MARKER_ADDR 652
#define EEPROM_MARKER_VALUE 0xA5

// Starts the WiFi setup portal (webserver)
void run_wifi_setup_portal();

// Loads credentials and config from EEPROM or owm_credentials.h
void load_wifi_config();

String eeprom_read_string(int addr, int maxlen);

void erase_eeprom(int eeprom_size, byte value);

#endif // WIFI_SETUP_H
