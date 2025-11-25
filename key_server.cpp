#include "key_server.h"
#include "lcd_endpoint.h"
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include <WiFi.h>

// single global server instance used by keyServer_init/loop
static WebServer *s_srv = nullptr;

// HTML templates in PROGMEM
const char HTML_WIFI_FORM[] PROGMEM = 
  "<!doctype html><html><head><meta charset='utf-8'><title>WiFi Setup</title></head><body>"
  "<h3>Store WiFi credentials</h3>"
  "<form method='POST' action='/set_wifi'>"
  "SSID:<br><input name='ssid' type='text'><br>"
  "Password:<br><input name='pass' type='password'><br><br>"
  "<input type='submit' value='Save'>"
  "</form>"
  "</body></html>";

const char HTML_INDEX[] PROGMEM = 
  "<!doctype html><html><head><meta charset='utf-8'><title>Key Server</title></head><body>"
  "<h3>Beehive Monitor</h3>"
  "<ul>"
  "<li><a href='/wifi'>Store WiFi credentials</a></li>"
  "<li><a href='/lcd.json'>LCD (JSON)</a></li>"
  "</ul>"
  "</body></html>";

// Minimal JSON escaper for lcd lines
static String escapeJSON(const String &s) {
  String out;
  out.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (unsigned int)(uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// Internal: register handlers on an already-constructed server instance
static void register_handlers(WebServer &srv) {
  // lcd.json on port 80
  srv.on("/lcd.json", HTTP_GET, [&srv]() {
    String json = String("{\"lines\":[\"")
      + escapeJSON(lcd_get_line(0)) + String("\",\"")
      + escapeJSON(lcd_get_line(1)) + String("\",\"")
      + escapeJSON(lcd_get_line(2)) + String("\",\"")
      + escapeJSON(lcd_get_line(3)) + String("\"],\"ts\":") + String(millis()) + String("}");
    srv.sendHeader("Access-Control-Allow-Origin", "*");
    srv.send(200, "application/json; charset=utf-8", json);
  });

  // /wifi form
  srv.on("/wifi", HTTP_GET, [&srv]() {
    srv.send_P(200, "text/html; charset=utf-8", HTML_WIFI_FORM);
  });

  // Save WiFi POST handler
  srv.on("/set_wifi", HTTP_POST, [&srv]() {
    if (!srv.hasArg("ssid") || !srv.hasArg("pass")) {
      srv.send(400, F("text/plain"), F("Missing fields"));
      return;
    }
    String ssid = srv.arg("ssid");
    String pass = srv.arg("pass");
    Preferences p;
    p.begin("wifi_cfg", false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();
    srv.sendHeader(F("Location"), F("/wifi"));
    srv.send(303);
  });

  // index
  srv.on("/", HTTP_GET, [&srv]() {
    srv.send_P(200, "text/html; charset=utf-8", HTML_INDEX);
  });

  // OPTIONS preflight for /set_wifi
  srv.on("/set_wifi", HTTP_OPTIONS, [&srv]() {
    srv.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    srv.sendHeader(F("Access-Control-Allow-Methods"), F("POST, OPTIONS"));
    srv.sendHeader(F("Access-Control-Allow-Headers"), F("Content-Type"));
    srv.send(204);
  });
}

void keyServer_init() {
  // create server only if WiFi is up; otherwise do nothing (lazy start in loop)
  if (s_srv) return; // already created
  if (WiFi.status() != WL_CONNECTED) {
    // don't create server now; it will be auto-started inside keyServer_loop()
#if ENABLE_DEBUG
    Serial.println(F("[KeyServer] WiFi not connected - deferring server start"));
#endif
    return;
  }

  s_srv = new WebServer(80);
  register_handlers(*s_srv);
  s_srv->begin();
#if ENABLE_DEBUG
  Serial.println(F("[KeyServer] started on port 80"));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[KeyServer] IP: "));
    Serial.println(WiFi.localIP());
  }
#endif
}

void keyServer_stop() {
  if (!s_srv) return;
#if ENABLE_DEBUG
  Serial.println(F("[KeyServer] stopping server"));
#endif
  // attempt graceful stop; WebServer has no explicit stop API in all builds,
  // but deleting the instance will free resources. First try close() if available.
  #if defined(ESP8266)
    s_srv->close();
  #endif
  delete s_srv;
  s_srv = nullptr;
}

void keyServer_loop() {
  // Auto-start server when WiFi comes up
  if (!s_srv && WiFi.status() == WL_CONNECTED) {
    keyServer_init();
  }

  // Auto-stop server when WiFi goes down
  if (s_srv && WiFi.status() != WL_CONNECTED) {
    keyServer_stop();
    return;
  }

  if (s_srv) s_srv->handleClient();
}