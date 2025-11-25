#ifndef LCD_SERVER_SIMPLE_H
#define LCD_SERVER_SIMPLE_H

#include <Arduino.h>

// Simple LCD JSON server (standalone).
// API:
void lcd_set_line_simple(uint8_t idx, const String &text);
String lcd_get_line_simple(uint8_t idx);

#endif // LCD_SERVER_SIMPLE_H