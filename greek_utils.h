#pragma once
#include <Arduino.h>

// Convert a UTF-8 string containing Greek (and ASCII) to an uppercase UTF-8 string.
// Handles common Greek small letters, final sigma and common tonos/diaeresis forms by mapping to base uppercase.
// Returns a new String with UTF-8 uppercase characters.
String greekToUpper(const String &in);