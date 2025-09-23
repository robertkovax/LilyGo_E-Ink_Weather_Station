// The stored variables are loaded from EEPROM at startup (and initialized from the program variables in case the eeprom is erased)
// It provides a web interface for setting up the WiFi credentials, OpenWeatherMap API key, location data, refresh period, etc.

// Functions:
// Setup: http://192.168.4.1
// Popup messages: http://192.168.4.1/popups (e.g. birthday greeting that occur every year)
// Erase EEPROM: http://192.168.4.1/erase_eeprom
// Reboot: http://192.168.4.1/reboot

#include "setup_server.h"
#include <EEPROM.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <WebServer.h>

WebServer wifiServer(80);

// time setup for the ESP32 internal clock (the weather data already contains the timestamps)
// best to use pool.ntp.org to find an NTP server then the NTP system tries to find the closest available servers
// EU "0.europe.pool.ntp.org"
// US "0.north-america.pool.ntp.org"
// See: https://www.ntppool.org/en/
char ntpServer[32] = "pool.ntp.org";

// weather server
// example calls: https://api.openweathermap.org/data/2.5/weather?lat=52.50&lon=13.40&appid=6e3ebdb485176f42f2c77ac171f89677&mode=json&units=metric&lang=EN
//                https://api.openweathermap.org/data/2.5/forecast?lat=52.50&lon=13.40&appid=6e3ebdb485176f42f2c77ac171f89677&mode=json&units=metric&lang=EN
char weatherServer[] = "api.openweathermap.org";

// --------------------------------- variable to set as defaults in EEPROM -------------------------------------------
// -----------------------------(these can be updated via the setup webpage) ---------------------------------

// wifi credentials
char ssid[64] = "";
char password[64] = "";
char MAC[32] = "";
// API Key
char apikey[64] = ""; // Use your own API key by signing up for a free developer account at https://openweathermap.org/
// Location data
char LAT[32] = "";
char LON[32] = "";
char Units[8] = "M";         // M = metric, else imperial
char Location_name[32] = ""; // only for display purpose
// Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
char Timezone[32] = "CET-1CEST,M3.5.0,M10.5.0/3"; // central EU
int gmtOffset_hour = 1;                           //  in hous
int daylightOffset_hour = 1;                      // in hours

// ----------------------------------------------- defaults end -----------------------------------------------------

// Helper to read/write String to EEPROM
void eeprom_write_string(int addr, const String &value, int maxlen)
{
  for (int i = 0; i < maxlen; i++)
  {
    EEPROM.write(addr + i, i < value.length() ? value[i] : 0);
  }
}
String eeprom_read_string(int addr, int maxlen)
{
  char buf[128] = {0};
  EEPROM.readBytes(addr, buf, maxlen - 1);
  buf[maxlen - 1] = 0;
  String s = String(buf);
  s.trim();
  return s;
}

// Load config from EEPROM or owm_credentials.h
void load_config()
{
  EEPROM.begin(EEPROM_SIZE);

  // if EEPROM empty => initialize from program variables
  bool eeprom_initialized = (EEPROM.read(EEPROM_MARKER_ADDR) == EEPROM_MARKER_VALUE);
  if (!eeprom_initialized)
  {
    erase_eeprom(EEPROM_SIZE, 0x00); // first fill with 00
                                     // get hardware MAC address as default
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_str[18]; // "AA:BB:CC:DD:EE:FF" + null
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    eeprom_write_string(MAC_ADDR, String(mac_str), 32);
    eeprom_write_string(SSID_ADDR, String(::ssid), 64);
    eeprom_write_string(PASS_ADDR, String(::password), 64);
    eeprom_write_string(APIKEY_ADDR, String(::apikey), 64);
    eeprom_write_string(LAT_ADDR, String(::LAT), 32);
    eeprom_write_string(LON_ADDR, String(::LON), 32);
    eeprom_write_string(LOCATION_ADDR, String(::Location_name), 32);
    eeprom_write_string(UNITS_ADDR, String(::Units), 8);
    eeprom_write_string(TIMEZONE_ADDR, String(::Timezone), 32);
    EEPROM.write(GMTOFFSET_ADDR, (uint8_t)((::gmtOffset_hour >> 0) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR + 1, (uint8_t)((::gmtOffset_hour >> 8) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR + 2, (uint8_t)((::gmtOffset_hour >> 16) & 0xFF));
    EEPROM.write(GMTOFFSET_ADDR + 3, (uint8_t)((::gmtOffset_hour >> 24) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR, (uint8_t)((::daylightOffset_hour >> 0) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR + 1, (uint8_t)((::daylightOffset_hour >> 8) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR + 2, (uint8_t)((::daylightOffset_hour >> 16) & 0xFF));
    EEPROM.write(DAYLIGHT_ADDR + 3, (uint8_t)((::daylightOffset_hour >> 24) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR, (uint8_t)((SleepDurationPreset >> 0) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR + 1, (uint8_t)((SleepDurationPreset >> 8) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR + 2, (uint8_t)((SleepDurationPreset >> 16) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR + 3, (uint8_t)((SleepDurationPreset >> 24) & 0xFF));
    EEPROM.write(EEPROM_MARKER_ADDR, EEPROM_MARKER_VALUE); // Set marker
    EEPROM.commit();
  }
  // Load all config from EEPROM
  String mac_str = eeprom_read_string(MAC_ADDR, 32);
  mac_str.toCharArray(MAC, sizeof(MAC));
  String ssid_str = eeprom_read_string(SSID_ADDR, 64);
  ssid_str.toCharArray(ssid, sizeof(ssid));
  String pass_str = eeprom_read_string(PASS_ADDR, 64);
  pass_str.toCharArray(password, sizeof(password));
  String apikey_str = eeprom_read_string(APIKEY_ADDR, 64);
  apikey_str.toCharArray(apikey, sizeof(apikey));
  String lat_str = eeprom_read_string(LAT_ADDR, 32);
  lat_str.toCharArray(LAT, sizeof(LAT));
  String lon_str = eeprom_read_string(LON_ADDR, 32);
  lon_str.toCharArray(LON, sizeof(LON));
  String location_str = eeprom_read_string(LOCATION_ADDR, 32);
  location_str.toCharArray(Location_name, sizeof(Location_name));
  String units_str = eeprom_read_string(UNITS_ADDR, 8);
  units_str.toCharArray(Units, sizeof(Units));
  String timezone_str = eeprom_read_string(TIMEZONE_ADDR, 32);
  timezone_str.toCharArray(Timezone, sizeof(Timezone));
  gmtOffset_hour = EEPROM.read(GMTOFFSET_ADDR) | (EEPROM.read(GMTOFFSET_ADDR + 1) << 8) | (EEPROM.read(GMTOFFSET_ADDR + 2) << 16) | (EEPROM.read(GMTOFFSET_ADDR + 3) << 24);
  daylightOffset_hour = EEPROM.read(DAYLIGHT_ADDR) | (EEPROM.read(DAYLIGHT_ADDR + 1) << 8) | (EEPROM.read(DAYLIGHT_ADDR + 2) << 16) | (EEPROM.read(DAYLIGHT_ADDR + 3) << 24);
  SleepDurationPreset = EEPROM.read(SLEEPDURATION_ADDR) | (EEPROM.read(SLEEPDURATION_ADDR + 1) << 8) | (EEPROM.read(SLEEPDURATION_ADDR + 2) << 16) | (EEPROM.read(SLEEPDURATION_ADDR + 3) << 24);
}

// Helper to get current config value for HTML
String html_input(const char *name, const String &value, bool isPassword = false, const char *label = nullptr, const char *note = nullptr)
{
  String displayLabel = label ? String(label) : String(name);
  String input = "<form class='field-form' action='/save' method='POST'>";
  input += "<span class='field-label'>" + displayLabel + ":</span>";
  input += "<div class='field-row'>";
  input += "<input type='";
  input += (isPassword ? "password" : "text");
  input += "' name='" + String(name) + "' value='" + (isPassword ? "****" : value) + "' data-original='" + value + "' oninput='detectChange(this)'>";
  input += "<button type='submit' name='update' value='" + String(name) + "'>Save field</button>";
  input += "</div>";
  if (note)
  {
    input += "<span class='field-note'>" + String(note) + "</span>";
  }
  input += "</form>";
  return input;
}

const char *wifi_form_html_template = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Weather Station Setup</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html, body {
      margin: 0;
      padding: 0;
      background: #f5f5f5;
      font-family: sans-serif;
      color: #222;
    }
    .container {
      max-width: 420px;
      margin: 20px auto;
      background: #fff;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
    }
    h2 {
      font-size: 20px;
      margin-bottom: 20px;
      text-align: center;
      color: #003344;
    }
    /* Field Form */
    form.field-form {
      margin-bottom: 16px;
    }
    .field-label {
      font-size: 14px;
      margin-bottom: 4px;
      display: block;
      color: #003344;
    }
    .field-row {
      display: flex;
      align-items: center;
      gap: 6px;
    }
    input[type="text"], input[type="password"] {
      flex: 1;
      padding: 6px;
      border-radius: 4px;
      border: 1px solid #ccc;
      font-size: 14px;
    }
    button {
      background-color: #003344;
      color: white;
      border: none;
      padding: 7px 12px;
      border-radius: 4px;
      font-size: 13px;
      cursor: pointer;
      transition: background 0.3s;
      white-space: nowrap;
    }
    .field-note {
      font-size: 12px;
      color: #555;
      margin-top: 4px;
      display: block;
    }
    /* Bottom Buttons */
    .btn-group {
      text-align: center;
      margin-top: 14px;
      display: block;
    }
    .btn-group button {
      width: 100%;
      margin-bottom: 10px;
    }
  </style>
  <script>
    function detectChange(input, userTyped = false) {
      var form = input.form;
      var btn = form.querySelector('button[type="submit"]');
      var changed = false;
      var inputs = form.querySelectorAll('input');
      inputs.forEach(function(inp) {
        var original = inp.getAttribute('data-original') || '';
        var current = inp.value;
        // For password fields, only detect change when user typed
        if (inp.type === 'password' && !userTyped) return;
        if (current !== original) changed = true;
      });
      if (changed) {
        btn.style.backgroundColor = '#ff9800'; // changed → orange
        btn._changed = true;
      } else {
        btn.style.backgroundColor = '#003344'; // default → dark blue
        btn._changed = false;
      }
    }

    function saveField(btn, form) {
      var xhr = new XMLHttpRequest();
      xhr.open('POST', '/save', true);
      xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
      xhr.onreadystatechange = function() {
        if (xhr.readyState === 4 && xhr.status === 204) {
          btn.style.backgroundColor = '#003344'; // reset after save
          btn._changed = false;
          var input = form.querySelector('input');
          if (input) {
            input.setAttribute('data-original', input.value);
          }
        }
      };
      var input = form.querySelector('input');
      var data = '';
      if (input) {
        data = encodeURIComponent(input.name) + '=' + encodeURIComponent(input.value) + '&update=' + encodeURIComponent(input.name);
      }
      xhr.send(data);
      return false;
    }

    window.onload = function() {
      var forms = document.querySelectorAll('.field-form');
      forms.forEach(function(form) {
        var input = form.querySelector('input');
        var isPassword = input && input.type === 'password';

        if (input) {
          input.addEventListener('input', function() {
            detectChange(input, true);
          });
          detectChange(input, !isPassword);
        }

        form.onsubmit = function(e) {
          e.preventDefault();
          var btn = form.querySelector('button[type="submit"]');
          saveField(btn, form);
        };
      });
    };
  </script>
</head>
<body>
  <div class="container">
    <h2>Weather Station Settings</h2>
    %s
    <form action='/reboot' method='POST' class="btn-group">
      <button type='submit'>Apply and Reboot</button>
    </form>
  </div>
</body>
</html>
)rawliteral";

void handle_popups_root()
{
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Setup Popup Messages</legend>";
  for (int i = 0; i < 4; i++)
  {
    String message = eeprom_read_string((i == 0) ? POPUP1_MSG_ADDR : (i == 1) ? POPUP2_MSG_ADDR
                                                                 : (i == 2)   ? POPUP3_MSG_ADDR
                                                                              : POPUP4_MSG_ADDR,
                                        48);
    String date = eeprom_read_string((i == 0) ? POPUP1_DATE_ADDR : (i == 1) ? POPUP2_DATE_ADDR
                                                               : (i == 2)   ? POPUP3_DATE_ADDR
                                                                            : POPUP4_DATE_ADDR,
                                     8);

    form += "<div style='margin-bottom:24px; padding-bottom:12px; border-bottom:1px solid #ddd;'>";

    // message field
    form += html_input(String("popup_msg" + String(i + 1)).c_str(), message, false,
                       String("popup message " + String(i + 1)).c_str(), "");

    // date field
    form += html_input(String("popup_date" + String(i + 1)).c_str(), date, false,
                       String("on date ").c_str(), "(dd.mm, e.g. 24.12)");

    form += "</div>";
  }

  // Counter script: puts a block counter under each message input and updates it in real time
  form += "<script>"
          "document.addEventListener('DOMContentLoaded',function(){"
          "var sels=\"input[name^='popup_msg'],input[id^='popup_msg']\";"
          "document.querySelectorAll(sels).forEach(function(el){"
          "el.setAttribute('maxlength','45');"
          "var counter=document.createElement('div');"
          "counter.className='character-counter';"
          "counter.style.marginTop='4px';"
          "counter.style.fontSize='0.85em';"
          "counter.style.color='#666';"       // softer gray text
          "counter.style.fontStyle='italic';" // matches the (e.g. ...) hint style
          // place under the entire field row
          "var parent=el.parentElement;"
          "var isFlex=parent && getComputedStyle(parent).display.indexOf('flex')>-1;"
          "if(isFlex){"
          "parent.insertAdjacentElement('afterend',counter);"
          "}else{"
          "el.insertAdjacentElement('afterend',counter);"
          "}"
          "var update=function(){ counter.textContent=(el.value||'').length + '/45 (use \\\\n for line break)'; };"
          "['input','keyup','change'].forEach(function(ev){ el.addEventListener(ev,update); });"
          "update();"
          "});"
          "});"
          "</script>";

  form += "<form action='/' method='GET' class='btn-group'><button type='submit'>Back to Setup</button></form>";
  String html = String(wifi_form_html_template);
  html.replace("%s", form);
  wifiServer.send(200, "text/html", html);
}

void handle_wifi_root()
{
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>WiFi</legend>";
  String ssid_val = eeprom_read_string(SSID_ADDR, 64);
  String mac_str = eeprom_read_string(MAC_ADDR, 32);
  form += html_input("ssid", ssid_val, false, nullptr, nullptr);
  form += html_input("pass", "", true, nullptr, nullptr);
  form += html_input("MAC address", mac_str, false, "device MAC address", "e.g. 96:e1:33:e9:02:f4, default = hardware MAC (empty = default)");
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Location</legend>";
  String lat_val = eeprom_read_string(LAT_ADDR, 32);
  String lon_val = eeprom_read_string(LON_ADDR, 32);
  String location_val = eeprom_read_string(LOCATION_ADDR, 32);
  String units_val = eeprom_read_string(UNITS_ADDR, 8);
  String timezone_val = eeprom_read_string(TIMEZONE_ADDR, 32);
  String gmtoffset_val = String(EEPROM.read(GMTOFFSET_ADDR) | (EEPROM.read(GMTOFFSET_ADDR + 1) << 8) | (EEPROM.read(GMTOFFSET_ADDR + 2) << 16) | (EEPROM.read(GMTOFFSET_ADDR + 3) << 24));
  String daylight_val = String(EEPROM.read(DAYLIGHT_ADDR) | (EEPROM.read(DAYLIGHT_ADDR + 1) << 8) | (EEPROM.read(DAYLIGHT_ADDR + 2) << 16) | (EEPROM.read(DAYLIGHT_ADDR + 3) << 24));
  String sleepduration_val = String(EEPROM.read(SLEEPDURATION_ADDR) | (EEPROM.read(SLEEPDURATION_ADDR + 1) << 8) | (EEPROM.read(SLEEPDURATION_ADDR + 2) << 16) | (EEPROM.read(SLEEPDURATION_ADDR + 3) << 24));
  form += html_input("lat", lat_val, false, "LAT", nullptr);
  form += html_input("lon", lon_val, false, "LON", nullptr);
  form += html_input("location", location_val, false, "location name", " the name of the place to display");
  form += html_input("units", units_val, false, nullptr, "M - metric (Celsius), I - imperial (Fahrenheit)");
  form += html_input("timezone", timezone_val, false, "time zone", "see: <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank'>https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv</a>");
  form += html_input("gmtoffset", gmtoffset_val, false, "GMT offset [h]", "(e.g. -8 for GMT-8)");
  form += html_input("daylight", daylight_val, false, "daylight saving offset [h]", "(e.g. 1 for 1 hour, 0 if not used)");
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Refresh period</legend>";
  form += html_input("sleepduration", sleepduration_val, false, "update every [min]", "(default: 60 min)");
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>OpenWeatherMap API</legend>";
  String apikey_val = eeprom_read_string(APIKEY_ADDR, 64);
  form += html_input("apikey", apikey_val, false, "apikey 2.5", "register for free api key at: <a href='https://home.openweathermap.org' target='_blank'> https://home.openweathermap.org</a>");
  form += "</fieldset>";
  String html = String(wifi_form_html_template);
  html.replace("%s", form);
  wifiServer.send(200, "text/html", html);
}

void handle_wifi_reboot()
{
  wifiServer.send(200, "text/html", "<h2>Rebooting...</h2>");
  delay(1000);
  ESP.restart();
}

void handle_wifi_refresh()
{
  wifiServer.sendHeader("Location", "/", true);
  wifiServer.send(302, "text/plain", "");
}

void handle_wifi_save()
{
  if (wifiServer.hasArg("update"))
  {
    String field = wifiServer.arg("update");
    Serial.println("Update requested for field: " + field);
    if (field == "ssid" && wifiServer.hasArg("ssid"))
    {
      Serial.println("Saving SSID: " + wifiServer.arg("ssid"));
      eeprom_write_string(SSID_ADDR, wifiServer.arg("ssid"), 64);
    }
    if (field == "pass" && wifiServer.hasArg("pass"))
    {
      Serial.println("Saving password: " + wifiServer.arg("pass"));
      eeprom_write_string(PASS_ADDR, wifiServer.arg("pass"), 64);
    }
    if (field == "MAC address" && wifiServer.hasArg("MAC address"))
    {
      Serial.println("Saving MAC: " + wifiServer.arg("MAC address"));
      eeprom_write_string(MAC_ADDR, wifiServer.arg("MAC address"), 64);
    }
    if (field == "apikey" && wifiServer.hasArg("apikey"))
    {
      Serial.println("Saving API key: " + wifiServer.arg("apikey"));
      eeprom_write_string(APIKEY_ADDR, wifiServer.arg("apikey"), 64);
    }
    if (field == "lat" && wifiServer.hasArg("lat"))
    {
      Serial.println("Saving Latitude: " + wifiServer.arg("lat"));
      eeprom_write_string(LAT_ADDR, wifiServer.arg("lat"), 32);
    }
    if (field == "lon" && wifiServer.hasArg("lon"))
    {
      Serial.println("Saving Longitude: " + wifiServer.arg("lon"));
      eeprom_write_string(LON_ADDR, wifiServer.arg("lon"), 32);
    }
    if (field == "location" && wifiServer.hasArg("location"))
    {
      Serial.println("Saving location: " + wifiServer.arg("location"));
      eeprom_write_string(LOCATION_ADDR, wifiServer.arg("location"), 32);
    }
    if (field == "units" && wifiServer.hasArg("units"))
    {
      Serial.println("Saving Units: " + wifiServer.arg("units"));
      eeprom_write_string(UNITS_ADDR, wifiServer.arg("units"), 8);
    }
    if (field == "timezone" && wifiServer.hasArg("timezone"))
    {
      Serial.println("Saving Timezone: " + wifiServer.arg("timezone"));
      eeprom_write_string(TIMEZONE_ADDR, wifiServer.arg("timezone"), 32);
    }
    if (field == "gmtoffset" && wifiServer.hasArg("gmtoffset"))
    {
      long gmtOffset = wifiServer.arg("gmtoffset").toInt();
      Serial.println("Saving GMT Offset: " + String(gmtOffset));
      EEPROM.write(GMTOFFSET_ADDR, (uint8_t)((gmtOffset >> 0) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR + 1, (uint8_t)((gmtOffset >> 8) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR + 2, (uint8_t)((gmtOffset >> 16) & 0xFF));
      EEPROM.write(GMTOFFSET_ADDR + 3, (uint8_t)((gmtOffset >> 24) & 0xFF));
    }
    if (field == "daylight" && wifiServer.hasArg("daylight"))
    {
      int daylightOffset = wifiServer.arg("daylight").toInt();
      Serial.println("Saving Daylight Offset: " + String(daylightOffset));
      EEPROM.write(DAYLIGHT_ADDR, (uint8_t)((daylightOffset >> 0) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR + 1, (uint8_t)((daylightOffset >> 8) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR + 2, (uint8_t)((daylightOffset >> 16) & 0xFF));
      EEPROM.write(DAYLIGHT_ADDR + 3, (uint8_t)((daylightOffset >> 24) & 0xFF));
    }
    if (field == "sleepduration" && wifiServer.hasArg("sleepduration"))
    {
      long val = wifiServer.arg("sleepduration").toInt();
      Serial.println("Saving Sleep Duration: " + String(val));
      EEPROM.write(SLEEPDURATION_ADDR, (uint8_t)((val >> 0) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR + 1, (uint8_t)((val >> 8) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR + 2, (uint8_t)((val >> 16) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR + 3, (uint8_t)((val >> 24) & 0xFF));
    }
    for (int i = 0; i < 4; i++)
    {
      String msg_field = String("popup_msg" + String(i + 1));
      String date_field = String("popup_date" + String(i + 1));
      int msg_addr = (i == 0) ? POPUP1_MSG_ADDR : (i == 1) ? POPUP2_MSG_ADDR
                                              : (i == 2)   ? POPUP3_MSG_ADDR
                                                           : POPUP4_MSG_ADDR;
      int date_addr = (i == 0) ? POPUP1_DATE_ADDR : (i == 1) ? POPUP2_DATE_ADDR
                                                : (i == 2)   ? POPUP3_DATE_ADDR
                                                             : POPUP4_DATE_ADDR;
      if (field == msg_field && wifiServer.hasArg(msg_field))
      {
        Serial.println("Saving " + msg_field + ": " + wifiServer.arg(msg_field));
        eeprom_write_string(msg_addr, wifiServer.arg(msg_field), 48);
      }
      if (field == date_field && wifiServer.hasArg(date_field))
      {
        Serial.println("Saving " + date_field + ": " + wifiServer.arg(date_field));
        eeprom_write_string(date_addr, wifiServer.arg(date_field), 8);
      }
    }
    EEPROM.commit();
    Serial.println("EEPROM commit done. No page refresh.");
    wifiServer.send(204, "text/plain", "");
  }
  else
  {
    Serial.println("No field selected for update.");
    wifiServer.send(400, "text/html", "<h2>No field selected for update</h2>");
  }
}

void handle_erase_eeprom()
{
  for (int i = 0; i < EEPROM_SIZE; i++)
  {
    EEPROM.write(i, 0x00);
  }
  EEPROM.commit();
  wifiServer.send(200, "text/html", "<h2>EEPROM filled with 0x00. Please reboot the device.</h2>");
}

void erase_eeprom(int eeprom_size, byte erase_value)
{
  for (int i = 0; i < eeprom_size; i++)
  {
    EEPROM.write(i, erase_value);
  }
  EEPROM.commit();
}
// #########################################################################################
void run_wifi_setup_portal(unsigned long timeoutMinutes) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("weather_station_wifi");
  delay(300);

  unsigned long lastActivityMs = millis();

  // Shortcut lambda to bump activity
  auto touch = [&]() { lastActivityMs = millis(); };

  // Wrap handlers so they update activity
  wifiServer.on("/", HTTP_GET,         [&](){ touch(); handle_wifi_root(); });
  wifiServer.on("/save", HTTP_POST,    [&](){ touch(); handle_wifi_save(); });
  wifiServer.on("/reboot", HTTP_POST,  [&](){ touch(); handle_wifi_reboot(); });
  wifiServer.on("/refresh", HTTP_POST, [&](){ touch(); handle_wifi_refresh(); });
  wifiServer.on("/popups", HTTP_GET,   [&](){ touch(); handle_popups_root(); });
  wifiServer.on("/erase_eeprom", HTTP_GET, [&](){ touch(); handle_erase_eeprom(); });
  wifiServer.onNotFound([&](){ touch(); wifiServer.send(404, "text/plain", "Not found"); });

  wifiServer.begin();
  Serial.println("WiFi setup portal started. Connect to 'weather_station_wifi' and open http://192.168.4.1/");

  const unsigned long timeoutMs = timeoutMinutes * 60UL * 1000UL;
  unsigned long lastAssocCheck = 0;

  for (;;) {
    wifiServer.handleClient();

    unsigned long now = millis();

    // Update activity if any station is associated
    if (now - lastAssocCheck >= 250) {
      lastAssocCheck = now;
      if (WiFi.softAPgetStationNum() > 0) {
        touch();
      }
    }

    // Timeout check
    if (now - lastActivityMs >= timeoutMs) {
      Serial.printf("%lu-minute timeout reached. Closing portal.\n", timeoutMinutes);
      break;
    }

    delay(10); // yield and keep loop responsive
  }
  wifiServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}



// #########################################################################################
