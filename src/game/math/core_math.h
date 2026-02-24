// ===========================================================================
// Core Math — Fixed-point Utilities
// ===========================================================================
#ifndef Q16_MATH_CORE_MATH_H
#define Q16_MATH_CORE_MATH_H

#include "types/fixed16.h"
#include "types/types.h"

// Distance approximation, direction/length, and angle14 operations.

// --- Distance approximation ------------------------------------------------

// Manhattan-ish distance: max(|dx|,|dz|) + min(|dx|,|dz|)/2.
// Over-estimates diagonals by ~12%. Used everywhere instead of Euclidean.
Fixed16 dist_approx(Fixed16 x0, Fixed16 z0, Fixed16 x1, Fixed16 z1);

// Same approximation, but from origin: dist_approx(0, 0, dx, dz).
Fixed16 vec2_length(Fixed16 dx, Fixed16 dz);

// --- Direction & length ----------------------------------------------------
typedef struct DirAndLength {
  Fixed16 dir_x;
  Fixed16 dir_z;
  Fixed16 length;
} DirAndLength;

// Normalize a 2D vector and return its approximate length.
// Components are clamped to [-1.0, 1.0] fixed-point.
DirAndLength compute_dir_and_length(Fixed16 dx, Fixed16 dz);

// --- Angle14 utilities -----------------------------------------------------
// Convert a 2D direction vector to angle14.
// dz>0 (forward) = 0, dx>0 (right) = 4096 (90 deg).
Angle14 vec2_to_angle(Fixed16 dx, Fixed16 dz);

// Signed shortest-arc difference. Result in [-8192, +8191] (+-180 deg).
Angle14 get_angle_difference(Angle14 angle0, Angle14 angle1);

// Arc cosine via cosine table lookup. Unusual negative-case behavior:
// when sin_angle < 0, the lookup uses -angle (the second parameter),
// not -sin_angle.
Angle14 arc_cos_fixed(Fixed16 sin_angle, Angle14 angle);

#endif /* Q16_MATH_CORE_MATH_H */
