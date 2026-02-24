#ifndef Q16_FIXED16_H
#define Q16_FIXED16_H

#include <stdint.h>

// 16.16 fixed-point: upper 16 bits integer, lower 16 bits fraction.
// FIXED(5) = 5 << 16 = 0x50000 = 327680
typedef int32_t Fixed16;

#define FIXED16_FRAC_BITS 16
#define FIXED16_ONE       (1 << FIXED16_FRAC_BITS)       // 0x10000
#define FIXED16_HALF      (1 << (FIXED16_FRAC_BITS - 1)) // 0x08000

// Integer to fixed-point.
// Multiply by 65536 instead of shifting — avoids UB on negative values.
#define FIXED(n) ((Fixed16)((n) * FIXED16_ONE))

// Fixed-point multiply: (a * b) >> 16 using 64-bit intermediate.
static inline Fixed16 mul16(Fixed16 a, Fixed16 b) {
  return (Fixed16)(((int64_t)a * b) >> FIXED16_FRAC_BITS);
}

// Fixed-point divide: (a << 16) / b using 64-bit intermediate.
// Caller must guarantee b != 0.
static inline Fixed16 div16(Fixed16 a, Fixed16 b) {
  return (Fixed16)(((int64_t)a << FIXED16_FRAC_BITS) / b);
}

// Convert float to fixed-point. Double intermediate avoids premature rounding
// of the product before truncation to int32_t.
static inline Fixed16 float_to_fixed16(float f) {
  return (Fixed16)((double)f * (double)FIXED16_ONE);
}

// Convert fixed-point to float. Double intermediate preserves all 31 value bits
// of int32_t (float only has 24-bit mantissa, double has 53). The final narrowing
// to float rounds once at the end instead of losing bits before the division.
// On x87 (Pentium target) this costs nothing — the FPU works in 80-bit extended
// precision internally, so float and double operations are the same speed.
static inline float fixed16_to_float(Fixed16 x) {
  return (float)((double)x / (double)FIXED16_ONE);
}

static inline Fixed16 int_to_fixed16(int32_t n) {
  return (Fixed16)(n << FIXED16_FRAC_BITS);
}

static inline int32_t fixed16_to_int(Fixed16 x) {
  return x >> FIXED16_FRAC_BITS;
}

static inline Fixed16 fixed16_floor(Fixed16 x) {
  return x & ~(FIXED16_ONE - 1);
}

static inline Fixed16 fixed16_ceil(Fixed16 x) {
  return (x + FIXED16_ONE - 1) & ~(FIXED16_ONE - 1);
}

static inline Fixed16 fixed16_abs(Fixed16 x) {
  return x < 0 ? -x : x;
}

// Extract the fractional part (lower 16 bits).
static inline Fixed16 fixed16_fract(Fixed16 x) {
  return x & 0xFFFF;
}

// Round to nearest integer (returns integer, not fixed-point).
static inline int32_t fixed16_round(Fixed16 x) {
  return (x + FIXED16_HALF) >> FIXED16_FRAC_BITS;
}

// Clamp a fixed-point value to [lo, hi].
static inline Fixed16 fixed16_clamp(Fixed16 v, Fixed16 lo, Fixed16 hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Integer square root of a fixed16_16 value.
// Too large for inlining (~25 lines with loops) — defined in fixed16.c.
Fixed16 fixed16_sqrt(Fixed16 value);

#endif /* Q16_FIXED16_H */
