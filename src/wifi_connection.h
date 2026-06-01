#pragma once
#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <Arduino.h>

uint8_t StartWiFi(const uint8_t *mac = nullptr);
void connect2wifi();
void StopWiFi();

#endif // WIFI_CONNECTION_H
