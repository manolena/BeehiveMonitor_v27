#include "calibration.h"
#include <Preferences.h>

#if HAVE_HX711
  #include <HX711.h>
  #ifndef HX711_DOUT_PIN
    #define HX711_DOUT_PIN  DOUT
  #endif
  #ifndef HX711_SCK_PIN
    #define HX711_SCK_PIN   SCK
  #endif
  static HX711 hx;
#endif

static float g_saved_factor = 0.0f; // counts per gram
static long  g_saved_offset = 0;
static int   g_saved_known  = 0;
static bool  g_inited = false;

void calib_init() {
  if (g_inited) return;
  g_inited = true;

#if HAVE_HX711
  hx.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  delay(100);
  #if ENABLE_DEBUG
    if (!hx.is_ready()) Serial.println(F("[CALIB] HX711 not ready"));
  #endif
#endif

  Preferences p;
  if (p.begin(CALIB_PREF_NS, true)) {
    g_saved_factor = p.getFloat("factor", 0.0f);
    g_saved_offset = p.getLong("offset", 0);
    g_saved_known  = p.getInt("known", 0);
    p.end();
  }

  #if ENABLE_DEBUG
    Serial.print(F("[CALIB] loaded factor="));
    Serial.print(g_saved_factor);
    Serial.print(F(" offset="));
    Serial.println(g_saved_offset);
  #endif
}

static long calib_safe_read_once() {
#if HAVE_HX711
  unsigned long start = millis();
  while (!hx.is_ready()) {
    if (millis() - start > 500) break;
    delay(1);
  }
  long v = hx.read();
  return v;
#else
  #if ENABLE_DEBUG
    Serial.println(F("[CALIB] HX711 not present: stub read->0"));
  #endif
  return 0;
#endif
}

long calib_readRawAverage(int samples, int skip) {
  if (samples <= 0) samples = CALIB_SAMPLES;
  if (skip < 0) skip = CALIB_SKIP;
  long sum = 0;
  int count = 0;

  for (int i = 0; i < skip; i++) {
    (void)calib_safe_read_once();
    delay(20);
  }
  for (int i = 0; i < samples; i++) {
    long v = calib_safe_read_once();
    sum += v;
    count++;
    delay(30);
  }
  if (count == 0) return 0;
  return sum / count;
}

long calib_doTare(int samples, int skip) {
  long off = calib_readRawAverage(samples, skip);
  g_saved_offset = off; // transient until saved by caller if desired
  #if ENABLE_DEBUG
    Serial.print(F("[CALIB] TARE offset="));
    Serial.println(off);
  #endif
  return off;
}

float calib_computeFactorFromKnownWeight(long raw_at_weight, long offset, float grams) {
  if (grams <= 0.0f) return 0.0f;
  float diff = (float)(raw_at_weight - offset);
  float factor = diff / grams; // counts per gram
  return factor;
}

bool calib_saveFactor(float factor, long offset, int known_grams) {
  Preferences p;
  if (!p.begin(CALIB_PREF_NS, false)) {
    #if ENABLE_DEBUG
      Serial.println(F("[CALIB] Preferences begin failed"));
    #endif
    return false;
  }
  p.putFloat("factor", factor);
  p.putLong("offset", offset);
  p.putInt("known", known_grams);
  p.end();

  g_saved_factor = factor;
  g_saved_offset = offset;
  g_saved_known  = known_grams;

  #if ENABLE_DEBUG
    Serial.print(F("[CALIB] saved factor="));
    Serial.print(factor);
    Serial.print(F(" offset="));
    Serial.println(offset);
  #endif
  return true;
}

bool calib_hasSavedFactor() { return (g_saved_factor > 0.0f); }
float calib_getSavedFactor() { return g_saved_factor; }
long  calib_getSavedOffset() { return g_saved_offset; }
int   calib_getSavedKnown()  { return g_saved_known; }