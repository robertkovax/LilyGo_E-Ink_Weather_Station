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

// variables to set as defaults in EEPROM (these can be updated via the setup webpage)
// wifi credentials
char ssid[64] = "";
char password[64] = "";
char MAC[18] = "";
uint32_t device_id;
// API Key
char apikey[64] = ""; // Use your own API key by signing up for a free developer account at https://openweathermap.org/
// Location data
char LAT[32] = "";
char LON[32] = "";
char Units[8] = "M";         // M = metric, else imperial
char Location_name[32] = ""; // only for display purpose
// Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
char Timezone[32] = "CET-1CEST,M3.5.0,M10.5.0/3"; // central EU
uint32_t gmtOffset_hour = 1;                           //  in hous
uint32_t daylightOffset_hour = 1;                      // in hours

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

void eeprom_write_u32(int address, uint32_t value)
{
  for (int i = 0; i < 4; i++)
  {
    EEPROM.write(address + i, (uint8_t)((value >> (8 * i)) & 0xFF));
  }
}

uint32_t eeprom_read_u32(int address)
{
  uint32_t value = 0;
  for (int i = 0; i < 4; i++)
  {
    value |= ((uint32_t)EEPROM.read(address + i)) << (8 * i);
  }
  return value;
}

void eeprom_commit(){
  EEPROM.commit();
}

uint32_t mac_hash32(const uint8_t _mac[6])
{
  uint32_t hash = 2166136261u; // FNV-1a offset basis
  const uint32_t prime = 16777619u;
  for (int i = 0; i < 6; i++)
  {
    hash ^= _mac[i];
    hash *= prime;
  }
  return hash;
}

// Load config from EEPROM or owm_credentials.h
void load_config()
{
  EEPROM.begin(EEPROM_SIZE);

  // if EEPROM empty => initialize from program variables
  bool eeprom_initialized = (EEPROM.read(EEPROM_MARKER_ADDR) == EEPROM_MARKER_VALUE);
  if (!eeprom_initialized)
  {
    // first fill all with 00
    erase_eeprom(EEPROM_SIZE, 0x00); 
    
    // get hardware MAC address as default
    uint8_t mac[6];
    WiFi.mode(WIFI_STA);
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char mac_chr[18]; // "AA:BB:CC:DD:EE:FF" + null
    snprintf(mac_chr, sizeof(mac_chr),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    eeprom_write_string(MAC_ADDR, String(mac_chr), sizeof(mac_chr));
    device_id = mac_hash32(mac);
    eeprom_write_u32(DEVICE_ID_ADDR, device_id);
    eeprom_write_string(SSID_ADDR, String(::ssid), sizeof(ssid));
    eeprom_write_string(PASS_ADDR, String(::password), sizeof(password));
    eeprom_write_string(APIKEY_ADDR, String(::apikey), sizeof(apikey));
    eeprom_write_string(LAT_ADDR, String(::LAT), sizeof(LAT));
    eeprom_write_string(LON_ADDR, String(::LON), sizeof(LON));
    eeprom_write_string(LOCATION_ADDR, String(::Location_name), sizeof(Location_name));
    eeprom_write_string(UNITS_ADDR, String(::Units), sizeof(Units));
    eeprom_write_string(TIMEZONE_ADDR, String(::Timezone), sizeof(Timezone));
    eeprom_write_u32(GMTOFFSET_ADDR, gmtOffset_hour);
    eeprom_write_u32(DAYLIGHT_ADDR, daylightOffset_hour);
    eeprom_write_u32(SLEEPDURATION_ADDR, SleepDurationPreset);
    EEPROM.write(EEPROM_MARKER_ADDR, EEPROM_MARKER_VALUE); // Set marker
    EEPROM.commit();
  }
  // Load all config from EEPROM
  String mac_str = eeprom_read_string(MAC_ADDR, sizeof(MAC));
  mac_str.toCharArray(MAC, sizeof(MAC));
  String ssid_str = eeprom_read_string(SSID_ADDR, sizeof(ssid));
  ssid_str.toCharArray(ssid, sizeof(ssid));
  String pass_str = eeprom_read_string(PASS_ADDR, sizeof(password));
  pass_str.toCharArray(password, sizeof(password));
  String apikey_str = eeprom_read_string(APIKEY_ADDR, sizeof(apikey));
  apikey_str.toCharArray(apikey, sizeof(apikey));
  String lat_str = eeprom_read_string(LAT_ADDR, sizeof(LAT));
  lat_str.toCharArray(LAT, sizeof(LAT));
  String lon_str = eeprom_read_string(LON_ADDR, sizeof(LON));
  lon_str.toCharArray(LON, sizeof(LON));
  String location_str = eeprom_read_string(LOCATION_ADDR, sizeof(Location_name));
  location_str.toCharArray(Location_name, sizeof(Location_name));
  String units_str = eeprom_read_string(UNITS_ADDR, sizeof(Units));
  units_str.toCharArray(Units, sizeof(Units));
  String timezone_str = eeprom_read_string(TIMEZONE_ADDR, sizeof(Timezone));
  timezone_str.toCharArray(Timezone, sizeof(Timezone));
  gmtOffset_hour = eeprom_read_u32(GMTOFFSET_ADDR);
  daylightOffset_hour = eeprom_read_u32(DAYLIGHT_ADDR);
  SleepDurationPreset = eeprom_read_u32(SLEEPDURATION_ADDR);
  device_id = eeprom_read_u32(DEVICE_ID_ADDR);
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
    /* Footer */
    .footer {
      margin-top: 18px;
      padding-top: 10px;
      border-top: 1px solid #e5e5e5;
      font-size: 12px;
      color: #666;
      text-align: center;
      line-height: 1.4;
    }
    .footer a {
      color: #003344;
      text-decoration: none;
      border-bottom: 1px dotted rgba(0, 51, 68, 0.4);
      transition: color 0.2s ease, border-color 0.2s ease;
      overflow-wrap: anywhere;
    }
    .footer a:hover {
      color: #003344;
      border-bottom-color: currentColor;
      text-decoration: underline;
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
    <div class="footer">
      Station ID: __STATION_ID__ 
      <br>Firmware v__FW_VERSION__
      <br>&copy; 2025 Robert Kovacs (<a href="https://www.robertkovax.com" target="_blank">www.robertkovax.com</a>)
      <br>Source: <a href="https://github.com/robertkovax/LilyGo_E-Ink_Weather_Station" target="_blank">https://github.com/robertkovax/LilyGo_E-Ink_Weather_Station</a>
      
    </div>
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
  html.replace("__STATION_ID__", String(device_id));
  html.replace("__FW_VERSION__", String(fw_version_major) + "." + String(fw_version_minor));
  wifiServer.send(200, "text/html", html);
}

void handle_wifi_root()
{
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>WiFi</legend>";
  String ssid_val = eeprom_read_string(SSID_ADDR, sizeof(ssid));
  String mac_str = eeprom_read_string(MAC_ADDR, sizeof(MAC));
  form += html_input("ssid", ssid_val, false, nullptr, nullptr);
  form += html_input("pass", "", true, nullptr, nullptr);
  form += html_input("MAC address", mac_str, false, "MAC address", "e.g. 96:e1:33:e9:02:f4, (default / empty = hardware MAC)");
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Location</legend>";
  String lat_val = eeprom_read_string(LAT_ADDR, sizeof(LAT));
  String lon_val = eeprom_read_string(LON_ADDR, sizeof(LON));
  String location_val = eeprom_read_string(LOCATION_ADDR, sizeof(Location_name));
  String units_val = eeprom_read_string(UNITS_ADDR, sizeof(Units));
  String timezone_val = eeprom_read_string(TIMEZONE_ADDR, sizeof(Timezone));
  String gmtoffset_val = String(eeprom_read_u32(GMTOFFSET_ADDR));
  String daylight_val = String(eeprom_read_u32(DAYLIGHT_ADDR));
  String sleepduration_val = String(eeprom_read_u32(SLEEPDURATION_ADDR));
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
  String apikey_val = eeprom_read_string(APIKEY_ADDR, sizeof(apikey));
  form += html_input("apikey", apikey_val, false, "apikey 2.5", "register for free api key at: <a href='https://home.openweathermap.org' target='_blank'> https://home.openweathermap.org</a>");
  form += "</fieldset>";
  String html = String(wifi_form_html_template);
  html.replace("%s", form);
  html.replace("__STATION_ID__", String(device_id));
  html.replace("__FW_VERSION__", String(fw_version_major) + "." + String(fw_version_minor));
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
      eeprom_write_string(SSID_ADDR, wifiServer.arg("ssid"), sizeof(ssid));
    }
    if (field == "pass" && wifiServer.hasArg("pass"))
    {
      Serial.println("Saving password: " + wifiServer.arg("pass"));
      eeprom_write_string(PASS_ADDR, wifiServer.arg("pass"), sizeof(password));
    }
    if (field == "MAC address" && wifiServer.hasArg("MAC address"))
    {
      Serial.println("Saving MAC: " + wifiServer.arg("MAC address"));
      eeprom_write_string(MAC_ADDR, wifiServer.arg("MAC address"), sizeof(MAC));
    }
    if (field == "apikey" && wifiServer.hasArg("apikey"))
    {
      Serial.println("Saving API key: " + wifiServer.arg("apikey"));
      eeprom_write_string(APIKEY_ADDR, wifiServer.arg("apikey"), sizeof(apikey));
    }
    if (field == "lat" && wifiServer.hasArg("lat"))
    {
      Serial.println("Saving Latitude: " + wifiServer.arg("lat"));
      eeprom_write_string(LAT_ADDR, wifiServer.arg("lat"), sizeof(LAT));
    }
    if (field == "lon" && wifiServer.hasArg("lon"))
    {
      Serial.println("Saving Longitude: " + wifiServer.arg("lon"));
      eeprom_write_string(LON_ADDR, wifiServer.arg("lon"), sizeof(LON));
    }
    if (field == "location" && wifiServer.hasArg("location"))
    {
      Serial.println("Saving location: " + wifiServer.arg("location"));
      eeprom_write_string(LOCATION_ADDR, wifiServer.arg("location"), sizeof(Location_name));
    }
    if (field == "units" && wifiServer.hasArg("units"))
    {
      Serial.println("Saving Units: " + wifiServer.arg("units"));
      eeprom_write_string(UNITS_ADDR, wifiServer.arg("units"), sizeof(Units));
    }
    if (field == "timezone" && wifiServer.hasArg("timezone"))
    {
      Serial.println("Saving Timezone: " + wifiServer.arg("timezone"));
      eeprom_write_string(TIMEZONE_ADDR, wifiServer.arg("timezone"), sizeof(Timezone));
    }
    if (field == "gmtoffset" && wifiServer.hasArg("gmtoffset"))
    {
      long gmtOffset = wifiServer.arg("gmtoffset").toInt();
      Serial.println("Saving GMT Offset: " + String(gmtOffset));
      eeprom_write_u32(GMTOFFSET_ADDR, gmtOffset);
    }
    if (field == "daylight" && wifiServer.hasArg("daylight"))
    {
      int daylightOffset = wifiServer.arg("daylight").toInt();
      Serial.println("Saving Daylight Offset: " + String(daylightOffset));
      eeprom_write_u32(DAYLIGHT_ADDR, daylightOffset);
    }
    if (field == "sleepduration" && wifiServer.hasArg("sleepduration"))
    {
      long sleepduration = wifiServer.arg("sleepduration").toInt();
      Serial.println("Saving Sleep Duration: " + String(sleepduration));
      eeprom_write_u32(SLEEPDURATION_ADDR, sleepduration);
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
void run_wifi_setup_portal(uint32_t timeoutMinutes)
{
  WiFi.mode(WIFI_AP);
  WiFi.softAP("weather_station_wifi");
  delay(300);

  const uint32_t timeoutMs = timeoutMinutes * 60000UL; // minutes → ms safely
  // Helper: extend deadline on any activity
  auto nowMs = []() -> uint32_t
  { return millis(); };
  uint32_t deadline = nowMs() + timeoutMs;
  auto touch = [&]()
  { deadline = nowMs() + timeoutMs; };

  // Touch on every handler
  wifiServer.on("/", HTTP_GET, [&]()
                { touch(); handle_wifi_root(); });
  wifiServer.on("/save", HTTP_POST, [&]()
                { touch(); handle_wifi_save(); });
  wifiServer.on("/reboot", HTTP_POST, [&]()
                { touch(); handle_wifi_reboot(); });
  wifiServer.on("/refresh", HTTP_POST, [&]()
                { touch(); handle_wifi_refresh(); });
  wifiServer.on("/popups", HTTP_GET, [&]()
                { touch(); handle_popups_root(); });
  wifiServer.on("/erase_eeprom", HTTP_GET, [&]()
                { touch(); handle_erase_eeprom(); });
  wifiServer.onNotFound([&]()
                        { touch(); wifiServer.send(404, "text/plain", "Not found"); });

  wifiServer.begin();
  Serial.printf("Portal up. Timeout: %lu min\n", (unsigned long)timeoutMinutes);

  uint32_t lastAssocCheck = 0;
  bool firstAssocTouched = false;
  for (;;)
  {
    wifiServer.handleClient();
    uint32_t now = nowMs();
    // Treat first association as "activity" (checked ~4×/s)
    if ((uint32_t)(now - lastAssocCheck) >= 250UL)
    {
      lastAssocCheck = now;
      if (WiFi.softAPgetStationNum() > 0 && !firstAssocTouched)
      {
        touch();
        firstAssocTouched = true;
        Serial.println("Client associated.");
      }
    }
    // Deadline reached?
    if ((int32_t)(now - deadline) >= 0)
    {
      Serial.printf("%lu-minute timeout reached. Closing portal.\n", (unsigned long)timeoutMinutes);
      break;
    }
    delay(100); // yield to WiFi/RTOS
  }
  wifiServer.stop();
  delay(20);
  WiFi.softAPdisconnect(true);
  delay(20);
}

// #########################################################################################
