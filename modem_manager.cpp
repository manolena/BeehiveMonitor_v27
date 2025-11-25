// modem_manager.cpp (patched - full)
// Contains: modem_hw_init(), modem_get(), modemManager_init(),
// modem_isNetworkRegistered(), modem_getRSSI(), modem_getOperator().

#include "modem_manager.h"
#include "config.h"
#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <Arduino.h>

// ---------------------------------------------------------
// Physical serial port for modem (ESP32 UART2)
HardwareSerial SerialAT(2);  // UART2 on ESP32

static TinyGsm* _modem = nullptr;

// ---------------------------------------------------------
// Helper: power-up sequences and AT check
// ---------------------------------------------------------
#ifndef MODEM_PWR_ACTIVE_LOW
#define MODEM_PWR_ACTIVE_LOW 0
#endif

static bool modem_power_up_check(unsigned long atTimeoutMs = 5000) {
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] Starting power-up/detection sequence"));
#endif

    pinMode(MODEM_PWR, OUTPUT);

    // Baseline
    if (MODEM_PWR_ACTIVE_LOW) digitalWrite(MODEM_PWR, HIGH);
    else digitalWrite(MODEM_PWR, LOW);
    delay(50);

    // Sequence 1: short pulse (200ms)
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] Sequence 1: short pulse (200ms)"));
#endif
    if (MODEM_PWR_ACTIVE_LOW) {
      digitalWrite(MODEM_PWR, LOW);
      delay(200);
      digitalWrite(MODEM_PWR, HIGH);
    } else {
      digitalWrite(MODEM_PWR, HIGH);
      delay(200);
      digitalWrite(MODEM_PWR, LOW);
    }
    delay(1500);

    // Sequence 2: long hold (1200ms)
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] Sequence 2: long hold (1200ms)"));
#endif
    if (MODEM_PWR_ACTIVE_LOW) {
      digitalWrite(MODEM_PWR, LOW);
      delay(1200);
      digitalWrite(MODEM_PWR, HIGH);
    } else {
      digitalWrite(MODEM_PWR, HIGH);
      delay(1200);
      digitalWrite(MODEM_PWR, LOW);
    }
    delay(1500);

    // Sequence 3: alternate polarity hold (1200ms)
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] Sequence 3: alternate polarity hold (1200ms)"));
#endif
    if (MODEM_PWR_ACTIVE_LOW) {
      digitalWrite(MODEM_PWR, HIGH);
      delay(50);
      digitalWrite(MODEM_PWR, LOW);
      delay(1200);
      digitalWrite(MODEM_PWR, HIGH);
    } else {
      digitalWrite(MODEM_PWR, LOW);
      delay(50);
      digitalWrite(MODEM_PWR, HIGH);
      delay(1200);
      digitalWrite(MODEM_PWR, LOW);
    }
    delay(1500);

    // Start UART
    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(200);

    // Flush and test AT
    while (SerialAT.available()) SerialAT.read();
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] Sending AT to check modem responsiveness"));
#endif
    SerialAT.print("AT\r\n");

    unsigned long start = millis();
    String resp;
    while (millis() - start < atTimeoutMs) {
      while (SerialAT.available()) {
        char c = (char)SerialAT.read();
        resp += c;
        if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
      }
      if (resp.indexOf("OK") >= 0) {
#if ENABLE_DEBUG
        Serial.println(F("[modem_hw_init] Modem responded with OK"));
#endif
        return true;
      }
      if (resp.indexOf("ERROR") >= 0) {
#if ENABLE_DEBUG
        Serial.print(F("[modem_hw_init] Modem responded with ERROR: "));
        Serial.println(resp);
#endif
        break;
      }
      delay(50);
    }

#if ENABLE_DEBUG
    Serial.print(F("[modem_hw_init] No OK received, resp="));
    Serial.println(resp);
#endif
    return false;
}

// High-level modem_hw_init
void modem_hw_init() {
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] BEGIN"));
#endif

    bool up = modem_power_up_check(5000);
#if ENABLE_DEBUG
    if (up) {
      Serial.println(F("[modem_hw_init] modem_power_up_check succeeded"));
    } else {
      Serial.println(F("[modem_hw_init] modem_power_up_check failed - check wiring/power"));
    }
#endif

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    delay(200);
#if ENABLE_DEBUG
    Serial.println(F("[modem_hw_init] SerialAT started"));
    Serial.printf("[modem_hw_init] SerialAT TX=%d RX=%d\n", MODEM_TX, MODEM_RX);
#endif
}

// ---------------------------------------------------------
// Accessor for global modem instance
// ---------------------------------------------------------
TinyGsm& modem_get() {
    if (!_modem) {
        static TinyGsm modemInstance(SerialAT);
        _modem = &modemInstance;
    }
    return *_modem;
}

// ---------------------------------------------------------
// Initialization
// ---------------------------------------------------------
void modemManager_init()
{
    // safe to call modem_hw_init here as well
    modem_hw_init();
    delay(300);

    TinyGsm &modem = modem_get();

#if ENABLE_DEBUG
    Serial.println(F("[modemManager_init] attempting modem.restart()"));
#endif
    modem.restart();
    delay(500);

#if ENABLE_DEBUG
    Serial.println(F("[modemManager_init] setting CFUN=1"));
#endif
    modem.sendAT("+CFUN=1");
    modem.waitResponse(1000);

#if ENABLE_DEBUG
    Serial.println(F("[modemManager_init] modemManager_init completed"));
#endif
}

// ---------------------------------------------------------
// CHECK REGISTRATION (single canonical implementation)
// ---------------------------------------------------------
bool modem_isNetworkRegistered()
{
    TinyGsm &modem = modem_get();
    int stat = modem.getRegistrationStatus();
    return (stat == 1 || stat == 5);
}

// ---------------------------------------------------------
// Signal quality (RSSI) - implemented here (canonical)
// ---------------------------------------------------------
int16_t modem_getRSSI()
{
    TinyGsm &modem = modem_get();
    // TinyGsm returns signal quality (0-31 or 99); return as int16_t
    return modem.getSignalQuality();
}

// ---------------------------------------------------------
// Operator name
// ---------------------------------------------------------
String modem_getOperator()
{
    TinyGsm &modem = modem_get();
    return modem.getOperator();
}