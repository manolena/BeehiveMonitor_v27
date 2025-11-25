// url=https://github.com/manolena/Beehive-Monitor/blob/main/text_strings.h
#ifndef TEXT_STRINGS_H
#define TEXT_STRINGS_H

#ifdef ARDUINO
#include <Arduino.h>
#endif

// Centralized text identifiers for all UI strings (EN/GR)
enum TextId
{
    TXT_NONE = 0,

    // MAIN MENU
    TXT_STATUS,
    TXT_TIME,
    TXT_MEASUREMENTS,
    TXT_WEATHER,
    TXT_CONNECTIVITY,
    TXT_PROVISION,
    TXT_CALIBRATION,
    TXT_LANGUAGE,
    TXT_SD_INFO,
    TXT_BACK,

    // STATUS / COMMON
    TXT_FETCHING_WEATHER,
    TXT_WEATHER_NO_DATA,
    TXT_LAST_FETCH,
    TXT_PRESS_ANY,     // generic
    TXT_PRESS_SELECT,  // press SELECT (new)

    // PROVISION / KEY ENTRY
    TXT_ENTER_API_KEY,
    TXT_KEY_STORED,
    TXT_KEY_STORED_VERIFY,
    TXT_CANCELLED,
    TXT_ENTER_CITY,
    TXT_ENTER_COUNTRY,
    TXT_GEOCODE_SAVED,
    TXT_GEOCODE_FAILED,
    TXT_VERIFYING_KEY,

    // MEASUREMENTS SUBPAGE LABELS
    TXT_WEIGHT,
    TXT_T_INT,
    TXT_H_INT,
    TXT_T_EXT,
    TXT_H_EXT,
    TXT_PRESSURE,
    TXT_ACCEL,

    // CALIBRATION
    TXT_TARE,
    TXT_CALIBRATE_KNOWN,
    TXT_RAW_VALUE,
    TXT_SAVE_FACTOR,
    TXT_TARE_DONE,
    TXT_CALIBRATION_DONE,
    TXT_FACTOR_SAVED,

    // New calibration helper texts
    TXT_TARE_PROMPT,      // e.g. "REMOVE ALL WEIGHTS"
    TXT_PLACE_WEIGHT,     // e.g. "PLACE %s"
    TXT_MEASURING,        // "MEASURING..."
    TXT_SAVE_FAILED,      // "SAVE FAILED"
    TXT_NO_CALIBRATION,   // "NO CALIBRATION"

    // CONNECTIVITY
    TXT_WIFI_CONNECTED,
    TXT_LTE_REGISTERED,
    TXT_NO_CONNECTIVITY,
    TXT_SSID,
    TXT_RSSI,
    TXT_MODE,

    // SD
    TXT_SD_CARD_INFO,
    TXT_SD_OK,
    TXT_NO_CARD,

    // LANGUAGE
    TXT_LANGUAGE_EN,
    TXT_LANGUAGE_GR,

    // Small helpers
    TXT_BACK_SMALL,
    TXT_OK,
    TXT_ERROR,

    TXT_COUNT
};

const char* getTextEN(TextId id);
const char* getTextGR(TextId id);

#endif // TEXT_STRINGS_H