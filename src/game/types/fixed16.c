// ===========================================================================
// Fixed16 — Non-inline Operations
// ===========================================================================
// Functions too large for header inlining (e.g. fixed16_sqrt).

#include "fixed16.h"

// Integer square root of a fixed16_16 value.
// Uses 64-bit intermediate so fixed16_sqrt(FIXED(4)) == FIXED(2).
// Algorithm: binary digit-by-digit (restoring) method.
//
// Not inlined because it's ~25 lines with two loops — inlining would bloat
// every call site without measurable benefit. The loop-heavy body dominates
// execution time, so the function-call overhead is negligible.
Fixed16 fixed16_sqrt(Fixed16 value) {
  if (value <= 0)
    return 0;

  // Shift into 64-bit space so the result lands in fixed16_16 format.
  // Max input is ~0x7FFFFFFF, so val can be up to ~2^47. Start bit at 2^46.
  int64_t val = (int64_t)value << 16;
  int64_t result = 0;
  int64_t bit = (int64_t)1 << 46;

  // Find highest power-of-4 that fits.
  while (bit > val) {
    bit >>= 2;
  }

  while (bit != 0) {
    if (val >= result + bit) {
      val -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }

  return (Fixed16)result;
}
