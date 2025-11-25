#include "thingspeak_client.h"
#include "config.h"
#include "modem_manager.h"
#include <TinyGsmClient.h>

// Exposed function used by serial command handler to POST via modem.
// Returns true on success (ThingSpeak returns numeric id > 0).
bool thingspeak_post_via_modem(const String &postBody) {
  TinyGsm &modem = modem_get();
  TinyGsmClient client(modem);
  client.setTimeout(15000); // 15s

  #if ENABLE_DEBUG
    Serial.println("[TS-MODEM] Connecting to api.thingspeak.com:80 ...");
  #endif

  if (!client.connect("api.thingspeak.com", 80)) {
    #if ENABLE_DEBUG
      Serial.println("[TS-MODEM] client.connect failed");
    #endif
    return false;
  }

  // build HTTP POST request
  String req;
  req.reserve(postBody.length() + 200);
  req  = String("POST /update HTTP/1.1\r\n");
  req += String("Host: api.thingspeak.com\r\n");
  req += String("Content-Type: application/x-www-form-urlencoded\r\n");
  req += String("Content-Length: ") + String(postBody.length()) + String("\r\n");
  req += String("Connection: close\r\n\r\n");
  req += postBody;

  client.print(req);

  #if ENABLE_DEBUG
    Serial.println("[TS-MODEM] Sent HTTP POST, waiting for response...");
  #endif

  unsigned long start = millis();
  String resp;
  while (millis() - start < 8000) {
    while (client.available()) {
      char c = client.read();
      resp += c;
      if (resp.length() > 4096) break;
    }
    if (resp.length()) break;
    delay(50);
  }

  client.stop();

  #if ENABLE_DEBUG
    if (resp.length()) {
      Serial.print("[TS-MODEM] HTTP response (truncated 1024): ");
      if (resp.length() > 1024) Serial.println(resp.substring(0,1024));
      else Serial.println(resp);
    } else {
      Serial.println("[TS-MODEM] HTTP response empty");
    }
  #endif

  // Check HTTP status (200) and parse body for numeric id
  if (resp.indexOf("HTTP/1.1 200") >= 0 || resp.indexOf("HTTP/1.0 200") >= 0) {
    int pos = resp.lastIndexOf("\r\n\r\n");
    String body = (pos >= 0) ? resp.substring(pos + 4) : resp;
    body.trim();
    long vid = body.toInt();
    return (vid > 0);
  }

  return false;
}