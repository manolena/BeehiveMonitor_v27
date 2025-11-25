#pragma once
#include <Arduino.h>

// sensors.h : API for sensor module
// Implementations update global variables declared in config.h

// Initialize sensors (I2C, SPI, HX711, etc). Return true if initialization ok.
bool sensors_init();

// Periodic update function — should be called regularly (or from a task)
// to read sensors and update the global variables.
// Returns true on successful update.
bool sensors_update();

// Optional: explicit functions to read/refresh individual sensors
bool sensors_update_loadcell();   // updates test_weight
bool sensors_update_internal();   // updates test_temp_int/test_hum_int
bool sensors_update_external();   // updates test_temp_ext/test_hum_ext/test_pressure
bool sensors_update_accel();      // updates test_acc_x/_y/_z
bool sensors_update_battery();    // updates test_batt_voltage/test_batt_percent

// If you add more sensor-specific APIs, declare them here.