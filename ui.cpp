#include "config.h"
#include "ui.h"                // bring Language, Button, prototypes into scope
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "lcd_endpoint.h"      // update web mirror when UI prints
#include "lcd_server_simple.h" // update standalone lcd server on :8080 as well
#include "greek_utils.h"

extern LiquidCrystal_I2C lcd;

// Fixed-size array structure to replace std::vector
#define MAX_VISUAL_CELLS 20
#define PROGMEM_BUFFER_SIZE 96

struct VisualCells {
    String cells[MAX_VISUAL_CELLS];
    int count;
    
    VisualCells() : count(0) {}
    
    void clear() { count = 0; }
    void push_back(const String& s) {
        if (count < MAX_VISUAL_CELLS) {
            cells[count++] = s;
        }
    }
    int size() const { return count; }
    const String& operator[](int i) const { 
        if (i < 0 || i >= MAX_VISUAL_CELLS) i = 0; // bounds check
        return cells[i]; 
    }
    String& operator[](int i) { 
        if (i < 0 || i >= MAX_VISUAL_CELLS) i = 0; // bounds check
        return cells[i]; 
    }
    void resize(int n) {
        if (n < count) count = n;
        while (count < n && count < MAX_VISUAL_CELLS) cells[count++] = String(" ");
    }
};

// Split a UTF-8 string into visual "cells" (each entry is one displayed column — ASCII or full multibyte seq)
// Returns at most maxCells entries (if maxCells <= 0 -> unlimited, but capped at 20)
static void splitVisual(const String &s, VisualCells &out, int maxCells = -1) {
    out.clear();
    const char *p = s.c_str();
    int limit = (maxCells < 0) ? 20 : maxCells;
    while (*p && out.size() < limit) {
        uint8_t c = (uint8_t)*p;
        if (c < 0x80) {
            out.push_back(String((char)*p));
            p++;
        } else if ((c & 0xE0) == 0xC0) {
            if (p[1] == 0) break;
            char buf[3] = {p[0], p[1], 0};
            out.push_back(String(buf));
            p += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (p[1] == 0 || p[2] == 0) break;
            char buf[4] = {p[0], p[1], p[2], 0};
            out.push_back(String(buf));
            p += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (p[1] == 0 || p[2] == 0 || p[3] == 0) break;
            char buf[5] = {p[0], p[1], p[2], p[3], 0};
            out.push_back(String(buf));
            p += 4;
        } else {
            // unknown sequence - treat as single byte
            out.push_back(String((char)*p));
            p++;
        }
    }
}

// Join visual cells back into a UTF-8 string; pad to exactly 20 visual columns with spaces
static String joinVisualPad(const VisualCells &cells, int padTo = 20) {
    String r;
    r.reserve(padTo * 3);
    int filled = 0;
    for (int i = 0; i < cells.size() && filled < padTo; ++i) {
        r += cells[i];
        filled++;
    }
    while (filled < padTo) { r += ' '; filled++; }
    return r;
}

// Get current web line (may be empty) and produce a vector of its visual cells (always 20 cells)
static void getExistingLineCells(uint8_t row, VisualCells &cells) {
    String cur = lcd_get_line(row);
    // If underlying endpoint isn't initialized yet, ensure 20 spaces
    if (cur.length() == 0) {
        cells.clear();
        for (int i = 0; i < 20; ++i) cells.push_back(String(" "));
        return;
    }
    splitVisual(cur, cells, 20);
    // pad to 20 cells
    while (cells.size() < 20) cells.push_back(String(" "));
    if (cells.size() > 20) cells.resize(20);
}

// Insert text (UTF-8) at given visual column (col), row, replacing existing cells as needed.
// This writes the resulting 20-column string to BOTH lcd_endpoint and lcd_server_simple buffers.
static void setWebTextAt(uint8_t col, uint8_t row, const String &text) {
    if (row >= 4) return;
    if (col >= 20) return;

    VisualCells existing;
    getExistingLineCells(row, existing); // 20 cells

    VisualCells newCells;
    splitVisual(text, newCells, 20 - col);

    for (int i = 0; i < newCells.size() && (col + i) < 20; ++i) {
        existing[col + i] = newCells[i];
    }

    String result = joinVisualPad(existing, 20);

    // write to both web buffers so :80 and :8080 reflect same content
    lcd_set_line(row, result);
    lcd_set_line_simple(row, result);
}

// --------------------------------------------------
// BUTTON HANDLING
// --------------------------------------------------
Button getButton()
{
    static uint32_t lastTime = 0;
    static bool upLast   = true;
    static bool downLast = true;
    static bool selLast  = true;
    static bool backLast = true;

    uint32_t now = millis();
    if (now - lastTime < 120)   // strong debounce
        return BTN_NONE;
    lastTime = now;

    bool upNow   = digitalRead(BTN_UP);
    bool downNow = digitalRead(BTN_DOWN);
    bool selNow  = digitalRead(BTN_SELECT);
    bool backNow = digitalRead(BTN_BACK);

    if (upLast && !upNow)       { upLast = upNow;   return BTN_UP_PRESSED; }
    if (downLast && !downNow)   { downLast = downNow; return BTN_DOWN_PRESSED; }
    if (selLast && !selNow)     { selLast = selNow; return BTN_SELECT_PRESSED; }
    if (backLast && !backNow)   { backLast = backNow; return BTN_BACK_PRESSED; }

    upLast   = upNow;
    downLast = downNow;
    selLast  = selNow;
    backLast = backNow;

    return BTN_NONE;
}

// --------------------------------------------------
// UI INIT
// --------------------------------------------------
void uiInit() {
    pinMode(BTN_UP,     INPUT_PULLUP);
    pinMode(BTN_DOWN,   INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK,   INPUT_PULLUP);

    lcd.init();
    lcd.backlight();
    lcd.clear();

    initGreekChars();

    // Ensure web buffer starts clean (20 spaces per line)
    for (uint8_t i = 0; i < 4; ++i) {
        String empty20;
        for (int k = 0; k < 20; ++k) empty20 += ' ';
        lcd_set_line(i, empty20);
        lcd_set_line_simple(i, empty20);
    }
}

void uiClear() {
    lcd.clear();
    // clear web buffer as well (20 spaces)
    for (uint8_t i = 0; i < 4; ++i) {
        String empty20;
        for (int k = 0; k < 20; ++k) empty20 += ' ';
        lcd_set_line(i, empty20);
        lcd_set_line_simple(i, empty20);
    }
}

// --------------------------------------------------
// PRINT WRAPPER (EN/GR)
// --------------------------------------------------
void uiPrint(uint8_t col, uint8_t row, const char *msg) {
    // Update both the physical display and the web mirror at the correct column.
    String s = String(msg);

    if (currentLanguage == LANG_GR) {
        // For Greek text, use lcdPrintGreek which handles physical mapping and then updates web buffer.
        lcdPrintGreek(s.c_str(), col, row);
        return;
    } else {
        // For English (and other Latin text) we print to LCD handling special UTF-8 sequences like degree sign.
        const char *p = s.c_str();
        lcd.setCursor(col, row);
        while (*p) {
            uint8_t b = (uint8_t)*p;
            // Detect UTF-8 degree sign U+00B0 -> 0xC2 0xB0
            if (b == 0xC2 && (uint8_t)p[1] == 0xB0) {
                lcd.write((uint8_t)DEGREE_SYMBOL_LCD);
                p += 2;
            } else {
                // simple ascii/byte print
                lcd.write(*p);
                p++;
            }
        }
    }

    // Update web buffer at precise column (web expects UTF-8 degree char present in s)
    setWebTextAt(col, row, s);
}

// --------------------------------------------------
// GREEK CHAR SYSTEM
// --------------------------------------------------
void initGreekChars() {
  byte Gamma[8]  = { B11111, B10000, B10000, B10000, B10000, B10000, B10000, B00000 };
  byte Delta[8]  = { B00100, B01010, B10001, B10001, B10001, B10001, B11111, B00000 };
  byte Lambda[8] = { B00100, B01010, B10001, B10001, B10001, B10001, B10001, B00000 };
  byte Xi[8]     = { B11111, B00000, B00000, B01110, B00000, B00000, B11111, B00000 };
  byte Pi[8]     = { B11111, B10001, B10001, B10001, B10001, B10001, B10001, B00000 };
  byte Phi[8]    = { B01110, B10101, B10101, B10101, B01110, B00100, B00100, B00000 };
  byte Psi[8]    = { B10101, B10101, B10101, B01110, B00100, B00100, B00100, B00000 };
  byte Omega[8]  = { B01110, B10001, B10001, B10001, B01110, B00000, B11111, B00000 };

  lcd.createChar(0, Gamma);
  lcd.createChar(1, Delta);
  lcd.createChar(2, Lambda);
  lcd.createChar(3, Xi);
  lcd.createChar(4, Pi);
  lcd.createChar(5, Phi);
  lcd.createChar(6, Psi);
  lcd.createChar(7, Omega);
}

// Replace the beginning of lcdPrintGreek(...) body with the following:
void lcdPrintGreek(const char *utf8str, uint8_t col, uint8_t row) {
    // Convert input to uppercase Greek (UTF-8 aware) so LCD mapping uses CGRAM custom chars.
    String up = greekToUpper(String(utf8str));
    const char *s = up.c_str();

    // then use 's' instead of 'utf8str' in the rest of the function
    lcd.setCursor(col, row);
    const char *p = s;

    while (*p) {
        // Special-case UTF-8 degree sign for physical LCD
        if ((uint8_t)*p == 0xC2 && (uint8_t)p[1] == 0xB0) {
            lcd.write((uint8_t)DEGREE_SYMBOL_LCD);
            p += 2;
            continue;
        }

        if ((uint8_t)*p == 0xCE || (uint8_t)*p == 0xCF) {
            uint8_t first = (uint8_t)*p;
            p++;
            uint8_t second = (uint8_t)*p;

            if (first == 0xCE) {
                switch (second) {
                    case 0x91: lcd.write('A'); break;      // Α
                    case 0x92: lcd.write('B'); break;      // Β
                    case 0x93: lcd.write((uint8_t)0); break; // Γ
                    case 0x94: lcd.write((uint8_t)1); break; // Δ
                    case 0x95: lcd.write('E'); break;      // Ε
                    case 0x96: lcd.write('Z'); break;      // Ζ
                    case 0x97: lcd.write('H'); break;      // Η
                    case 0x98: lcd.write(242); break;      // Θ
                    case 0x99: lcd.write('I'); break;      // Ι
                    case 0x9A: lcd.write('K'); break;      // Κ
                    case 0x9B: lcd.write((uint8_t)2); break; // Λ
                    case 0x9C: lcd.write('M'); break;      // Μ
                    case 0x9D: lcd.write('N'); break;      // Ν
                    case 0x9E: lcd.write((uint8_t)3); break; // Ξ
                    case 0x9F: lcd.write('O'); break;      // Ο
                    case 0xA0: lcd.write((uint8_t)4); break; // Π
                    case 0xA1: lcd.write('P'); break;      // Ρ
                    case 0xA3: lcd.write(246); break;      // Σ
                    case 0xA4: lcd.write('T'); break;      // Τ
                    case 0xA5: lcd.write('Y'); break;      // Υ
                    case 0xA6: lcd.write((uint8_t)5); break; // Φ
                    case 0xA7: lcd.write('X'); break;      // Χ
                    case 0xA8: lcd.write((uint8_t)6); break; // Ψ
                    case 0xA9: lcd.write((uint8_t)7); break; // Ω
                    default: lcd.write('?'); break;
                }
            } else {
                // 0xCF sequences (accented letters) — best effort: map to plain Latin approx or '?'
                switch (second) {
                  case 0x80: lcd.write('P'); break; // π -> P
                  case 0x81: lcd.write('R'); break; // ρ -> R
                  case 0x82: lcd.write(246); break; // ς/σ -> mapped to Σ char or approximation
                  case 0x83: lcd.write(246); break; // σ
                  default: lcd.write('?'); break;
                }
            }
            p++;
        } else {
            lcd.write(*p);
            p++;
        }
    }

    // Update web mirror with the original UTF-8 uppercase string (padded/truncated)
    String webLine = String(s);
    setWebTextAt(col, row, webLine);
}

// --------------------------------------------------
// PROGMEM HELPERS
// --------------------------------------------------
// Copy flash string into stack buffer and call regular uiPrint
void uiPrint_P(const __FlashStringHelper *f, uint8_t col, uint8_t row) {
    char buf[PROGMEM_BUFFER_SIZE];
    strncpy_P(buf, (const char*)f, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    uiPrint(col, row, buf);
}

// Copy flash string into stack buffer and call regular lcdPrintGreek
void lcdPrintGreek_P(const __FlashStringHelper *f, uint8_t col, uint8_t row) {
    char buf[PROGMEM_BUFFER_SIZE];
    strncpy_P(buf, (const char*)f, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    lcdPrintGreek(buf, col, row);
}