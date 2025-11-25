#include "provisioning_server.h"
#include <WebServer.h>
#include <Preferences.h>
#include <Arduino.h>

static WebServer server8080(8080);

static const char wifiFormHtml[] PROGMEM = R"rawliteral(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Beehive WiFi Provision</title>
<style>body{font-family:Arial;margin:12px}label{display:block;margin-top:8px}</style>
</head><body>
<h3>Beehive WiFi Provision</h3>
<form action="/save-wifi" method="POST">
<label>SSID 1<input name="ssid1" type="text" maxlength="64"></label>
<label>Password 1<input name="psk1" type="password" maxlength="64"></label>
<hr>
<label>SSID 2 (backup)<input name="ssid2" type="text" maxlength="64"></label>
<label>Password 2<input name="psk2" type="password" maxlength="64"></label>
<div style="margin-top:12px">
<button type="submit">Save</button>
</div>
</form>
<form action="/reboot" method="POST" style="margin-top:10px"><button type="submit">Save & Reboot</button></form>
</body></html>
)rawliteral";

static void handleRoot() {
  server8080.send_P(200, "text/html", wifiFormHtml);
}

static void handleSaveWifi() {
  if (server8080.method() != HTTP_POST) {
    server8080.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
    return;
  }
  String ssid1 = server8080.arg("ssid1");
  String psk1  = server8080.arg("psk1");
  String ssid2 = server8080.arg("ssid2");
  String psk2  = server8080.arg("psk2");

  Preferences prefs;
  prefs.begin("beehive", false);
  if (ssid1.length()) prefs.putString("wifi_ssid1", ssid1); else prefs.remove("wifi_ssid1");
  if (psk1.length())  prefs.putString("wifi_psk1", psk1);   else prefs.remove("wifi_psk1");
  if (ssid2.length()) prefs.putString("wifi_ssid2", ssid2); else prefs.remove("wifi_ssid2");
  if (psk2.length())  prefs.putString("wifi_psk2", psk2);   else prefs.remove("wifi_psk2");
  prefs.end();

  server8080.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handleReboot() {
  if (server8080.method() != HTTP_POST) {
    server8080.send(405, "application/json", "{\"status\":\"error\",\"message\":\"Method not allowed\"}");
    return;
  }
  server8080.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"rebooting\"}");
  delay(200);
  ESP.restart();
}

void provisioning_init() {
  server8080.on("/", HTTP_GET, handleRoot);
  server8080.on("/save-wifi", HTTP_POST, handleSaveWifi);
  server8080.on("/reboot", HTTP_POST, handleReboot);
  server8080.begin();
  Serial.println(F("[Provision] provisioning server started on port 8080"));
}

void provisioning_loop() {
  server8080.handleClient();
}