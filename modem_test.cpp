/*
  modem_test.cpp - non-invasive modem test helper for BeeHive project
  Usage:
    - Add this file to the sketch folder.
    - From loop() add a small Serial trigger (see instructions below) or call the functions manually.
    - Make sure modemManager_init() already ran in setup() (your main setup does this).
*/

#include <Arduino.h>
#include "modem_manager.h"
#include <TinyGsmClient.h>

// Timeout helpers
static void wait_ms(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    delay(10);
  }
}

// Print current modem status: operator, RSSI, registration
void modem_test_status() {
  Serial.println("[MODEM-TEST] Checking modem status...");
  TinyGsm &m = modem_get();
  // Operator
  String op = m.getOperator();
  Serial.print("[MODEM-TEST] Operator: "); Serial.println(op);
  // RSSI / signal quality
  int16_t sq = m.getSignalQuality();
  Serial.print("[MODEM-TEST] Signal quality (RSSI-like): "); Serial.println(sq);
  // Registration
  int reg = m.getRegistrationStatus();
  Serial.print("[MODEM-TEST] Registration status: "); Serial.println(reg);
  // registration values: 0=unknown, 1=registered home, 5=registered roaming, 2-searching, 3-denied, 4-off
}

// Attempt to attach GPRS using APN and then perform a raw HTTP GET using TinyGsmClient.
// Returns true if GET succeeded (got a response), false otherwise.
bool modem_test_http_get(const char *apn = "internet", const char *user = "", const char *pass = "", const char *host = "example.com", uint16_t port = 80, const char *path = "/") {
  Serial.printf("[MODEM-TEST] Attempting GPRS attach with APN='%s'\n", apn);
  TinyGsm &m = modem_get();

  // Try GPRS connect (this is non-blocking for a short timeout)
  bool ok = m.gprsConnect(apn, user, pass);
  Serial.print("[MODEM-TEST] gprsConnect() returned: "); Serial.println(ok ? "OK" : "FAIL");
  if (!ok) {
    Serial.println("[MODEM-TEST] GPRS attach failed - check APN/coverage/power.");
    return false;
  }

  // Use TinyGsmClient for a raw HTTP GET
  TinyGsmClient client(m);

  Serial.printf("[MODEM-TEST] Connecting to %s:%u ...\n", host, port);
  if (!client.connect(host, port)) {
    Serial.println("[MODEM-TEST] TCP connect failed");
    m.gprsDisconnect();
    return false;
  }

  // Send a minimal HTTP/1.0 GET request (simple)
  String req = String("GET ") + path + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
  client.print(req);

  // Read response status line (timeout-protected)
  unsigned long start = millis();
  String statusLine = "";
  while (client.available() == 0 && millis() - start < 7000) {
    delay(10);
  }
  start = millis();
  while (client.available() && millis() - start < 7000) {
    char c = client.read();
    statusLine += c;
    if (statusLine.endsWith("\r\n")) break;
  }
  if (statusLine.length() == 0) {
    Serial.println("[MODEM-TEST] No HTTP response received");
    client.stop();
    m.gprsDisconnect();
    return false;
  }
  Serial.print("[MODEM-TEST] HTTP response (prefix): ");
  Serial.println(statusLine);

  // Print first ~512 bytes of response body/headers
  Serial.println("[MODEM-TEST] HTTP response (first bytes):");
  start = millis();
  int printed = 0;
  while (client.available() && millis() - start < 5000 && printed < 512) {
    char c = client.read();
    Serial.write(c);
    printed++;
  }
  Serial.println();
  client.stop();
  m.gprsDisconnect();
  Serial.println("[MODEM-TEST] Done, disconnected GPRS.");
  return true;
}