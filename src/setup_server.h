#pragma once
#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <Arduino.h>

extern const uint8_t fw_version_major;
extern const uint8_t fw_version_minor;


extern int SleepDurationPreset;
extern char weatherServer[];

extern char ssid[64];
extern char password[64];
extern char MAC[32];
extern char apikey[64];
extern char LAT[32];
extern char LON[32];
extern char Location_name[32];
extern char Hemisphere[32];
extern char Units[8];
extern char Timezone[32];
extern char ntpServer[32];
extern int gmtOffset_hour;
extern int daylightOffset_hour;


#define EEPROM_SIZE 1024
#define SSID_ADDR 0
#define PASS_ADDR 64
#define APIKEY_ADDR 128
#define LAT_ADDR 192
#define LON_ADDR 224
#define LOCATION_ADDR 256
#define MAC_ADDR 288
#define UNITS_ADDR 384
#define TIMEZONE_ADDR 416
#define GMTOFFSET_ADDR 480
#define DAYLIGHT_ADDR 484
#define SLEEPDURATION_ADDR 488

#define POPUP1_MSG_ADDR 492
#define POPUP1_DATE_ADDR 540
#define POPUP2_MSG_ADDR 548
#define POPUP2_DATE_ADDR 596
#define POPUP3_MSG_ADDR 604
#define POPUP3_DATE_ADDR 652
#define POPUP4_MSG_ADDR 660
#define POPUP4_DATE_ADDR 708

#define EEPROM_MARKER_ADDR 716
#define EEPROM_MARKER_VALUE 0xA5

#define DEVICE_ID_ADDR 732

// Starts the WiFi setup portal (webserver)
void run_wifi_setup_portal(uint32_t portalTimeoutMinutes);

// Loads credentials and config from EEPROM or owm_credentials.h
void load_config();

uint32_t mac_hash32(const uint8_t _mac[6]);

String eeprom_read_string(int addr, int maxlen);

void erase_eeprom(int eeprom_size, byte value);

#endif // WIFI_SETUP_H
