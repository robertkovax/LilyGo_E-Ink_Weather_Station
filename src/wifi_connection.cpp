#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
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

  // If a valid unicast MAC (not all 00, not all FF, LSB of first byte not multicast), set it.
  auto valid_unicast_mac = [](const uint8_t *m)
  {
    if (!m)
      return false;
    bool all_zero = true, all_ff = true;
    for (int i = 0; i < 6; ++i)
    {
      all_zero &= (m[i] == 0x00);
      all_ff &= (m[i] == 0xFF);
    }
    if (all_zero || all_ff)
      return false;
    if (m[0] & 0x01)
      return false; // multicast bit set -> not a unicast MAC
    return true;
  };

  auto mac_equal = [](const uint8_t *a, const uint8_t *b)
  {
    for (int i = 0; i < 6; i++)
    {
      if (a[i] != b[i])
        return false;
    }
    return true;
  };

  if (valid_unicast_mac(mac) && !mac_equal(mac, hw_mac))
  {
    // Must stop before changing MAC on ESP32
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
    //save also to eeprom
    char mac_c[18];
    snprintf(mac_c, sizeof(mac_c),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             hw_mac[0], hw_mac[1], hw_mac[2], hw_mac[3], hw_mac[4], hw_mac[5]);
    eeprom_write_string(MAC_ADDR, String(mac_c), sizeof(mac_c));
    eeprom_commit();
  }
  else{
    Serial.print("using hardware MAC: ");
  }

  // Verify MAC address
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

  // Return current status WITHOUT waiting
  return WiFi.status();
}
// ##########################################################################################
// ---------------- Captive portal / Meraki network-auth ----------------

// Fallback from a captured URL. The code first tries to discover a fresh
// base_grant_url for this ESP32, then falls back to this grant endpoint.
static const char *CAPTIVE_FALLBACK_GRANT_URL = "https://n148.network-auth.com/splash/grant";
static const char *CAPTIVE_CONTINUE_URL = "http://www.msftconnecttest.com/redirect";
static const uint32_t CAPTIVE_DURATION_SEC = 3600;
static const char *CAPTIVE_PROBE_URL = "http://www.msftconnecttest.com/redirect";
static const char *ONLINE_TEST_URL = "http://www.msftconnecttest.com/connecttest.txt";
static const char *ONLINE_TEST_BODY = "Microsoft Connect Test";

static String captiveSplashUrl;
static String captiveNetworkAuthCookie;
static String captiveSocialCookie;
static String captiveLandingUrl;
static String captiveGrantReferer;

static int hexVal(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool isUrlUnreserved(char c)
{
  return (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == '.' || c == '~';
}

static bool isAsciiSpace(char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static bool isHtmlNameChar(char c)
{
  return (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') ||
         c == '-' || c == '_' || c == ':';
}

static String urlEncode(const String &in)
{
  String out;
  char buf[4];

  for (size_t i = 0; i < in.length(); i++)
  {
    uint8_t c = (uint8_t)in[i];

    if (isUrlUnreserved((char)c))
    {
      out += (char)c;
    }
    else
    {
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }

  return out;
}

static String urlDecode(const String &in)
{
  String out;

  for (size_t i = 0; i < in.length(); i++)
  {
    char c = in[i];

    if (c == '+')
    {
      out += ' ';
    }
    else if (c == '%' && i + 2 < in.length())
    {
      int hi = hexVal(in[i + 1]);
      int lo = hexVal(in[i + 2]);

      if (hi >= 0 && lo >= 0)
      {
        out += (char)((hi << 4) | lo);
        i += 2;
      }
      else
      {
        out += c;
      }
    }
    else
    {
      out += c;
    }
  }

  return out;
}

static String getQueryParam(const String &url, const char *name)
{
  int q = url.indexOf('?');
  if (q < 0) return "";

  String wanted = String(name);
  int start = q + 1;

  while (start < url.length())
  {
    int end = url.indexOf('&', start);
    if (end < 0) end = url.length();

    int eq = url.indexOf('=', start);

    if (eq >= start && eq < end)
    {
      String key = url.substring(start, eq);
      if (key == wanted)
      {
        return urlDecode(url.substring(eq + 1, end));
      }
    }

    start = end + 1;
  }

  return "";
}

static String getUrlOrigin(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);
  if (pathStart < 0) return url;

  return url.substring(0, pathStart);
}

static String getUrlBasePath(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int pathStart = url.indexOf('/', schemeEnd + 3);
  if (pathStart < 0) return url + "/";

  int queryStart = url.indexOf('?', pathStart);
  int lastSlash = url.lastIndexOf('/', queryStart >= 0 ? queryStart : url.length());
  if (lastSlash < pathStart) return getUrlOrigin(url) + "/";

  return url.substring(0, lastSlash + 1);
}

static String resolveUrl(const String &baseUrl, const String &maybeRelative)
{
  if (maybeRelative.startsWith("http://") || maybeRelative.startsWith("https://"))
  {
    return maybeRelative;
  }

  if (maybeRelative.startsWith("//"))
  {
    int schemeEnd = baseUrl.indexOf(':');
    if (schemeEnd >= 0) return baseUrl.substring(0, schemeEnd + 1) + maybeRelative;
  }

  if (maybeRelative.startsWith("/"))
  {
    return getUrlOrigin(baseUrl) + maybeRelative;
  }

  return getUrlBasePath(baseUrl) + maybeRelative;
}

static String cookiePairFromSetCookie(const String &setCookie)
{
  int end = setCookie.indexOf(';');
  if (end < 0) end = setCookie.length();

  String pair = setCookie.substring(0, end);
  pair.trim();
  return pair;
}

static void appendCookiePairToJar(String &jar, const String &pair)
{
  String clean = pair;
  clean.trim();

  int eq = clean.indexOf('=');
  if (eq <= 0) return;

  String name = clean.substring(0, eq);

  if (jar.indexOf(name + "=") >= 0)
  {
    return;
  }

  if (jar.length())
  {
    jar += "; ";
  }

  jar += clean;
}

static void storeSetCookieInJar(String &jar, const String &setCookie)
{
  String pair = cookiePairFromSetCookie(setCookie);
  if (pair.length())
  {
    appendCookiePairToJar(jar, pair);
  }
}

static bool cookieJarHas(const String &jar, const char *cookieName)
{
  return jar.indexOf(String(cookieName) + "=") >= 0;
}

static String getUrlHost(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "";

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);

  String hostPort = pathStart < 0 ? url.substring(hostStart) : url.substring(hostStart, pathStart);

  int colon = hostPort.indexOf(':');
  if (colon >= 0)
  {
    return hostPort.substring(0, colon);
  }

  return hostPort;
}

static uint16_t getUrlPort(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return 80;

  String scheme = url.substring(0, schemeEnd);
  scheme.toLowerCase();

  int hostStart = schemeEnd + 3;
  int pathStart = url.indexOf('/', hostStart);

  String hostPort = pathStart < 0 ? url.substring(hostStart) : url.substring(hostStart, pathStart);

  int colon = hostPort.indexOf(':');
  if (colon >= 0)
  {
    return (uint16_t)hostPort.substring(colon + 1).toInt();
  }

  return scheme == "https" ? 443 : 80;
}

static String getUrlPathAndQuery(const String &url)
{
  int schemeEnd = url.indexOf("://");
  if (schemeEnd < 0) return "/";

  int pathStart = url.indexOf('/', schemeEnd + 3);
  if (pathStart < 0) return "/";

  return url.substring(pathStart);
}

static String getHeaderValue(const String &line, const char *headerName)
{
  String prefix = String(headerName) + ":";

  if (line.length() < prefix.length())
  {
    return "";
  }

  String actual = line.substring(0, prefix.length());

  if (!actual.equalsIgnoreCase(prefix))
  {
    return "";
  }

  String value = line.substring(prefix.length());
  value.trim();
  return value;
}

static bool isRedirectStatus(int code)
{
  return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

static String appendQueryParam(const String &url, const char *name, const String &value)
{
  String out = url;
  out += (out.indexOf('?') >= 0) ? "&" : "?";
  out += name;
  out += "=";
  out += urlEncode(value);
  return out;
}

static String buildGrantUrl(const String &baseGrantUrl, const String &continueUrl)
{
  String grantUrl = appendQueryParam(baseGrantUrl, "continue_url", continueUrl);

  if (CAPTIVE_DURATION_SEC > 0)
  {
    grantUrl += "&duration=";
    grantUrl += String(CAPTIVE_DURATION_SEC);
  }

  return grantUrl;
}

static String getHtmlAttr(const String &tag, const char *attrName)
{
  String wanted = String(attrName);
  String lowerTag = tag;
  lowerTag.toLowerCase();
  wanted.toLowerCase();

  int pos = lowerTag.indexOf(wanted);

  while (pos >= 0)
  {
    int nameEnd = pos + wanted.length();
    bool leftOk = (pos == 0) || !isHtmlNameChar(lowerTag[pos - 1]);
    bool rightOk = nameEnd < lowerTag.length() && (lowerTag[nameEnd] == '=' || isAsciiSpace(lowerTag[nameEnd]));

    if (leftOk && rightOk)
    {
      int eq = lowerTag.indexOf('=', nameEnd);
      if (eq >= 0)
      {
        int valueStart = eq + 1;
        while (valueStart < tag.length() && isAsciiSpace(tag[valueStart])) valueStart++;

        if (valueStart < tag.length() && (tag[valueStart] == '"' || tag[valueStart] == '\''))
        {
          char quote = tag[valueStart++];
          int valueEnd = tag.indexOf(quote, valueStart);
          if (valueEnd >= 0) return tag.substring(valueStart, valueEnd);
        }
        else
        {
          int valueEnd = valueStart;
          while (valueEnd < tag.length() && !isAsciiSpace(tag[valueEnd]) && tag[valueEnd] != '>') valueEnd++;
          return tag.substring(valueStart, valueEnd);
        }
      }
    }

    pos = lowerTag.indexOf(wanted, pos + 1);
  }

  return "";
}

static String submitCaptiveLandingForm(const String &landingUrl, const String &body)
{
  String lowerBody = body;
  lowerBody.toLowerCase();

  int formStart = lowerBody.indexOf("<form");
  if (formStart < 0)
  {
    Serial.println("Captive landing form: none found.");
    return "";
  }

  int formTagEnd = lowerBody.indexOf('>', formStart);
  int formEnd = lowerBody.indexOf("</form", formTagEnd);
  if (formTagEnd < 0 || formEnd < 0)
  {
    Serial.println("Captive landing form: malformed.");
    return "";
  }

  String formTag = body.substring(formStart, formTagEnd + 1);
  String action = getHtmlAttr(formTag, "action");
  String method = getHtmlAttr(formTag, "method");
  method.toUpperCase();

  if (action.length() == 0)
  {
    action = landingUrl;
  }

  if (method.length() == 0)
  {
    method = "GET";
  }

  String actionUrl = resolveUrl(landingUrl, action);
  String formBody = "";
  int inputPos = formStart;

  while (true)
  {
    inputPos = lowerBody.indexOf("<input", inputPos);
    if (inputPos < 0 || inputPos > formEnd) break;

    int inputEnd = lowerBody.indexOf('>', inputPos);
    if (inputEnd < 0 || inputEnd > formEnd) break;

    String inputTag = body.substring(inputPos, inputEnd + 1);
    String name = getHtmlAttr(inputTag, "name");
    String value = getHtmlAttr(inputTag, "value");
    String type = getHtmlAttr(inputTag, "type");
    type.toLowerCase();

    if (name.length() && type != "button" && type != "reset")
    {
      if (type == "checkbox" && value.length() == 0)
      {
        value = "on";
      }

      if (formBody.length()) formBody += "&";
      formBody += urlEncode(name);
      formBody += "=";
      formBody += urlEncode(value);
    }

    inputPos = inputEnd + 1;
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *headerKeys[] = {"Location", "Set-Cookie"};

  String requestUrl = actionUrl;
  if (method == "GET" && formBody.length())
  {
    requestUrl += (requestUrl.indexOf('?') >= 0) ? "&" : "?";
    requestUrl += formBody;
  }

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(secureClient, requestUrl))
  {
    Serial.println("Captive landing form begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 2);
  http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");
  http.addHeader("Referer", landingUrl);

  if (captiveSocialCookie.length())
  {
    http.addHeader("Cookie", captiveSocialCookie);
  }

  int code = 0;
  if (method == "POST")
  {
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST(formBody);
  }
  else
  {
    code = http.GET();
  }

  String location = http.header("Location");
  if (location.length())
  {
    location = resolveUrl(requestUrl, location);
    captiveGrantReferer = requestUrl;
  }

  String setCookie = http.header("Set-Cookie");
  if (setCookie.length())
  {
    storeSetCookieInJar(captiveSocialCookie, setCookie);
  }

  http.end();
  return (code >= 200 && code < 400) ? location : "";
}

static void primeCaptiveSplashSession(const String &splashUrl)
{
  captiveNetworkAuthCookie = "";
  captiveSocialCookie = "";
  captiveLandingUrl = "";
  captiveGrantReferer = "";

  if (splashUrl.length() == 0)
  {
    return;
  }

  String host = getUrlHost(splashUrl);
  String path = getUrlPathAndQuery(splashUrl);
  uint16_t port = getUrlPort(splashUrl);

  if (host.length() == 0)
  {
    Serial.println("Could not parse splash host.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  if (!client.connect(host.c_str(), port))
  {
    Serial.println("Captive splash raw TLS connect failed.");
    return;
  }

  client.print(String("GET ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  client.print("User-Agent: Mozilla/5.0 ESP32 captive portal\r\n");
  client.print("Accept: */*\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");

  client.readStringUntil('\n'); // status line

  while (client.connected() || client.available())
  {
    String line = client.readStringUntil('\n');
    line.trim();

    if (line.length() == 0)
    {
      break;
    }

    String location = getHeaderValue(line, "Location");
    if (location.length())
    {
      captiveLandingUrl = resolveUrl(splashUrl, location);
      continue;
    }

    String setCookie = getHeaderValue(line, "Set-Cookie");
    if (setCookie.length())
    {
      storeSetCookieInJar(captiveNetworkAuthCookie, setCookie);
    }
  }

  client.stop();

  if (!cookieJarHas(captiveNetworkAuthCookie, "p_splash_session"))
  {
    Serial.println("WARNING: p_splash_session cookie was not captured.");
  }
}

static String fetchCaptiveLandingPage(const String &landingUrl)
{
  if (landingUrl.length() == 0)
  {
    return "";
  }

  WiFiClientSecure secureClient;
  secureClient.setInsecure();

  HTTPClient http;
  const char *headerKeys[] = {"Location", "Set-Cookie"};

  http.setConnectTimeout(15000);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(secureClient, landingUrl))
  {
    Serial.println("Captive landing begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 2);
  http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");

  if (captiveSocialCookie.length())
  {
    http.addHeader("Cookie", captiveSocialCookie);
  }

  int code = http.GET();
  String location = http.header("Location");

  String setCookie = http.header("Set-Cookie");
  if (setCookie.length())
  {
    storeSetCookieInJar(captiveSocialCookie, setCookie);
  }

  String grantUrl = "";

  if (location.indexOf("/splash/grant") >= 0)
  {
    grantUrl = location;
  }
  else
  {
    String baseGrantUrl = getQueryParam(landingUrl, "base_grant_url");
    if (baseGrantUrl.length())
    {
      String continueUrl = getQueryParam(landingUrl, "user_continue_url");
      if (continueUrl.length() == 0)
      {
        continueUrl = CAPTIVE_CONTINUE_URL;
      }

      grantUrl = buildGrantUrl(baseGrantUrl, continueUrl);
    }
  }

  if (code == HTTP_CODE_OK)
  {
    String body = http.getString();
    body.replace("\r", " ");
    body.replace("\n", " ");
    body.trim();

    String formLocation = submitCaptiveLandingForm(landingUrl, body);
    if (formLocation.length())
    {
      if (formLocation.indexOf("/splash/grant") >= 0)
      {
        grantUrl = formLocation;
      }
      else
      {
        Serial.println("Captive landing form did not return a Meraki grant URL.");
      }
    }
  }

  http.end();
  return grantUrl;
}

static bool internetIsOpen()
{
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(client, ONLINE_TEST_URL))
  {
    Serial.println("Online probe begin failed.");
    return false;
  }

  int code = http.GET();

  bool ok = false;

  if (code == HTTP_CODE_OK)
  {
    String body = http.getString();
    body.trim();

    if (body.indexOf(ONLINE_TEST_BODY) >= 0)
    {
      ok = true;
    }
  }

  http.end();
  return ok;
}

static String discoverMerakiGrantUrl()
{
  WiFiClient client;
  HTTPClient http;
  const char *headerKeys[] = {"Location"};

  captiveSplashUrl = "";
  captiveNetworkAuthCookie = "";
  captiveSocialCookie = "";
  captiveLandingUrl = "";
  captiveGrantReferer = "";

  http.setConnectTimeout(8000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

  if (!http.begin(client, CAPTIVE_PROBE_URL))
  {
    Serial.println("Captive portal probe begin failed.");
    return "";
  }

  http.collectHeaders(headerKeys, 1);

  http.GET();
  String location = http.header("Location");

  http.end();

  String baseGrantUrl = getQueryParam(location, "base_grant_url");
  if (baseGrantUrl.length() == 0)
  {
    String origin = getUrlOrigin(location);
    if (origin.length() == 0 || location.indexOf("/splash/") < 0)
    {
      return "";
    }

    captiveSplashUrl = location;
    baseGrantUrl = origin + "/splash/grant";
  }

  String continueUrl = getQueryParam(location, "user_continue_url");
  if (continueUrl.length() == 0)
  {
    continueUrl = getQueryParam(location, "continue_url");
  }

  if (continueUrl.length() == 0)
  {
    continueUrl = CAPTIVE_CONTINUE_URL;
  }

  String grantUrl = baseGrantUrl;

  return buildGrantUrl(grantUrl, continueUrl);
}

static bool callCaptiveGrantUrl(const String &grantUrl, const String &splashUrl = "")
{
  if (grantUrl.length() == 0)
  {
    Serial.println("Empty captive grant URL.");
    return false;
  }

  String finalGrantUrl = grantUrl;

  if (splashUrl.length())
  {
    primeCaptiveSplashSession(splashUrl);
  }

  String landingGrantUrl = fetchCaptiveLandingPage(captiveLandingUrl);
  if (landingGrantUrl.length())
  {
    finalGrantUrl = landingGrantUrl;
  }

  if (!cookieJarHas(captiveNetworkAuthCookie, "p_splash_session"))
  {
    Serial.println("WARNING: Grant will probably fail because p_splash_session is missing.");
  }

  String currentUrl = finalGrantUrl;
  String referer = captiveGrantReferer.length() ? captiveGrantReferer : splashUrl;

  for (byte hop = 0; hop < 5; hop++)
  {
    bool thisIsGrantUserAccess = currentUrl.indexOf("/splash/grant_user_access") >= 0;

    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    HTTPClient http;

    secureClient.setInsecure();

    http.setConnectTimeout(15000);
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);

    bool beginOk = false;

    if (currentUrl.startsWith("https://"))
    {
      beginOk = http.begin(secureClient, currentUrl);
    }
    else
    {
      beginOk = http.begin(plainClient, currentUrl);
    }

    if (!beginOk)
    {
      Serial.println("Captive grant begin failed.");
      return false;
    }

    const char *headerKeys[] = {"Location", "Set-Cookie"};
    http.collectHeaders(headerKeys, 2);

    http.addHeader("User-Agent", "Mozilla/5.0 ESP32 captive portal");

    if (referer.length())
    {
      http.addHeader("Referer", referer);
    }

    if (currentUrl.indexOf("network-auth.com") >= 0 && captiveNetworkAuthCookie.length())
    {
      http.addHeader("Cookie", captiveNetworkAuthCookie);
    }

    int code = http.GET();

    String setCookie = http.header("Set-Cookie");
    if (setCookie.length() && currentUrl.indexOf("network-auth.com") >= 0)
    {
      storeSetCookieInJar(captiveNetworkAuthCookie, setCookie);
    }

    String location = http.header("Location");
    if (location.length())
    {
      location = resolveUrl(currentUrl, location);
    }

    if (code < 0)
    {
      String err = http.errorToString(code);
      Serial.printf("Captive grant HTTP error: %s\n", err.c_str());
      http.end();
      return false;
    }

    if (thisIsGrantUserAccess && code >= 200 && code < 400)
    {
      http.end();
      return true;
    }

    if (isRedirectStatus(code) && location.length())
    {
      http.end();

      referer = currentUrl;
      currentUrl = location;
      delay(300);
      continue;
    }

    if (currentUrl.indexOf("/splash/grant") >= 0 && currentUrl.indexOf("/splash/grant_user_access") < 0)
    {
      if (code == HTTP_CODE_OK)
      {
        http.getString();
        Serial.println("Grant returned 200 instead of redirecting to grant_user_access.");
        Serial.println("This usually means the Meraki p_splash_session cookie was missing or invalid.");

        http.end();
        return false;
      }
    }

    http.end();
    return code >= 200 && code < 400;
  }

  Serial.println("Too many captive grant redirects.");
  return false;
}

static String buildFallbackGrantUrl()
{
  return buildGrantUrl(CAPTIVE_FALLBACK_GRANT_URL, CAPTIVE_CONTINUE_URL);
}

static bool ensureCaptivePortalAuthenticated()
{
  if (internetIsOpen())
  {
    Serial.println("Internet already available. No captive portal grant needed.");
    return true;
  }

  Serial.println("Internet not open yet. Trying captive portal authentication...");

  String grantUrl = discoverMerakiGrantUrl();

  if (grantUrl.length() == 0)
  {
    Serial.println("Could not discover fresh base_grant_url. Using fallback grant URL.");
    grantUrl = buildFallbackGrantUrl();
  }

  if (!callCaptiveGrantUrl(grantUrl, captiveSplashUrl))
  {
    Serial.println("Captive grant request failed.");
    return false;
  }

  delay(1500);

  if (internetIsOpen())
  {
    Serial.println("Captive portal authentication succeeded.");
    return true;
  }

  Serial.println("Grant request completed, but internet probe still failed.");
  return false;
}

// ##########################################################################################
// Connect with retries + UI + sleep policy. ALL waiting happens here.
void connect2wifi()
{
  // Parse desired MAC
  uint8_t desiredMac[6] = {};
  if (sscanf(MAC, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
             &desiredMac[0], &desiredMac[1], &desiredMac[2],
             &desiredMac[3], &desiredMac[4], &desiredMac[5]) != 6)
  {
    memset(desiredMac, 0, sizeof(desiredMac)); // use hardware MAC
  }

  byte reconnect_cnt = 0;
  const byte max_reconnect_attempts = 1;
  const unsigned long attempt_timeout_ms = 15000; // full attempt window
  const unsigned long poll_ms = 500;
  const unsigned long cooldown_ms = 800; // small pause between attempts

  for (;;)
  {
    // Kick off (non-blocking)
    StartWiFi(desiredMac);

    // Wait ONLY here: until connected OR attempt window expires
    unsigned long t0 = millis();
    uint8_t status = WL_IDLE_STATUS;
    bool auth_failed = false;

    Serial.print("Connecting..");

    do
    {
      status = WiFi.status();

      if (status == WL_CONNECTED)
      {
        Serial.println("\nWiFi associated.");
        Serial.println("local IP: " + WiFi.localIP().toString());

        if (!ensureCaptivePortalAuthenticated())
        {
          Serial.println("WiFi associated, but captive portal authentication failed.");
          auth_failed = true;
          break;
        }

        int wifi_signal = WiFi.RSSI();
        Serial.println("\nWiFi connected + captive portal authenticated!" +
                       String("\nlocal IP: ") + WiFi.localIP().toString() +
                       String("\nstrength: ") + String(wifi_signal) + " dBm");
        return; // success
      }

      delay(poll_ms);
      Serial.print(".");
    } while ((millis() - t0) < attempt_timeout_ms);

    // Attempt failed
    reconnect_cnt++;

    if (auth_failed)
    {
      Serial.printf("\nWiFi captive portal auth attempt %u failed (status=%d)\n", reconnect_cnt, status);
    }
    else
    {
      Serial.printf("\nWiFi connection attempt %u failed (status=%d)\n", reconnect_cnt, status);
    }

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
      return; // device will sleep
    }

    // Cooldown + hard reset of station state before next attempt
    WiFi.disconnect(true, true); // forget connection + drop STA
    esp_wifi_stop();             // stop driver to clear state
    delay(cooldown_ms);
  }
}
// #########################################################################################
void StopWiFi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
