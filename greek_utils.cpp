#include "greek_utils.h"

// Helper: append a Unicode codepoint as UTF-8 to a String
static void appendCodepointUtf8(String &out, uint32_t cp) {
  if (cp <= 0x7F) {
    out += (char)cp;
  } else if (cp <= 0x7FF) {
    out += (char)(0xC0 | ((cp >> 6) & 0x1F));
    out += (char)(0x80 | (cp & 0x3F));
  } else if (cp <= 0xFFFF) {
    out += (char)(0xE0 | ((cp >> 12) & 0x0F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  } else {
    out += (char)(0xF0 | ((cp >> 18) & 0x07));
    out += (char)(0x80 | ((cp >> 12) & 0x3F));
    out += (char)(0x80 | ((cp >> 6) & 0x3F));
    out += (char)(0x80 | (cp & 0x3F));
  }
}

// Convert from UTF-8 sequence (pointer) to Unicode codepoint and return number of bytes consumed.
// If invalid, consumes one byte and returns that byte as codepoint.
static int utf8_to_codepoint(const char *p, uint32_t &outCp) {
  uint8_t b0 = (uint8_t)p[0];
  if (b0 < 0x80) {
    outCp = b0;
    return 1;
  } else if ((b0 & 0xE0) == 0xC0) {
    uint8_t b1 = (uint8_t)p[1];
    if ((b1 & 0xC0) != 0x80) { outCp = b0; return 1; }
    outCp = ((b0 & 0x1F) << 6) | (b1 & 0x3F);
    return 2;
  } else if ((b0 & 0xF0) == 0xE0) {
    uint8_t b1 = (uint8_t)p[1], b2 = (uint8_t)p[2];
    if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80)) { outCp = b0; return 1; }
    outCp = ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);
    return 3;
  } else if ((b0 & 0xF8) == 0xF0) {
    uint8_t b1 = (uint8_t)p[1], b2 = (uint8_t)p[2], b3 = (uint8_t)p[3];
    if (((b1 & 0xC0) != 0x80) || ((b2 & 0xC0) != 0x80) || ((b3 & 0xC0) != 0x80)) { outCp = b0; return 1; }
    outCp = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) | ((b2 & 0x3F) << 6) | (b3 & 0x3F);
    return 4;
  } else {
    outCp = b0;
    return 1;
  }
}

// Map common Greek lowercase codepoints (and accented forms) to uppercase base letters.
// We intentionally map accented lowercase vowels to base uppercase WITHOUT tonos/diacritics, to ensure display compatibility.
static uint32_t greek_map_to_upper(uint32_t cp) {
  // Basic small letters: α (0x03B1) .. ω (0x03C9)
  if (cp >= 0x03B1 && cp <= 0x03C1) {
    return cp - 0x20; // α->Α etc.
  }
  if (cp == 0x03C2) { // final sigma ς -> Σ (U+03A3)
    return 0x03A3;
  }
  if (cp >= 0x03C3 && cp <= 0x03C9) {
    return cp - 0x20;
  }

  // Common tonos/accented small vowels -> base uppercase without accent
  switch (cp) {
    case 0x03AC: // ά -> Α
      return 0x0391;
    case 0x03AD: // έ -> Ε
      return 0x0395;
    case 0x03AE: // ή -> Η
      return 0x0397;
    case 0x03AF: // ί -> Ι
      return 0x0399;
    case 0x03CC: // ό -> Ο
      return 0x039F;
    case 0x03CD: // ύ -> Υ
      return 0x03A5;
    case 0x03CE: // ώ -> Ω
      return 0x03A9;
    case 0x0390: // ΐ etc -> Ι
    case 0x03CA: // ϊ -> Ι
    case 0x03CB: // ϋ -> Υ
    case 0x03B0: // ΰ -> Υ
      // map diaeresis variants to base uppercase
      if (cp == 0x03CA || cp == 0x03AF || cp == 0x0390) return 0x0399; // Ι
      if (cp == 0x03CB || cp == 0x03B0) return 0x03A5; // Υ
      break;
    default:
      break;
  }

  // If not Greek small letter, attempt ASCII uppercase for latin letters
  if (cp >= 'a' && cp <= 'z') return (uint32_t) (cp - ('a' - 'A'));

  // Otherwise, return unchanged
  return cp;
}

String greekToUpper(const String &in) {
  String out;
  out.reserve(in.length());

  const char *p = in.c_str();
  while (*p) {
    uint32_t cp = 0;
    int n = utf8_to_codepoint(p, cp);
    if (n <= 0) { // safety
      p++;
      continue;
    }

    // If codepoint is in Greek ranges or ascii lowercase, map it
    uint32_t up = greek_map_to_upper(cp);

    // Append uppercase codepoint as UTF-8
    appendCodepointUtf8(out, up);

    p += n;
  }

  return out;
}