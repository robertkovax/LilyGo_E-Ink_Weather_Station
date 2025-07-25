#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <Arduino.h>

// Returns true if button is held at power-on
bool is_wifi_setup_requested();

// Starts the WiFi setup portal (webserver)
void run_wifi_setup_portal();

// Loads credentials and config from EEPROM or owm_credentials.h
void load_wifi_config();

#endif // WIFI_SETUP_H
