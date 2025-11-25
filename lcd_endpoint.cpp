// url=https://github.com/manolena/Beehive-Monitor/blob/main/lcd_endpoint.cpp
#include "lcd_endpoint.h"
#include <WebServer.h>

// Internal storage for the last 4 LCD lines (initial placeholders)
static String lcd_lines[4] = {
  "....................",
  "....................",
  "....................",
  "...................."
};

// UTF‑8 aware truncation/pad (same logic as ui.cpp pad20)
static String pad20_local(const String &s_in) {
    String out;
    out.reserve(32);
    const char *p = s_in.c_str();
    int visual = 0;
    while (*p && visual < 20) {
        uint8_t c = (uint8_t)*p;
        if (c < 0x80) {
            out += *p++;
            visual++;
        } else if ((c & 0xE0) == 0xC0) {
            if (p[1] == 0) break;
            out += *p++; out += *p++;
            visual++;
        } else if ((c & 0xF0) == 0xE0) {
            if (p[1] == 0 || p[2] == 0) break;
            out += *p++; out += *p++; out += *p++;
            visual++;
        } else if ((c & 0xF8) == 0xF0) {
            if (p[1] == 0 || p[2] == 0 || p[3] == 0) break;
            out += *p++; out += *p++; out += *p++; out += *p++;
            visual++;
        } else {
            out += *p++;
            visual++;
        }
    }
    while (visual < 20) { out += ' '; visual++; }
    return out;
}

// Store up to 20 visual chars UTF-8 per line (pad/truncate safely)
void lcd_set_line(uint8_t idx, const String &text) {
  if (idx >= 4) return;
  String s = pad20_local(text);
  lcd_lines[idx] = s;
}

String lcd_get_line(uint8_t idx) {
  if (idx >= 4) return String();
  return lcd_lines[idx];
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
      r += ' ';
    } else if (b == 223) {
      r += (char)0xC2;
      r += (char)0xB0; // U+00B0
    } else if (b < 0x80) {
      r += (char)b;
    } else {
      char hi = (char)(0xC0 | (b >> 6));
      char lo = (char)(0x80 | (b & 0x3F));
      r += hi;
      r += lo;
    }
  }
  return r;
}

// Register the HTTP GET handler on the provided WebServer instance.
void register_lcd_endpoint(WebServer &srv) {
  srv.on("/lcd.json", HTTP_GET, [&srv]() {
    String json = String("{\"lines\":[\"")
      + escapeJSON(lcd_lines[0]) + String("\",\"")
      + escapeJSON(lcd_lines[1]) + String("\",\"")
      + escapeJSON(lcd_lines[2]) + String("\",\"")
      + escapeJSON(lcd_lines[3]) + String("\"],\"ts\":\"")
      + String(millis()) + String("\"}");

    srv.sendHeader("Access-Control-Allow-Origin", "*");
    srv.send(200, "application/json; charset=utf-8", json);
  });
}