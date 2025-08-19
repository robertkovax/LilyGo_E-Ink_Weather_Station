//Here are the WiFi credentials loaded from EEPROM (or owm_credentials.h at first boot after programming).
//It provides a web interface for setting up the WiFi credentials, OpenWeatherMap API key, location data, and refresh period.
//The web interface is accessible at http://192.168.4.1
//Birthday greeting setup at: at http://192.168.4.1/bday (You can set up a birthday greeting that occurs every year.)
//You can also erase the EEPROM to reset all settings to default values: http://192.168.4.1/erase_eeprom

#include "wifi_setup.h"
#include "owm_credentials.h"
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>

extern int SleepDurationPreset;

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

// Load config from EEPROM or owm_credentials.h
void load_wifi_config() {
  EEPROM.begin(EEPROM_SIZE);
  bool eeprom_initialized = (EEPROM.read(EEPROM_MARKER_ADDR) == EEPROM_MARKER_VALUE);
  if (!eeprom_initialized) {
    erase_eeprom(EEPROM_SIZE, 0x00); // Initialize EEPROM with default values
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
    EEPROM.write(SLEEPDURATION_ADDR, (uint8_t)((SleepDurationPreset >> 0) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR+1, (uint8_t)((SleepDurationPreset >> 8) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR+2, (uint8_t)((SleepDurationPreset >> 16) & 0xFF));
    EEPROM.write(SLEEPDURATION_ADDR+3, (uint8_t)((SleepDurationPreset >> 24) & 0xFF));
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
  SleepDurationPreset = EEPROM.read(SLEEPDURATION_ADDR) | (EEPROM.read(SLEEPDURATION_ADDR+1)<<8) | (EEPROM.read(SLEEPDURATION_ADDR+2)<<16) | (EEPROM.read(SLEEPDURATION_ADDR+3)<<24);
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
  String input = "<form class='field-form' action='/save' method='POST'>";
  input += "<span class='field-label'>" + displayLabel + ":</span>";
  input += "<div class='field-row'>";
  input += "<input type='";
  input += (isPassword ? "password" : "text");
  input += "' name='" + String(name) + "' value='" + (isPassword ? "****" : value) + "' data-original='" + value + "' oninput='detectChange(this)'>";
  input += "<button type='submit' name='update' value='" + String(name) + "'>Save field</button>";
  input += "</div>";
  if (note) {
    input += "<span class='field-note'>" + String(note) + "</span>";
  }
  input += "</form>";
  return input;
}

const char* wifi_form_html_template = R"rawliteral(
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


void handle_bday_root() {
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Setup birthday greetings</legend>";
  for (int i = 0; i < 4; i++) {
    String name = eeprom_read_string((i==0)?BDAY1_NAME_ADDR:(i==1)?BDAY2_NAME_ADDR:(i==2)?BDAY3_NAME_ADDR:BDAY4_NAME_ADDR, 16);
    String bday = eeprom_read_string((i==0)?BDAY1_DATE_ADDR:(i==1)?BDAY2_DATE_ADDR:(i==2)?BDAY3_DATE_ADDR:BDAY4_DATE_ADDR, 8);
    form += html_input(String("bday_name"+String(i+1)).c_str(), name, false, String("name "+String(i+1)).c_str(),  "(max. 10 characters)"); //longer than 10 characters won't fit
    form += html_input(String("bday_date"+String(i+1)).c_str(), bday, false, String("birthday (dd.mm) "+String(i+1)).c_str(), "(e.g. 24.12)");
  }
  form += "</fieldset>";
  form += "<form action='/' method='GET' class='btn-group'><button type='submit'>Back to Setup</button></form>";
  String html = String(wifi_form_html_template);
  html.replace("%s", form);
  wifiServer.send(200, "text/html", html);
}

void handle_wifi_root() {
  String form = "";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>WiFi Settings</legend>";
  String ssid_val = eeprom_read_string(SSID_ADDR, 64);
  form += html_input("ssid", ssid_val, false, nullptr, nullptr);
  form += html_input("pass", "", true, nullptr, nullptr);
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Geo Settings</legend>";
  String lat_val = eeprom_read_string(LAT_ADDR, 32);
  String lon_val = eeprom_read_string(LON_ADDR, 32);
  String city_val = eeprom_read_string(CITY_ADDR, 32);
  String country_val = eeprom_read_string(COUNTRY_ADDR, 8);
  String hemisphere_val = eeprom_read_string(HEMISPHERE_ADDR, 8);
  String timezone_val = eeprom_read_string(TIMEZONE_ADDR, 32);
  String gmtoffset_val = String(EEPROM.read(GMTOFFSET_ADDR) | (EEPROM.read(GMTOFFSET_ADDR+1)<<8) | (EEPROM.read(GMTOFFSET_ADDR+2)<<16) | (EEPROM.read(GMTOFFSET_ADDR+3)<<24));
  String daylight_val = String(EEPROM.read(DAYLIGHT_ADDR) | (EEPROM.read(DAYLIGHT_ADDR+1)<<8) | (EEPROM.read(DAYLIGHT_ADDR+2)<<16) | (EEPROM.read(DAYLIGHT_ADDR+3)<<24));
  String sleepduration_val = String(EEPROM.read(SLEEPDURATION_ADDR) | (EEPROM.read(SLEEPDURATION_ADDR+1)<<8) | (EEPROM.read(SLEEPDURATION_ADDR+2)<<16) | (EEPROM.read(SLEEPDURATION_ADDR+3)<<24));
  form += html_input("lat", lat_val, false, nullptr, nullptr);
  form += html_input("lon", lon_val, false, nullptr, nullptr);
  form += html_input("city", city_val, false, nullptr, nullptr);
  form += html_input("country", country_val, false, nullptr, nullptr);
  form += html_input("hemisphere", hemisphere_val, false, nullptr, nullptr);
  form += html_input("timezone", timezone_val, false, "time zone", "see: <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank'>https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv</a>");
  form += html_input("gmtoffset", gmtoffset_val, false, "GMT offset [sec]", "(e.g. 3600 for GMT+1)");
  form += html_input("daylight", daylight_val, false, "daylight saving offset [sec]", "(e.g. 3600 for 1 hour)");
   form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>Refresh Period</legend>";
  form += html_input("sleepduration", sleepduration_val, false, "update every [min]", "(default: 30 min)");
  form += "</fieldset>";
  form += "<fieldset style='margin-bottom:40px;'><legend style='font-size:1.2em;font-weight:bold;'>OpenWeatherMap API</legend>";
  String apikey_val = eeprom_read_string(APIKEY_ADDR, 64);
  form += html_input("apikey", apikey_val, false, "apikey 2.5", "register for free api key at: <a href='https://home.openweathermap.org' target='_blank'> https://home.openweathermap.org</a>");
  form += "</fieldset>";
  String html = String(wifi_form_html_template);
  html.replace("%s", form);
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
    if (field == "sleepduration" && wifiServer.hasArg("sleepduration")) {
      long val = wifiServer.arg("sleepduration").toInt();
      Serial.println("Saving Sleep Duration: " + String(val));
      EEPROM.write(SLEEPDURATION_ADDR, (uint8_t)((val >> 0) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR+1, (uint8_t)((val >> 8) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR+2, (uint8_t)((val >> 16) & 0xFF));
      EEPROM.write(SLEEPDURATION_ADDR+3, (uint8_t)((val >> 24) & 0xFF));
    }
    for (int i = 0; i < 4; i++) {
      String name_field = String("bday_name"+String(i+1));
      String date_field = String("bday_date"+String(i+1));
      int name_addr = (i==0)?BDAY1_NAME_ADDR:(i==1)?BDAY2_NAME_ADDR:(i==2)?BDAY3_NAME_ADDR:BDAY4_NAME_ADDR;
      int date_addr = (i==0)?BDAY1_DATE_ADDR:(i==1)?BDAY2_DATE_ADDR:(i==2)?BDAY3_DATE_ADDR:BDAY4_DATE_ADDR;
      if (field == name_field && wifiServer.hasArg(name_field)) {
        Serial.println("Saving " + name_field + ": " + wifiServer.arg(name_field));
        eeprom_write_string(name_addr, wifiServer.arg(name_field), 16);
      }
      if (field == date_field && wifiServer.hasArg(date_field)) {
        Serial.println("Saving " + date_field + ": " + wifiServer.arg(date_field));
        eeprom_write_string(date_addr, wifiServer.arg(date_field), 8);
      }
    }
    EEPROM.commit();
    Serial.println("EEPROM commit done. No page refresh.");
    wifiServer.send(204, "text/plain", "");
  } else {
    Serial.println("No field selected for update.");
    wifiServer.send(400, "text/html", "<h2>No field selected for update</h2>");
  }
}

void handle_erase_eeprom() {
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }
  EEPROM.commit();
  wifiServer.send(200, "text/html", "<h2>EEPROM erased with all 0x00. Please reboot the device.</h2>");
}

void erase_eeprom(int eeprom_size, byte erase_value){
  for (int i = 0; i < eeprom_size; i++) {
    EEPROM.write(i, erase_value);
  }
  EEPROM.commit();
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
  wifiServer.on("/erase_eeprom", HTTP_GET, handle_erase_eeprom);
  wifiServer.begin();
  Serial.println("WiFi setup portal started. Connect to 'weather_station_wifi' and open http://192.168.4.1/");
  while (true) {
    wifiServer.handleClient();
    delay(10);
  }
}
