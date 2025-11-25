#pragma once
#include <Arduino.h>

// ThingSpeak client API
bool initThingSpeakClient();

// Send telemetry bodyPairs (e.g. "field1=23.5&field2=60.0").
// This function implements WiFi-first policy: if WiFi is available it will
// POST immediately, otherwise it will try to auto-connect to known WiFi and
// if still unavailable it will enqueue the body for later retry (SD card).
bool sendToThingSpeak(const String &bodyPairs);

// Upload the current telemetry using the project's global test_* variables
// (used by loop()). Returns true on immediate success.
bool thingspeak_upload_current();

// Attempt to flush queued posts (will only try when WiFi is connected).
void retryQueuedThingSpeak();

// Filename on SD for queued ThingSpeak posts (one per line)
static const char *TS_QUEUE_FILENAME = "/ts_queue.txt";