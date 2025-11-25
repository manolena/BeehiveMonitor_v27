// Minimal stub for weather_debug_dumpAndFetch()
// Place this file into the sketch to satisfy linker if the real implementation is missing.
// If you have a full weather_manager.cpp with this function, delete this stub.

#include <Arduino.h>

// If the real header exists, include it to match prototype (optional).
// #include "weather_manager.h"

// Simple safe stub.
bool weather_debug_dumpAndFetch() {
  Serial.println("[Weather-Debug-Stub] weather_debug_dumpAndFetch() called - stub returns false");
  return false;
}