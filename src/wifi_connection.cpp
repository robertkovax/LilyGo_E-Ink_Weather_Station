#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#include "display.h"
#include "setup_server.h"
#include "wifi_connection.h"

extern int SleepDuration;
extern volatile int8_t buttonWake_cnt;

void BeginSleep(long _sleepDuration);

static const bool full = false;

uint8_t StartWiFi(const uint8_t *mac)
{
  Serial.println("Connecting to WiFi SSID: " + String(ssid));

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

  uint8_t hw_mac[6] = {};
  esp_wifi_get_mac(WIFI_IF_STA, hw_mac);
  Serial.print("hardware MAC: ");
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                hw_mac[0], hw_mac[1], hw_mac[2], hw_mac[3], hw_mac[4], hw_mac[5]);

  auto valid_unicast_mac = [](const uint8_t *m)
  {
    if (!m)
      return false;

    bool all_zero = true;
    bool all_ff = true;
    for (int i = 0; i < 6; ++i)
    {
      all_zero &= (m[i] == 0x00);
      all_ff &= (m[i] == 0xFF);
    }

    if (all_zero || all_ff)
      return false;

    return (m[0] & 0x01) == 0;
  };

  auto mac_equal = [](const uint8_t *a, const uint8_t *b)
  {
    if (!a || !b)
      return false;

    for (int i = 0; i < 6; i++)
    {
      if (a[i] != b[i])
        return false;
    }
    return true;
  };

  if (valid_unicast_mac(mac) && !mac_equal(mac, hw_mac))
  {
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

    char mac_c[18];
    snprintf(mac_c, sizeof(mac_c),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             hw_mac[0], hw_mac[1], hw_mac[2], hw_mac[3], hw_mac[4], hw_mac[5]);
    eeprom_write_string(MAC_ADDR, String(mac_c), sizeof(mac_c));
    eeprom_commit();
  }
  else
  {
    Serial.print("using hardware MAC: ");
  }

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

  return WiFi.status();
}

void connect2wifi()
{
  uint8_t desiredMac[6] = {};
  if (sscanf(MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &desiredMac[0], &desiredMac[1], &desiredMac[2],
             &desiredMac[3], &desiredMac[4], &desiredMac[5]) != 6)
  {
    memset(desiredMac, 0, sizeof(desiredMac));
  }

  byte reconnect_cnt = 0;
  const byte max_reconnect_attempts = 1;
  const unsigned long attempt_timeout_ms = 15000;
  const unsigned long poll_ms = 500;
  const unsigned long cooldown_ms = 800;

  for (;;)
  {
    StartWiFi(desiredMac);

    unsigned long t0 = millis();
    uint8_t status = WL_IDLE_STATUS;

    Serial.print("Connecting..");

    do
    {
      status = WiFi.status();

      if (status == WL_CONNECTED)
      {
        int wifi_signal = WiFi.RSSI();
        Serial.println("\nWiFi connected!" +
                       String("\nlocal IP: ") + WiFi.localIP().toString() +
                       String("\nstrength: ") + String(wifi_signal) + " dBm");
        return;
      }

      delay(poll_ms);
      Serial.print(".");
    } while ((millis() - t0) < attempt_timeout_ms);

    reconnect_cnt++;
    Serial.printf("\nWiFi connection attempt %u failed (status=%d)\n", reconnect_cnt, status);

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
      return;
    }

    WiFi.disconnect(true, true);
    esp_wifi_stop();
    delay(cooldown_ms);
  }
}

void StopWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
