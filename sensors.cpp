// sensors.cpp
// Bridge module: does NOT define test_* globals (they are defined in the .ino).
// Provides sensors_init() and sensors_update() that operate on the globals that
// already exist in the sketch (declared extern in config.h).
//
// IMPORTANT: keep your .ino definitions (the placeholders) as-is; this file
// must NOT re-declare/define them.

#include "sensors.h"
#include "config.h"
#include <Preferences.h>

// The actual globals are defined in the .ino (placeholders).
// Here we only declare them as extern so this translation unit can use them.
extern float test_weight;
extern float test_temp_int;
extern float test_hum_int;
extern float test_temp_ext;
extern float test_hum_ext;
extern float test_pressure;
extern float test_acc_x;
extern float test_acc_y;
extern float test_acc_z;
extern float test_batt_voltage;
extern int   test_batt_percent;
extern int   test_rssi;

static bool sensors_initialized = false;

bool sensors_init() {
  if (sensors_initialized) return true;

  #if ENABLE_DEBUG
    Serial.println("[SENS] sensors_init() stub called (no defs here)");
  #endif

  // Initialize buses if you want (Wire.begin etc.) — safe to leave commented until you wire hardware
  // Wire.begin(SDA_PIN, SCL_PIN);

  sensors_initialized = true;
  return true;
}

bool sensors_update_loadcell() {
  // Replace with real HX711 reading code once load cell is connected.
  // Example: long raw = hx.read_average(...); test_weight = (raw - offset) / factor / 1000.0;
  (void)0;
  return true;
}

bool sensors_update_internal() {
  // Replace with real SI7021 or other internal T/H sensor read.
  (void)0;
  return true;
}

bool sensors_update_external() {
  // Replace with real BME/BMP reading (temp/hum/pressure).
  (void)0;
  return true;
}

bool sensors_update_accel() {
  // Replace with real accelerometer read.
  (void)0;
  return true;
}

bool sensors_update_battery() {
  // If you later measure battery here, update test_batt_voltage and test_batt_percent.
  // For now do nothing because placeholders live in .ino and you asked to keep them.
  (void)0;
  return true;
}

bool sensors_update() {
  if (!sensors_initialized) {
    sensors_init();
  }

  bool ok = true;
  ok &= sensors_update_loadcell();
  ok &= sensors_update_internal();
  ok &= sensors_update_external();
  ok &= sensors_update_accel();
  ok &= sensors_update_battery();

  // Optionally update RSSI if modem available (example):
  // test_rssi = modem_getRSSI();

  return ok;
}