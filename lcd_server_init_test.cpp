// Updated: ensure degree symbol is encoded portably for Arduino toolchain.
// This file seeds test lines for /lcd.json at boot (remove when you wire real LCD updates).
#include "lcd_server_simple.h"

// Use explicit UTF-8 byte sequence \xC2\xB0 for the degree sign to ensure it appears
// correctly in JSON/HTML regardless of editor handling of \u escapes.
static bool _lcd_init_test = []()->bool {
  lcd_set_line_simple(0, String("T=23.5\xC2\xB0C"));
  lcd_set_line_simple(1, String("RSSI: 19"));
  lcd_set_line_simple(2, String("SIM: COSMOTE"));
  lcd_set_line_simple(3, String("LCD mirror: OK"));
  Serial.println("[LCD-INIT-TEST] initial test lines set for /lcd.json");
  return true;
}();