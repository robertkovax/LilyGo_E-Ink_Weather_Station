#include "wifi_setup.h"
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include "owm_credentials.h"

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

WebServer wifiServer(80);

// Helper to read/write String to EEPROM
void eeprom_write_string(int addr, const String& value, int maxlen) {
  for (int i = 0; i < maxlen; i++) {
    EEPROM.write(addr + i, i < value.length() ? value[i] : 0);
  }
}
String eeprom_read_string(int addr, int maxlen) {
  char buf[128] = {0};
  EEPROM.readBytes(addr, buf, maxlen-1);
  buf[maxlen-1] = 0;
  String s = String(buf);
  s.trim();
  return s;
}

// Check if button is held at power-on
bool is_wifi_setup_requested() {
  unsigned long start = millis();
  while (millis() - start < LONG_PRESS_MS) {
    if (digitalRead(BUTTON_PIN) == LOW) return true;
    delay(10);
  }
  return false;
}

// Load config from EEPROM or owm_credentials.h
void load_wifi_config() {
  EEPROM.begin(EEPROM_SIZE);
  bool eeprom_initialized = (EEPROM.read(EEPROM_MARKER_ADDR) == EEPROM_MARKER_VALUE);
  if (!eeprom_initialized) {
    // EEPROM empty, use owm_credentials.h and save to EEPROM
    eeprom_write_string(SSID_ADDR, String(::ssid), 64);
    eeprom_write_string(PASS_ADDR, String(::password), 64);
    eeprom_write_string(APIKEY_ADDR, ::apikey, 64);
    eeprom_write_string(LAT_ADDR, ::LAT, 32);
    eeprom_write_string(LON_ADDR, ::LON, 32);
    eeprom_write_string(CITY_ADDR, ::City, 32);
    eeprom_write_string(COUNTRY_ADDR, ::Country, 8);
    eeprom_write_string(HEMISPHERE_ADDR, ::Hemisphere, 8);
    eeprom_write_string(UNITS_ADDR, ::Units, 8);
    eeprom_write_string(TIMEZONE_ADDR, String(::Timezone), 32);
    eeprom_write_string(NTPSERVER_ADDR, String(::ntpServer), 32);
    EEPROM.write(GMTOFFSET_ADDR, (uint8_t)((::gmtOffset_sec >> 0) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR+1, (uint8_t)((::gmtOffset_sec >> 8) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR+2, (uint8_t)((::gmtOffset_sec >> 16) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR+3, (uint8_t)((::gmtOffset_sec >> 24) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR, (uint8_t)((::daylightOffset_sec >> 0) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR+1, (uint8_t)((::daylightOffset_sec >> 8) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR+2, (uint8_t)((::daylightOffset_sec >> 16) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR+3, (uint8_t)((::daylightOffset_sec >> 24) & 0xFF));
    EEPROM.write(EEPROM_MARKER_ADDR, EEPROM_MARKER_VALUE); // Set marker
    EEPROM.commit();
  }
  // Load all config from EEPROM
  String ssid_str = eeprom_read_string(SSID_ADDR, 64);
  String pass_str = eeprom_read_string(PASS_ADDR, 64);
  String apikey_str = eeprom_read_string(APIKEY_ADDR, 64);
  String lat_str = eeprom_read_string(LAT_ADDR, 32);
  String lon_str = eeprom_read_string(LON_ADDR, 32);
  String city_str = eeprom_read_string(CITY_ADDR, 32);
  String country_str = eeprom_read_string(COUNTRY_ADDR, 8);
  String hemisphere_str = eeprom_read_string(HEMISPHERE_ADDR, 8);
  String units_str = eeprom_read_string(UNITS_ADDR, 8);
  String timezone_str = eeprom_read_string(TIMEZONE_ADDR, 32);
  String ntpserver_str = eeprom_read_string(NTPSERVER_ADDR, 32);
  long gmtOffset = EEPROM.read(GMTOFFSET_ADDR) | (EEPROM.read(GMTOFFSET_ADDR+1)<<8) | (EEPROM.read(GMTOFFSET_ADDR+2)<<16) | (EEPROM.read(GMTOFFSET_ADDR+3)<<24);
  int daylightOffset = EEPROM.read(DAYLIGHT_ADDR) | (EEPROM.read(DAYLIGHT_ADDR+1)<<8) | (EEPROM.read(DAYLIGHT_ADDR+2)<<16) | (EEPROM.read(DAYLIGHT_ADDR+3)<<24);
  ssid_str.toCharArray(ssid, 64);
  pass_str.toCharArray(password, 64);
  apikey = apikey_str;
  LAT = lat_str;
  LON = lon_str;
  City = city_str;
  Country = country_str;
  Hemisphere = hemisphere_str;
  Units = units_str;
  Timezone = strdup(timezone_str.c_str());
  ntpServer = strdup(ntpserver_str.c_str());
  gmtOffset_sec = gmtOffset;
  daylightOffset_sec = daylightOffset;
}

// Helper to get current config value for HTML
String html_input(const char* name, const String& value, bool isPassword=false, const char* label=nullptr, const char* note=nullptr) {
  String displayLabel = label ? String(label) : String(name);
  String input = displayLabel + ":<br><input type='";
  input += (isPassword ? "password" : "text");
  input += "' name='" + String(name) + "' value='" + (isPassword ? "****" : value) + "'> ";
  input += "<button type='submit' name='update' value='" + String(name) + "'>Update field</button>";
  if (note) {
    input += "<br><small style='color: #555;'>" + String(note) + "</small>";
  }
  input += "<br><br>";
  return input;
}

// Simple HTML form for all config variables
const char* wifi_form_html_template = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Weather Station Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: sans-serif;
      background: #f5f5f5;
      margin: 0;
      padding: 15px;
      color: #222;
    }

    .container {
      max-width: 500px;
      margin: auto;
      background: #fff;
      padding: 20px;
      border-radius: 8px;
      box-shadow: 0 1px 5px rgba(0, 0, 0, 0.1);
    }

    h2 {
      font-size: 20px;
      margin-bottom: 20px;
      text-align: center;
      color: #003344;
    }

    form {
      margin: 0;
    }

    button {
      background-color: #006699;
      color: white;
      border: none;
      padding: 8px 16px;
      border-radius: 4px;
      font-size: 14px;
      cursor: pointer;
    }

    button:hover {
      background-color: #004d73;
    }

    .btn-group {
      text-align: center;
      margin-top: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h2>Weather Station Settings</h2>

    <form action='/save' method='POST'>
      %s
    </form>

    <form action='/reboot' method='POST' class="btn-group">
      <button type='submit'>Reboot</button>
    </form>
  </div>
</body>
</html>

)rawliteral";

void handle_bday_root() {
  String name = eeprom_read_string(BDAY_NAME_ADDR, 32);
  String bday = eeprom_read_string(BDAY_DATE_ADDR, 8);
  String form = "<h2>Birthday Setup</h2>";
  form += "<form action='/bday_save' method='POST'>";
  form += "Name:<br><input type='text' name='bday_name' value='" + name + "'><br><br>";
  form += "Birthday (dd.mm):<br><input type='text' name='bday_date' value='" + bday + "'><br><br>";
  form += "<button type='submit'>Save Birthday</button>";
  form += "</form>";
  form += "<br><form action='/' method='GET'><button type='submit'>Back to Setup</button></form>";
  wifiServer.send(200, "text/html", form);
}

void handle_bday_save() {
  if (wifiServer.hasArg("bday_name") && wifiServer.hasArg("bday_date")) {
    String name = wifiServer.arg("bday_name");
    String bday = wifiServer.arg("bday_date");
    EEPROM.begin(EEPROM_SIZE); // Ensure EEPROM is initialized with enough size
    eeprom_write_string(BDAY_NAME_ADDR, name, 32);
    eeprom_write_string(BDAY_DATE_ADDR, bday, 8);
    EEPROM.commit();
    Serial.println("Birthday name saved: " + name);
    Serial.println("Birthday date saved: " + bday);

    // Read back from EEPROM to ensure fields are populated with saved values
    String saved_name = eeprom_read_string(BDAY_NAME_ADDR, 32);
    String saved_bday = eeprom_read_string(BDAY_DATE_ADDR, 8);
    String form = "<h2>Birthday Setup</h2>";
    form += "<form action='/bday_save' method='POST'>";
    form += "Name:<br><input type='text' name='bday_name' value='" + saved_name + "'><br><br>";
    form += "Birthday (dd.mm):<br><input type='text' name='bday_date' value='" + saved_bday + "'><br><br>";
    form += "<button type='submit'>Save Birthday</button>";
    form += "</form>";
    form += "<br><form action='/' method='GET'><button type='submit'>Back to Setup</button></form>";
    wifiServer.send(200, "text/html", form);
  } else {
    wifiServer.send(400, "text/html", "<h2>Missing name or birthday</h2>");
  }
}

void handle_wifi_root() {
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>WiFi Settings</legend>";
  form += html_input("ssid", eeprom_read_string(SSID_ADDR, 64));
  form += html_input("pass", "", true);
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Geo Settings</legend>";
  form += html_input("lat", eeprom_read_string(LAT_ADDR, 32));
  form += html_input("lon", eeprom_read_string(LON_ADDR, 32));
  form += html_input("city", eeprom_read_string(CITY_ADDR, 32));
  form += html_input("country", eeprom_read_string(COUNTRY_ADDR, 8));
  form += html_input("hemisphere", eeprom_read_string(HEMISPHERE_ADDR, 8));
  form += html_input("timezone", eeprom_read_string(TIMEZONE_ADDR, 32), false, "timezone", "See <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank'>https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv</a>");
  form += html_input("gmtoffset", String(EEPROM.read(GMTOFFSET_ADDR) | (EEPROM.read(GMTOFFSET_ADDR+1)<<8) | (EEPROM.read(GMTOFFSET_ADDR+2)<<16) | (EEPROM.read(GMTOFFSET_ADDR+3)<<24)));
  form += html_input("daylight", String(EEPROM.read(DAYLIGHT_ADDR) | (EEPROM.read(DAYLIGHT_ADDR+1)<<8) | (EEPROM.read(DAYLIGHT_ADDR+2)<<16) | (EEPROM.read(DAYLIGHT_ADDR+3)<<24)));
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Openweathermap 2.5 API key</legend>";
  form += html_input("apikey", eeprom_read_string(APIKEY_ADDR, 64), false, "apikey", "See <a href='https://home.openweathermap.org' target='_blank'> https://home.openweathermap.org/api_keys</a>");
  form += "</fieldset>";
  char html[4096];
  snprintf(html, sizeof(html), wifi_form_html_template, form.c_str());
  wifiServer.send(200, "text/html", html);
}

void handle_wifi_reboot() {
  wifiServer.send(200, "text/html", "<h2>Rebooting...</h2>");
  delay(1000);
  ESP.restart();
}

void handle_wifi_refresh() {
  wifiServer.sendHeader("Location", "/", true);
  wifiServer.send(302, "text/plain", "");
}

void handle_wifi_save() {
  if (wifiServer.hasArg("update")) {
    String field = wifiServer.arg("update");
    Serial.println("Update requested for field: " + field);
    if (field == "ssid" && wifiServer.hasArg("ssid")) {
      Serial.println("Saving SSID: " + wifiServer.arg("ssid"));
      eeprom_write_string(SSID_ADDR, wifiServer.arg("ssid"), 64);
    }
    if (field == "pass" && wifiServer.hasArg("pass")) {
      Serial.println("Saving password: " + wifiServer.arg("pass"));
      eeprom_write_string(PASS_ADDR, wifiServer.arg("pass"), 64);
    }
    if (field == "apikey" && wifiServer.hasArg("apikey")) {
      Serial.println("Saving API key: " + wifiServer.arg("apikey"));
      eeprom_write_string(APIKEY_ADDR, wifiServer.arg("apikey"), 64);
    }
    if (field == "lat" && wifiServer.hasArg("lat")) {
      Serial.println("Saving Latitude: " + wifiServer.arg("lat"));
      eeprom_write_string(LAT_ADDR, wifiServer.arg("lat"), 32);
    }
    if (field == "lon" && wifiServer.hasArg("lon")) {
      Serial.println("Saving Longitude: " + wifiServer.arg("lon"));
      eeprom_write_string(LON_ADDR, wifiServer.arg("lon"), 32);
    }
    if (field == "city" && wifiServer.hasArg("city")) {
      Serial.println("Saving City: " + wifiServer.arg("city"));
      eeprom_write_string(CITY_ADDR, wifiServer.arg("city"), 32);
    }
    if (field == "country" && wifiServer.hasArg("country")) {
      Serial.println("Saving Country: " + wifiServer.arg("country"));
      eeprom_write_string(COUNTRY_ADDR, wifiServer.arg("country"), 8);
    }
    if (field == "hemisphere" && wifiServer.hasArg("hemisphere")) {
      Serial.println("Saving Hemisphere: " + wifiServer.arg("hemisphere"));
      eeprom_write_string(HEMISPHERE_ADDR, wifiServer.arg("hemisphere"), 8);
    }
    if (field == "timezone" && wifiServer.hasArg("timezone")) {
      Serial.println("Saving Timezone: " + wifiServer.arg("timezone"));
      eeprom_write_string(TIMEZONE_ADDR, wifiServer.arg("timezone"), 32);
    }
    if (field == "gmtoffset" && wifiServer.hasArg("gmtoffset")) {
      long gmtOffset = wifiServer.arg("gmtoffset").toInt();
      Serial.println("Saving GMT Offset: " + String(gmtOffset));
      EEPROM.write(GMTOFFSET_ADDR, (uint8_t)((gmtOffset >> 0) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR+1, (uint8_t)((gmtOffset >> 8) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR+2, (uint8_t)((gmtOffset >> 16) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR+3, (uint8_t)((gmtOffset >> 24) & 0xFF));
    }
    if (field == "daylight" && wifiServer.hasArg("daylight")) {
      int daylightOffset = wifiServer.arg("daylight").toInt();
      Serial.println("Saving Daylight Offset: " + String(daylightOffset));
      EEPROM.write(DAYLIGHT_ADDR, (uint8_t)((daylightOffset >> 0) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR+1, (uint8_t)((daylightOffset >> 8) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR+2, (uint8_t)((daylightOffset >> 16) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR+3, (uint8_t)((daylightOffset >> 24) & 0xFF));
    }
    EEPROM.commit();
    Serial.println("EEPROM commit done. Redirecting to root page.");
    wifiServer.sendHeader("Location", "/", true);
    wifiServer.send(302, "text/plain", ""); // Redirect to root page
  } else {
    Serial.println("No field selected for update.");
    wifiServer.send(400, "text/html", "<h2>No field selected for update</h2>");
  }
}

void run_wifi_setup_portal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("weather_station_wifi");
  delay(500);
  wifiServer.on("/", handle_wifi_root);
  wifiServer.on("/save", HTTP_POST, handle_wifi_save);
  wifiServer.on("/reboot", HTTP_POST, handle_wifi_reboot);
  wifiServer.on("/refresh", HTTP_POST, handle_wifi_refresh);
  wifiServer.on("/bday", HTTP_GET, handle_bday_root);
  wifiServer.on("/bday_save", HTTP_POST, handle_bday_save);
  wifiServer.begin();
  Serial.println("WiFi setup portal started. Connect to 'weather_station_wifi' and open http://192.168.4.1/");
  while (true) {
    wifiServer.handleClient();
    delay(10);
  }
}
