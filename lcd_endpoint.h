#ifndef LCD_ENDPOINT_H
#define LCD_ENDPOINT_H

#include <Arduino.h>
#include <WebServer.h>

// Register the /lcd.json endpoint on the provided WebServer instance.
// Call this from your key_server setup() after the server instance exists:
//    register_lcd_endpoint(server);
void register_lcd_endpoint(WebServer &srv);

// Small API to update/read the 4 cached LCD lines.
// Call lcd_set_line() wherever your code writes to the physical LCD so the
// web endpoint always reflects what the display shows.
void lcd_set_line(uint8_t idx, const String &text);
String lcd_get_line(uint8_t idx);

#endif // LCD_ENDPOINT_H