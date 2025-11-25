#include "serial_commands.h"
#include "sms_handler.h"
#include "thingspeak_client.h"
#include "config.h"
#include <WiFi.h>
#include <SD.h>
#include <Preferences.h>
#include "modem_manager.h"
#include <TinyGsmClient.h>

// Forward to modem post function implemented in thingspeak_client_modem.cpp
extern bool thingspeak_post_via_modem(const String &postBody);

static String inputLine;

void serial_commands_init() {
  Serial.println(F("[CMD] Serial commands ready. Type 'sms' to scan SMS, 'ts status' for TS status, 'ts send' to force upload, 'ts send-lte' to force LTE upload, 'modem test' for modem diag, 'help' for help."));
}

// non-blocking read of one line from Serial
static String readSerialLineNonBlocking() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (inputLine.length() == 0) return String();
      String out = inputLine;
      inputLine = String();
      return out;
    }
    inputLine += c;
    if (inputLine.length() > 200) inputLine = inputLine.substring(0, 200);
  }
  return String();
}

static void printTSQueueStatus() {
  if (!SD.begin(SD_CS)) {
    Serial.println(F("[TS STATUS] SD not available"));
    return;
  }
  if (!SD.exists(TS_QUEUE_FILENAME)) {
    Serial.println(F("[TS STATUS] No queued posts (no /ts_queue.txt)"));
    return;
  }
  File f = SD.open(TS_QUEUE_FILENAME, FILE_READ);
  if (!f) {
    Serial.println(F("[TS STATUS] Cannot open /ts_queue.txt"));
    return;
  }
  Serial.println(F("[TS STATUS] /ts_queue.txt contents:"));
  int i = 0;
  while (f.available()) {
    String ln = f.readStringUntil('\n');
    ln.trim();
    if (ln.length() == 0) continue;
    Serial.print("#");
    Serial.print(++i);
    Serial.print(": ");
    Serial.println(ln);
    if (i >= 50) {
      Serial.println(F("[TS STATUS] ... truncated after 50 lines"));
      break;
    }
  }
  f.close();
}

// --- Modem diagnostic helper (used by 'modem test') ---
static String modemReadStream(Stream &s, unsigned long timeout = 1000) {
  unsigned long t0 = millis();
  String resp;
  while (millis() - t0 < timeout) {
    while (s.available()) {
      char c = s.read();
      resp += c;
    }
    delay(5);
  }
  return resp;
}

static void runModemDiag() {
  Serial.println(F("[MODEM DIAG] Starting modem diagnostics..."));
  TinyGsm &modem = modem_get();
  Stream &s = modem.stream;

  // 1) Basic AT check
  modem.sendAT("AT");
  Serial.print(F("[MODEM DIAG] AT -> "));
  Serial.println(modemReadStream(s, 800));

  // 2) CGATT? attached?
  modem.sendAT("AT+CGATT?");
  Serial.print(F("[MODEM DIAG] AT+CGATT? -> "));
  Serial.println(modemReadStream(s, 800));

  // 3) PDP context definitions
  modem.sendAT("AT+CGDCONT?");
  Serial.print(F("[MODEM DIAG] AT+CGDCONT? -> "));
  Serial.println(modemReadStream(s, 800));

  // 4) Active PDP addresses (some modems)
  modem.sendAT("AT+CGPADDR");
  Serial.print(F("[MODEM DIAG] AT+CGPADDR -> "));
  Serial.println(modemReadStream(s, 800));

  // 5) Try TCP connect to ThingSpeak using TinyGsmClient
  Serial.println(F("[MODEM DIAG] Trying TCP connect to api.thingspeak.com:80"));

  TinyGsmClient client(modem);
  client.setTimeout(5000);
  bool conn = client.connect("api.thingspeak.com", 80);
  Serial.print(F("[MODEM DIAG] client.connect -> "));
  Serial.println(conn ? F("OK") : F("FAIL"));

  if (conn) {
    String req = String("GET /update?api_key=test&field1=1 HTTP/1.1\r\nHost: api.thingspeak.com\r\nConnection: close\r\n\r\n");
    client.print(req);
    Serial.println(F("[MODEM DIAG] Sent HTTP GET, waiting for response..."));
    unsigned long t0 = millis();
    String resp;
    while (millis() - t0 < 8000) {
      while (client.available()) {
        char c = client.read();
        resp += c;
      }
      if (resp.length()) break;
      delay(50);
    }
    if (resp.length()) {
      Serial.println(F("[MODEM DIAG] HTTP response (truncated 1024 chars):"));
      if (resp.length() > 1024) {
        Serial.println(resp.substring(0, 1024));
        Serial.println(F("... (truncated)"));
      } else {
        Serial.println(resp);
      }
    } else {
      Serial.println(F("[MODEM DIAG] No HTTP response received."));
    }
    client.stop();
  } else {
    Serial.println(F("[MODEM DIAG] TCP connect failed - no data path or DNS issue."));
  }

  Serial.println(F("[MODEM DIAG] Done."));
}

// Simple URL-encode for coordinates (space -> +, others left as-is)
static String urlEncodeSimple(const String &s) {
  String r = s;
  r.replace(" ", "+");
  return r;
}

void serial_commands_poll() {
  String ln = readSerialLineNonBlocking();
  if (ln.length() == 0) return;
  ln.trim();
  if (ln.length() == 0) return;
  String up = ln;
  up.toUpperCase();

  if (up == "SMS" || up == "SCAN" || up == "SCAN SMS" || up == "SMS SCAN") {
    Serial.println(F("[CMD] Triggering manual SMS scan..."));
    sms_scan_now();
    return;
  }
  if (up == "HELP" || up == "?") {
    Serial.println(F("[CMD] Commands:"));
    Serial.println(F("  sms            -> trigger immediate SMS scan"));
    Serial.println(F("  ts status      -> print ThingSpeak/WiFi/queue status"));
    Serial.println(F("  ts send        -> trigger immediate ThingSpeak upload (WiFi-first path)"));
    Serial.println(F("  ts send-lte    -> trigger ThingSpeak upload via MODEM (LTE, manual)"));
    Serial.println(F("  modem test     -> run modem diagnostics (AT cmds + TCP test)"));
    Serial.println(F("  help           -> print this help"));
    return;
  }

  if (up == "TS STATUS" || up == "TSSTATUS") {
    Serial.println(F("[CMD] ThingSpeak / connectivity status:"));
    Serial.print(F("  WiFi.status(): "));
    Serial.println(WiFi.status());
    IPAddress ip = WiFi.localIP();
    Serial.print(F("  localIP: "));
    Serial.println(ip);
    printTSQueueStatus();
    if (strlen(THINGSPEAK_WRITE_APIKEY) == 0) {
      Serial.println(F("  THINGSPEAK_WRITE_APIKEY: (empty)"));
    } else {
      Serial.println(F("  THINGSPEAK_WRITE_APIKEY: set"));
    }
    return;
  }

  if (up == "TS SEND" || up == "TSSEND") {
    Serial.println(F("[CMD] Triggering manual ThingSpeak upload (WiFi-first path)..."));
    bool ok = thingspeak_upload_current();
    if (ok) Serial.println(F("[CMD] ThingSpeak upload: SUCCESS (immediate)"));
    else Serial.println(F("[CMD] ThingSpeak upload: FAILED or queued"));
    return;
  }

  if (up == "TS SEND-LTE" || up == "TSSENDLTE") {
    Serial.println(F("[CMD] Triggering ThingSpeak upload via MODEM (LTE)..."));

    // Build post body same as thingspeak_upload_current() would
    char buf[64];
    String b;
    b.reserve(200);

    // field1: weight (kg) 1 decimal
    snprintf(buf, sizeof(buf), "%.1f", test_weight);
    b += "field1=" + String(buf);

    // field2: internal temp 1 decimal
    snprintf(buf, sizeof(buf), "%.1f", test_temp_int);
    b += "&field2=" + String(buf);

    // field3: internal humidity 0 decimals
    snprintf(buf, sizeof(buf), "%.0f", test_hum_int);
    b += "&field3=" + String(buf);

    // field4: external temp 1 decimal
    snprintf(buf, sizeof(buf), "%.1f", test_temp_ext);
    b += "&field4=" + String(buf);

    // field5: external humidity 0 decimals
    snprintf(buf, sizeof(buf), "%.0f", test_hum_ext);
    b += "&field5=" + String(buf);

    // field6: pressure 0 decimals
    snprintf(buf, sizeof(buf), "%.0f", test_pressure);
    b += "&field6=" + String(buf);

    // field7: battery voltage 2 decimals
    snprintf(buf, sizeof(buf), "%.2f", test_batt_voltage);
    b += "&field7=" + String(buf);

    // coords from preferences
    Preferences p;
    p.begin("beehive", true);
    String latS = p.getString("owm_lat", "");
    String lonS = p.getString("owm_lon", "");
    p.end();
    double lat = DEFAULT_LAT, lon = DEFAULT_LON;
    if (latS.length() && lonS.length()) {
      lat = latS.toDouble();
      lon = lonS.toDouble();
    }
    char latBuf[32], lonBuf[32];
    snprintf(latBuf, sizeof(latBuf), "%.4f", lat);
    snprintf(lonBuf, sizeof(lonBuf), "%.4f", lon);
    String coords = String(latBuf) + String(" ") + String(lonBuf);
    String coordsEnc = urlEncodeSimple(coords);  // or urlEncodeSimple(coords) -> ensure it encodes space as +
    b += "&field8=" + coordsEnc;

    // Prepend api_key
    String post = String("api_key=") + THINGSPEAK_WRITE_APIKEY + String("&") + b;

    // Call modem poster
    bool ok = thingspeak_post_via_modem(post);
    if (ok) Serial.println(F("[CMD] ThingSpeak upload via MODEM: SUCCESS"));
    else Serial.println(F("[CMD] ThingSpeak upload via MODEM: FAILED"));

    return;
  }

  if (up == "MODEM TEST" || up == "MODEMTEST") {
    Serial.println(F("[CMD] Running modem diagnostics..."));
    runModemDiag();
    return;
  }

  Serial.print(F("[CMD] Unknown command: "));
  Serial.println(ln);
  Serial.println(F("Type 'help' for available commands."));
}