#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include "config.h"

// HX711 detection (use library if available)
#ifdef __has_include
  #if __has_include(<HX711.h>)
    #include <HX711.h>
    #define HAVE_HX711 1
  #else
    #define HAVE_HX711 0
  #endif
#else
  #define HAVE_HX711 0
#endif

#define CALIB_PREF_NS "calib_ns"

#ifndef CALIB_SAMPLES
  #define CALIB_SAMPLES 20
#endif
#ifndef CALIB_SKIP
  #define CALIB_SKIP 5
#endif

void calib_init();
long calib_readRawAverage(int samples = CALIB_SAMPLES, int skip = CALIB_SKIP);
long calib_doTare(int samples = CALIB_SAMPLES, int skip = CALIB_SKIP);
float calib_computeFactorFromKnownWeight(long raw_at_weight, long offset, float grams);
bool calib_saveFactor(float factor, long offset, int known_grams);
bool calib_hasSavedFactor();
float calib_getSavedFactor();
long  calib_getSavedOffset();
int   calib_getSavedKnown();

#endif // CALIBRATION_H