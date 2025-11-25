#pragma once
#include <Arduino.h>

// Initialize serial command handler (optional: prints help)
void serial_commands_init();

// Poll serial input; should be called frequently from loop()
// Recognized commands:
//   sms    -> trigger immediate SMS scan (calls sms_scan_now())
//   help   -> print help
void serial_commands_poll();