#ifndef Q16_MATH_TRIG_TABLE_H
#define Q16_MATH_TRIG_TABLE_H

#include "types/fixed16.h"
#include "types/types.h"

// Cosine lookup table: 4096 entries covering the first quadrant [0, 90 deg).
// Index 0 = cos(0) = FIXED16_ONE, Index 4095 ~ cos(90) ~ 0.
// Values are fixed16_16. Defined in trig_table.c.
extern const Fixed16 g_cos_table[4096];

// Compute sin and cos for an angle14 value using the cosine table.
// Full quadrant decomposition for 1:1 accuracy with the original engine.
void sin_cos_fixed(Angle14 angle, Fixed16 *sin_out, Fixed16 *cos_out);

#endif /* Q16_MATH_TRIG_TABLE_H */
