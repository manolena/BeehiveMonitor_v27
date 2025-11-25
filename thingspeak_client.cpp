#include "thingspeak_client.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SD.h>

// forward to attempt auto connect from stored WiFi credentials
extern void wifi_connectFromPrefs(unsigned long timeoutMs);

// minimal URL-encode helper
static String urlEncode(const String &str) {
  String enc;
  enc.reserve(str.length() * 3);
  for (size_t i = 0; i < str.length(); ++i) {
    char c = str[i];
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') enc += c;
    else if (c == ' ') enc += '+';
    else {
      char tmp[8];
      snprintf(tmp, sizeof(tmp), "%%%02X", (unsigned char)c);
      enc += tmp;
    }
  }
  return enc;
}

// format double with 2 decimals into buffer
static void fmt2(char *buf, size_t bufsz, double v) {
  snprintf(buf, bufsz, "%.2f", v);
}

// append a line (bodyPairs) to the SD-backed queue file
static bool enqueuePost(const String &bodyPairs) {
  if (!SD.begin(SD_CS)) {
#if ENABLE_DEBUG
    Serial.println("[TS] SD.begin failed - cannot enqueue");
#endif
    return false;
  }
  File f = SD.open(TS_QUEUE_FILENAME, FILE_APPEND);
  if (!f) {
#if ENABLE_DEBUG
    Serial.println("[TS] open queue file failed");
#endif
    return false;
  }
  f.println(bodyPairs);
  f.close();
#if ENABLE_DEBUG
  Serial.println("[TS] enqueued post");
#endif
  return true;
}

// try to post via HTTPClient over WiFi
static bool postViaWiFi(const String &postBody) {
  HTTPClient http;
  http.begin("http://api.thingspeak.com/update");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST(postBody);
  String resp = http.getString();
#if ENABLE_DEBUG
  Serial.printf("[TS] HTTP code=%d resp=%s\n", code, resp.c_str());
#endif
  http.end();
  if (code == 200) {
    long vid = resp.toInt();
    return (vid > 0);
  }
  return false;
}

bool sendToThingSpeak(const String &bodyPairs) {
  // Build coordinates field from Preferences
  Preferences p;
  p.begin("beehive", true);
  String latS = p.getString("owm_lat", "");
  String lonS = p.getString("owm_lon", "");
  p.end();
  double lat = DEFAULT_LAT;
  double lon = DEFAULT_LON;
  if (latS.length() && lonS.length()) {
    lat = latS.toDouble();
    lon = lonS.toDouble();
  }
  char latBuf[32], lonBuf[32];
  snprintf(latBuf, sizeof(latBuf), "%.4f", lat);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", lon);
  // coords as "lat lon" (space separated)
  String coords = String(latBuf) + String(" ") + String(lonBuf);
  // use existing urlEncode (which turns spaces -> +)
  String coordsEnc = urlEncode(coords);

  // Build final POST body: api_key + caller pairs + field8
  String post;
  post.reserve(256);
  post += "api_key=";
  post += THINGSPEAK_WRITE_APIKEY;
  if (bodyPairs.length()) {
    post += "&";
    post += bodyPairs;
  }
  post += "&field8=";
  post += coordsEnc;

  // 1) If WiFi connected, post immediately
  if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_DEBUG
    Serial.println("[TS] WiFi connected - posting via WiFi");
#endif
    bool ok = postViaWiFi(post);
    if (ok) return true;
#if ENABLE_DEBUG
    Serial.println("[TS] WiFi post failed - enqueueing");
#endif
    enqueuePost(bodyPairs);  // store original bodyPairs (field8 will be appended when retried)
    return false;
  }

// 2) Try to auto-connect to known WiFi briefly
#if ENABLE_DEBUG
  Serial.println("[TS] WiFi not connected - attempting auto-connect");
#endif
  wifi_connectFromPrefs(8000);
  delay(200);
  if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_DEBUG
    Serial.println("[TS] Auto-connect succeeded - posting via WiFi");
#endif
    bool ok = postViaWiFi(post);
    if (ok) return true;
#if ENABLE_DEBUG
    Serial.println("[TS] WiFi post after auto-connect failed - enqueueing");
#endif
    enqueuePost(bodyPairs);
    return false;
  }

// 3) No WiFi available - enqueue for later retry
#if ENABLE_DEBUG
  Serial.println("[TS] No WiFi - enqueueing post for later");
#endif
  enqueuePost(bodyPairs);
  return false;
}

bool thingspeak_upload_current() {
  // Build body with fields 1..7 from global test_ variables
  char buf[64];
  String b;
  b.reserve(160);

  // field1: weight (kg) 1 decimal
  snprintf(buf, sizeof(buf), "%.1f", test_weight);
  b += "field1=" + urlEncode(String(buf));

  // field2: internal temp 1 decimal
  snprintf(buf, sizeof(buf), "%.1f", test_temp_int);
  b += "&field2=" + urlEncode(String(buf));

  // field3: internal humidity 0 decimals
  snprintf(buf, sizeof(buf), "%.0f", test_hum_int);
  b += "&field3=" + urlEncode(String(buf));

  // field4: external temp 1 decimal
  snprintf(buf, sizeof(buf), "%.1f", test_temp_ext);
  b += "&field4=" + urlEncode(String(buf));

  // field5: external humidity 0 decimals
  snprintf(buf, sizeof(buf), "%.0f", test_hum_ext);
  b += "&field5=" + urlEncode(String(buf));

  // field6: pressure 0 decimals
  snprintf(buf, sizeof(buf), "%.0f", test_pressure);
  b += "&field6=" + urlEncode(String(buf));

  // field7: battery voltage 2 decimals
  snprintf(buf, sizeof(buf), "%.2f", test_batt_voltage);
  b += "&field7=" + urlEncode(String(buf));

  return sendToThingSpeak(b);
}

// Attempt to flush queued posts from SD. Only runs when WiFi is connected.
void retryQueuedThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!SD.begin(SD_CS)) {
#if ENABLE_DEBUG
    Serial.println("[TS] SD.begin failed - cannot retry queue");
#endif
    return;
  }

  File f = SD.open(TS_QUEUE_FILENAME, FILE_READ);
  if (!f) return;  // nothing to do

  // Read all lines into memory (small queue expected). Keep lines that fail.
  std::vector<String> remaining;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    // attempt send
    bool ok = sendToThingSpeak(line);
    if (!ok) {
      remaining.push_back(line);
      // if send failed, we stop further sends to avoid hammering; break to rewrite remaining
      break;
    }
    // success -> continue with next
  }
  f.close();

  // Rewrite queue file with remaining entries
  if (remaining.empty()) {
    SD.remove(TS_QUEUE_FILENAME);
#if ENABLE_DEBUG
    Serial.println("[TS] queue flushed");
#endif
  } else {
    File fw = SD.open(TS_QUEUE_FILENAME, FILE_WRITE);
    if (fw) {
      fw.seek(0);
      for (auto &ln : remaining) {
        fw.println(ln);
      }
      fw.close();
    }
  }
}