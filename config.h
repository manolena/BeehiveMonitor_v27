#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>               // provide WiFi symbols (WiFi, WL_CONNECTED) to modules
#include <LiquidCrystal_I2C.h>  // provide LCD type and allow extern declaration

// =============================
// Debug configuration
// =============================
// Set to 1 to enable verbose debug Serial prints, 0 to disable
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 1
#endif

// Timing (microseconds)
#define MEASUREMENT_INTERVAL  (3600ULL * 1000000ULL)

#define THINGSPEAK_WRITE_APIKEY "10A4ZQ8S44BPJASO"

// =============================
// Fixed hardware pinout
// =============================

// I2C for LCD and Sensors
#define SDA_PIN        21
#define SCL_PIN        22

// HX711 Load Cell (default pins; can be overridden elsewhere)
#define DOUT           19
#define SCK            18

// SD Card pins
#define SD_MISO        2
#define SD_MOSI        15
#define SD_SCLK        14
#define SD_CS          13

// Battery
#define BATTERY_PIN    35
#define R1             10000.0
#define R2             10000.0

// LTE Modem
#define MODEM_RX       27
#define MODEM_TX       26
#define MODEM_PWR      4

// Buttons
#define BTN_UP         23
#define BTN_DOWN       12
#define BTN_SELECT     33
#define BTN_BACK       32

// Connectivity modes
#define CONNECTIVITY_LTE     0
#define CONNECTIVITY_WIFI    1
#define CONNECTIVITY_OFFLINE 2

// LTE Configuration
#define MODEM_APN        "internet"
#define MODEM_GPRS_USER  ""
#define MODEM_GPRS_PASS  ""

// Use Open-Meteo as default (no API key). Keep compatibility if overwritten.
#ifndef USE_OPENMETEO
#define USE_OPENMETEO 1
#endif

// Dual WiFi compile-time defaults (can be overridden via build)
#ifndef WIFI_SSID1
#define WIFI_SSID1 "Redmi Note 13"
#define WIFI_PASS1 "nen57asz5g44sh2"
#endif

#ifndef WIFI_SSID2
#define WIFI_SSID2 "COSMOTE-32bssa"
#define WIFI_PASS2 "vudvvc5x97s4afpk"
#endif

// Single global Preferences instance is defined in one .cpp (weather_manager.cpp).
extern Preferences prefs;
extern int connectivityMode;

// Single global LCD instance is defined in ui.cpp; make it visible to all units.
extern LiquidCrystal_I2C lcd;

// Safe INIT_SD_CARD macro: initializes SPI pins and SD; ensure backslashes remain at EOL
#ifndef INIT_SD_CARD
#define INIT_SD_CARD() do { \
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS); \
    SD.begin(SD_CS); \
} while(0)
#endif

#ifndef DEGREE_SYMBOL_UTF
// UTF-8 degree sign for Serial / web clients
#define DEGREE_SYMBOL_UTF "\xC2\xB0"
#endif

#ifndef DEGREE_SYMBOL_LCD
// Single-byte 0xDF is commonly used by HD44780 font to show degree symbol.
#define DEGREE_SYMBOL_LCD 0xDF
#endif

// ============================================================
// SENSOR VALUE GLOBALS (no hardcoded test values here)
//
// These globals are updated by the sensors module when real sensors are
// connected. They are declared extern here and defined (with safe defaults)
// in sensors.cpp. Do NOT hardcode temporary data here.
// ============================================================

// Weight (kg)
extern float test_weight;

// Internal Temperature & Humidity (SI7021)
extern float test_temp_int;
extern float test_hum_int;

// External Temperature, Humidity, Pressure (BME280/BME680)
extern float test_temp_ext;
extern float test_hum_ext;
extern float test_pressure;

// Accelerometer (X,Y,Z)
extern float test_acc_x;
extern float test_acc_y;
extern float test_acc_z;

// Battery voltage & percentage (retain placeholders as requested)
extern float test_batt_voltage;
extern int   test_batt_percent;

// Modem signal (optional)
extern int   test_rssi;

// ------------------------
// Default location (compile-time fallback only)
#define DEFAULT_LAT       37.983810       // fallback: Athens latitude
#define DEFAULT_LON       23.727539       // fallback: Athens longitude

// Optional behavior toggles (local to project)
#ifndef AUTOSTART_KEYSERVER
#define AUTOSTART_KEYSERVER 1
#endif