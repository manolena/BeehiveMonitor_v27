// (Complete file - put into your sketch, replaces existing .ino)
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 512

#include "config.h"
#include "ui.h"
#include "menu_manager.h"
#include "text_strings.h"
#include "modem_manager.h"
#include "time_manager.h"
#include "weather_manager.h"
#include "key_server.h"
#include "modem_test.h"
#include "thingspeak_client.h"
#include "serial_commands.h"
#include "sms_handler.h"
#include "provisioning_server.h"

#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

// -----------------------------------------------------------------------------
// Single global language selection (definition)
Language currentLanguage = LANG_EN;

// -----------------------------------------------------------------------------
// Define the single global LCD instance (ui.cpp declares extern LiquidCrystal_I2C lcd;)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ============================================
// PLACEHOLDER SENSOR VALUES FOR TESTING
float test_weight = 12.4;
float test_temp_int = 32.5;
float test_hum_int = 58.0;
float test_temp_ext = 28.8;
float test_hum_ext = 64.0;
float test_pressure = 1012.3;
float test_acc_x = 0.02;
float test_acc_y = -0.01;
float test_acc_z = 0.98;
float test_batt_voltage = 4.12;
int test_batt_percent = 87;
int test_rssi = -72;

// -----------------------------------------------------------------------------
// Preferences keys used by the provisioning form / app
static const char* PREF_WIFI_NS = "beehive";
static const char* PREF_WIFI_SSID1 = "wifi_ssid1";
static const char* PREF_WIFI_PSK1  = "wifi_psk1";
static const char* PREF_WIFI_SSID2 = "wifi_ssid2";
static const char* PREF_WIFI_PSK2  = "wifi_psk2";

static const char* PREF_APP_NS = "beehive_app";
static const char* PREF_TS_AUTO = "ts_auto";
static const char* PREF_TS_INTERVAL = "ts_interval_min";
static const char* PREF_NET_PREF = "net_pref"; // 0=auto,1=force wifi,2=force lte

// -----------------------------------------------------------------------------
// Network mode enum + runtime state
enum NetMode { NET_NONE = 0, NET_LTE = 1, NET_WIFI = 2 };
static NetMode currentNet = NET_NONE;
static int net_pref = 0; // read from prefs (0 auto,1 wifi,2 lte)

// Auto-upload state
static bool ts_auto_enabled = false;
static int ts_interval_min = 60;
static unsigned long ts_next_upload = 0;

// Forward declaration to the modem posting helper (in thingspeak_client_modem.cpp)
extern bool thingspeak_post_via_modem(const String &postBody);

// -----------------------------------------------------------------------------
// wifi_connectFromPrefs implementation
// Tries SSID1 then SSID2. Returns true if connected.
bool wifi_connectFromPrefs(unsigned long timeoutMs = 8000) {
  Preferences p;
  p.begin(PREF_WIFI_NS, true);
  String ssid1 = p.getString(PREF_WIFI_SSID1, "");
  String psk1  = p.getString(PREF_WIFI_PSK1, "");
  String ssid2 = p.getString(PREF_WIFI_SSID2, "");
  String psk2  = p.getString(PREF_WIFI_PSK2, "");
  p.end();

  if (ssid1.length() == 0 && ssid2.length() == 0) {
    Serial.println(F("[WiFi] No SSID stored in prefs"));
    return false;
  }

  auto tryConnect = [&](const String &ssid, const String &psk) -> bool {
    if (ssid.length() == 0) return false;
    Serial.print(F("[WiFi] Trying SSID: "));
    Serial.println(ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), psk.c_str());
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(F("[WiFi] Connected, IP: "));
      Serial.println(WiFi.localIP());
      return true;
    }
    Serial.println(F("[WiFi] Connect timed out"));
    return false;
  };

  if (tryConnect(ssid1, psk1)) { currentNet = NET_WIFI; return true; }
  if (tryConnect(ssid2, psk2)) { currentNet = NET_WIFI; return true; }
  return false;
}

// -----------------------------------------------------------------------------
// tryStartLTE implementation
void tryStartLTE() {
  TinyGsm& modem = modem_get();
  Serial.println(F("[LTE] Attempting GPRS attach"));
  bool ok = modem.gprsConnect(MODEM_APN, MODEM_GPRS_USER, MODEM_GPRS_PASS);
  if (ok) {
    Serial.println(F("[LTE] GPRS attach OK"));
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
    keyServer_stop();
    currentNet = NET_LTE;
  } else {
    Serial.println(F("[LTE] GPRS attach failed"));
    currentNet = NET_NONE;
  }
}

// -----------------------------------------------------------------------------
// enqueue to SD helper (for offline posts)
static bool enqueuePostToSD(const String &post) {
  if (!SD.begin(SD_CS)) {
    Serial.println(F("[TS-QUEUE] SD.begin failed - cannot enqueue"));
    return false;
  }
  File f = SD.open("/ts_queue.txt", FILE_APPEND);
  if (!f) {
    Serial.println(F("[TS-QUEUE] Cannot open /ts_queue.txt for append"));
    return false;
  }
  f.println(post);
  f.close();
  Serial.println(F("[TS-QUEUE] Post enqueued to /ts_queue.txt"));
  return true;
}

// Build ThingSpeak body
static String buildThingSpeakPostBody() {
  char buf[64];
  String b;
  b.reserve(256);

  snprintf(buf, sizeof(buf), "%.1f", test_weight);
  b += String("field1=") + String(buf);
  snprintf(buf, sizeof(buf), "%.1f", test_temp_int);
  b += String("&field2=") + String(buf);
  snprintf(buf, sizeof(buf), "%.0f", test_hum_int);
  b += String("&field3=") + String(buf);
  snprintf(buf, sizeof(buf), "%.1f", test_temp_ext);
  b += String("&field4=") + String(buf);
  snprintf(buf, sizeof(buf), "%.0f", test_hum_ext);
  b += String("&field5=") + String(buf);
  snprintf(buf, sizeof(buf), "%.0f", test_pressure);
  b += String("&field6=") + String(buf);
  snprintf(buf, sizeof(buf), "%.2f", test_batt_voltage);
  b += String("&field7=") + String(buf);

  Preferences p;
  p.begin(PREF_WIFI_NS, true);
  String latS = p.getString("owm_lat", "");
  String lonS = p.getString("owm_lon", "");
  p.end();
  double lat = DEFAULT_LAT, lon = DEFAULT_LON;
  if (latS.length() && lonS.length()) { lat = latS.toDouble(); lon = lonS.toDouble(); }
  char latBuf[32], lonBuf[32];
  snprintf(latBuf, sizeof(latBuf), "%.4f", lat);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", lon);
  String coords = String(latBuf) + String(" ") + String(lonBuf);
  coords.replace(" ", "+");
  b += String("&field8=") + coords;

  String post = String("api_key=") + String(THINGSPEAK_WRITE_APIKEY) + String("&") + b;
  return post;
}

// Try auto upload (WiFi -> LTE -> queue)
static bool uploadThingSpeakAuto() {
  String post = buildThingSpeakPostBody();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[TS-AUTO] WiFi connected - posting via WiFi"));
    bool ok = thingspeak_upload_current(); // existing WiFi path
    if (ok) return true;
    Serial.println(F("[TS-AUTO] thingspeak_upload_current() failed"));
  }

  if (modem_isNetworkRegistered()) {
    Serial.println(F("[TS-AUTO] Modem registered - posting via MODEM"));
    bool ok = thingspeak_post_via_modem(post);
    if (ok) return true;
    Serial.println(F("[TS-AUTO] modem post failed"));
  }

  return enqueuePostToSD(post);
}

// -----------------------------------------------------------------------------
// setup / loop
void setup() {
  Serial.begin(115200);
  delay(50);

  uiInit();
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);

  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
  bool sd_ok = SD.begin(SD_CS);

  weather_init();

  {
    Preferences p;
    p.begin(PREF_APP_NS, false);
    ts_auto_enabled = p.getBool(PREF_TS_AUTO, false);
    ts_interval_min = p.getInt(PREF_TS_INTERVAL, 60);
    net_pref = p.getInt(PREF_NET_PREF, 0);
    p.end();
    if (ts_interval_min <= 0) ts_interval_min = 60;
    if (ts_auto_enabled) ts_next_upload = millis() + 30 * 1000UL;
  }

  Serial.println(sd_ok ? F("SD init OK") : F("SD init FAIL"));

  showSplashScreen();
  menuInit();
  menuDraw();

  modem_hw_init();
  modemManager_init();
  serial_commands_init();
  sms_init();

  provisioning_init(); // starts server8080 internally

  // apply network pref
  if (net_pref == 2) tryStartLTE();
  else if (net_pref == 1) { if (!wifi_connectFromPrefs(8000)) Serial.println(F("[NET] Forced WiFi failed")); }
  else { tryStartLTE(); if (currentNet != NET_LTE) wifi_connectFromPrefs(8000); }

  timeManager_init();

  if (ts_auto_enabled && ts_next_upload == 0) ts_next_upload = millis() + (unsigned long)ts_interval_min * 60UL * 1000UL;
}

void loop() {
  menuUpdate();
  timeManager_update();

  // network management honoring net_pref
  if (net_pref == 0) {
    if (currentNet == NET_LTE) {
      if (!modem_isNetworkRegistered()) { currentNet = NET_NONE; wifi_connectFromPrefs(5000); }
    } else if (currentNet == NET_WIFI) {
      if (modem_isNetworkRegistered()) {
        if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(true); WiFi.mode(WIFI_OFF); }
        tryStartLTE();
      }
    } else {
      static unsigned long lastTry = 0;
      if (millis() - lastTry > 30000) { lastTry = millis(); tryStartLTE(); if (currentNet != NET_LTE) wifi_connectFromPrefs(5000); }
    }
  } else if (net_pref == 1) {
    if (currentNet != NET_WIFI) { if (wifi_connectFromPrefs(5000)) currentNet = NET_WIFI; }
  } else {
    if (currentNet != NET_LTE) tryStartLTE();
  }

  keyServer_loop();
  provisioning_loop();
  serial_commands_poll();
  sms_loop();

  if (ts_auto_enabled && millis() >= ts_next_upload) {
    Serial.println(F("[TS-AUTO] Scheduled ThingSpeak upload triggered"));
    bool ok = uploadThingSpeakAuto();
    Serial.print(F("[TS-AUTO] Upload result: "));
    Serial.println(ok ? F("OK") : F("FAIL"));
    ts_next_upload = millis() + (unsigned long)ts_interval_min * 60UL * 1000UL;
  }

  static unsigned long lastRetry = 0;
  if (millis() - lastRetry > 60000) { retryQueuedThingSpeak(); lastRetry = millis(); }

#if ENABLE_DEBUG
  if (Serial.available()) {
    int c = Serial.read();
    Serial.printf("[DBG] Serial rx: '%c' 0x%02X\n", (char)c, (uint8_t)c);
    if (c == 'h') modem_test_https_get("internet", "httpbin.org", "/get");
    else if (c == 'm') modem_test_status();
    else if (c == 't') modem_test_http_get("internet", "", "", "93.184.216.34", 80, "/");
    else if (c == 'T') modem_test_http_get("cosmote", "", "", "93.184.216.34", 80, "/");
    else if (c == 'g') modem_test_http_get("internet", "", "", "142.250.72.206", 80, "/");
    else if (c == 'a') {
      Serial.println(F("[DBG] Enter APN now (terminate with newline):"));
      String apn = "";
      unsigned long start = millis();
      while (millis() - start < 8000) {
        while (Serial.available()) {
          char ch = Serial.read();
          if (ch == '\r' || ch == '\n') break;
          apn += ch;
        }
        if (apn.length() > 0 && Serial.peek() == -1) {}
        if (apn.length() > 0 && (millis() - start) > 1000) break;
      }
      apn.trim();
      if (apn.length() == 0) Serial.println(F("[DBG] No APN entered"));
      else modem_test_http_get(apn.c_str(), "", "", "93.184.216.34", 80, "/");
    }
  }
#endif

  delay(10);
}

// showSplashScreen unchanged...
void showSplashScreen() {
  uiClear();
  if (currentLanguage == LANG_EN) {
    uiPrint_P(F("===================="), 0, 0);
    uiPrint_P(F("  BEEHIVE MONITOR   "), 0, 1);
    uiPrint_P(F("        v27         "), 0, 2);
    uiPrint_P(F("===================="), 0, 3);
  } else {
    uiPrint_P(F("===================="), 0, 0);
    lcdPrintGreek_P(F(" ΠΑΡΑΚΟΛΟΥΘΗΣΗ     "), 0, 1);
    lcdPrintGreek_P(F("  ΚΥΨΕΛΗΣ v27       "), 0, 2);
    uiPrint_P(F("===================="), 0, 3);
  }
  unsigned long splashStart = millis();
  const unsigned long SPLASH_TIMEOUT = 10000UL;
  while (true) {
    Button b = getButton();
    if (b != BTN_NONE) break;
    if (millis() - splashStart >= SPLASH_TIMEOUT) break;
    delay(20);
  }
  delay(1000);
}