// 2025-11-22 14:27:10 UTC
// Updated: robust JSON escaping / UTF-8 conversion to avoid "Bad control character in string literal"
// Produces valid UTF-8 JSON even when the LCD buffer contains raw single-byte characters
// (e.g. lcd.write(223) on the physical LCD). Only this file changed.

#include "lcd_server_simple.h"
#include <WiFi.h>

#ifndef LCD_SERVER_PORT
  #define LCD_SERVER_PORT 8080
#endif

// Internal storage for the last 4 LCD lines (initial placeholders)
static String _lcd_lines[4] = {
  "....................",
  "....................",
  "....................",
  "...................."
};

void lcd_set_line_simple(uint8_t idx, const String &text) {
  if (idx >= 4) return;
  String s = text;
  if (s.length() > 20) s = s.substring(0, 20);
  _lcd_lines[idx] = s;
}

String lcd_get_line_simple(uint8_t idx) {
  if (idx >= 4) return String();
  return _lcd_lines[idx];
}

// Improved escape: ensures returned JSON is valid UTF-8.
// - Escapes " and \, \n, \r
// - Replaces control chars (< 0x20) with space
// - Converts LCD raw byte 223 (0xDF) to degree sign U+00B0 (0xC2 0xB0)
// - Converts other bytes >= 0x80 using Latin-1 -> UTF-8 mapping (two-byte)
static String escapeJSON(const String& s) {
  String r;
  r.reserve(s.length() * 2);
  for (size_t i = 0; i < s.length(); ++i) {
    uint8_t b = (uint8_t)s[i];
    if (b == '\"') {
      r += "\\\"";
    } else if (b == '\\') {
      r += "\\\\";
    } else if (b == '\n') {
      r += "\\n";
    } else if (b == '\r') {
      r += "\\r";
    } else if (b < 0x20) {
      // control characters are not allowed unescaped in JSON string literals — replace with space
      r += ' ';
    } else if (b == 223) {
      // Special-case: many HD44780 LCDs map 223 to the degree glyph.
      r += (char)0xC2;
      r += (char)0xB0; // U+00B0
    } else if (b < 0x80) {
      // ASCII printable
      r += (char)b;
    } else {
      // Map single-byte Latin-1 to UTF-8 two-byte sequence
      char hi = (char)(0xC0 | (b >> 6));
      char lo = (char)(0x80 | (b & 0x3F));
      r += hi;
      r += lo;
    }
  }
  return r;
}

// Server task ---------------------------------------------------------------
static TaskHandle_t lcdServerTaskHandle = NULL;

static void lcd_server_task(void *pvParameters) {
  (void) pvParameters;
  WiFiServer server(LCD_SERVER_PORT);
  bool serverStarted = false;

  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!serverStarted) {
        server.begin();
        server.setNoDelay(true);
        serverStarted = true;
#if ENABLE_DEBUG
        Serial.printf("[LCD-8080] lcd server started on port %d, IP: %s\n", LCD_SERVER_PORT, WiFi.localIP().toString().c_str());
#endif
      }
    } else {
      if (serverStarted) {
        server.stop();
        serverStarted = false;
#if ENABLE_DEBUG
        Serial.println(F("[LCD-8080] WiFi lost - stopped lcd server"));
#endif
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    WiFiClient client = server.available();
    if (!client) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

#if ENABLE_DEBUG
    Serial.println(F("[LCD-8080] client connected"));
#endif

    unsigned long start = millis();
    String req;
    req.reserve(256);
    while (client.connected() && (millis() - start) < 2000) {
      while (client.available()) {
        char c = client.read();
        req += c;
        if (req.endsWith("\r\n\r\n")) break;
      }
      if (req.endsWith("\r\n\r\n")) break;
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    String firstLine;
    int nl = req.indexOf('\n');
    if (nl >= 0) firstLine = req.substring(0, nl);
    firstLine.trim();

    if (!firstLine.startsWith("GET ")) {
      client.print("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n");
      client.stop();
#if ENABLE_DEBUG
      Serial.println(F("[LCD-8080] bad request"));
#endif
      continue;
    }

    int sp1 = firstLine.indexOf(' ');
    int sp2 = firstLine.indexOf(' ', sp1 + 1);
    String path = "/";
    if (sp1 >= 0 && sp2 > sp1) path = firstLine.substring(sp1 + 1, sp2);

    if (path == "/lcd.json") {
      String json = String("{\"lines\":[\"")
        + escapeJSON(_lcd_lines[0]) + String("\",\"")
        + escapeJSON(_lcd_lines[1]) + String("\",\"")
        + escapeJSON(_lcd_lines[2]) + String("\",\"")
        + escapeJSON(_lcd_lines[3]) + String("\"],\"ts\":\"")
        + String(millis()) + String("\"}");

      client.print("HTTP/1.1 200 OK\r\n");
      client.print("Content-Type: application/json; charset=utf-8\r\n");
      client.print("Access-Control-Allow-Origin: *\r\n");
      client.print("Connection: close\r\n");
      client.print("Content-Length: ");
      client.print(json.length());
      client.print("\r\n\r\n");
      client.print(json);
      client.stop();
#if ENABLE_DEBUG
      Serial.println(F("[LCD-8080] served /lcd.json"));
#endif
    } else {
      client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
      client.stop();
#if ENABLE_DEBUG
      Serial.printf("[LCD-8080] 404 for %s\n", path.c_str());
#endif
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  } // loop
}

// Create the server task once at static init time.
static bool lcd_server_task_created = []()->bool {
  BaseType_t r = xTaskCreatePinnedToCore(
    lcd_server_task,
    "LCD8080",
    4096,
    NULL,
    1,
    &lcdServerTaskHandle,
    1
  );
#if ENABLE_DEBUG
  if (r == pdPASS) {
    Serial.println(F("[LCD-8080] server task created"));
    return true;
  } else {
    Serial.println(F("[LCD-8080] failed to create server task"));
    return false;
  }
#else
  return (r == pdPASS);
#endif
}();