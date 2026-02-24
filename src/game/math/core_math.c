// ===========================================================================
// Core Math — Fixed-point Utilities
// ===========================================================================
// Distance approximation, direction/length, and angle14 operations.

#include "math/core_math.h"
#include "math/trig_table.h"
#include <stdlib.h> // abs()

// Internal helpers

// Sign helper for vec2_to_angle: returns 1 for negative, 0 otherwise.
static int32_t sign_v2a(Fixed16 x) {
  return x < 0 ? 1 : 0;
}

// Module-level state for arc_cos_fixed (matches original engine static).
static int32_t s_neg_arc_cos = 0;

// Distance approximation

Fixed16 dist_approx(Fixed16 x0, Fixed16 z0, Fixed16 x1, Fixed16 z1) {
  Fixed16 dx = fixed16_abs(x1 - x0);
  Fixed16 dz = fixed16_abs(z1 - z0);

  if (dx < dz) {
    Fixed16 tmp = dx;
    dx = dz;
    dz = tmp;
  }
  return dx + (dz >> 1);
}

Fixed16 vec2_length(Fixed16 dx, Fixed16 dz) {
  return dist_approx(0, 0, dx, dz);
}

// Direction & length

DirAndLength compute_dir_and_length(Fixed16 dx, Fixed16 dz) {
  DirAndLength result = {0, 0, 0};
  Fixed16 dist = vec2_length(dx, dz);

  if (dist != 0) {
    result.dir_x = fixed16_clamp(div16(dx, dist), -FIXED16_ONE, FIXED16_ONE);
    result.dir_z = fixed16_clamp(div16(dz, dist), -FIXED16_ONE, FIXED16_ONE);
    result.length = dist;
  }
  return result;
}

// vec2_to_angle

Angle14 vec2_to_angle(Fixed16 dx, Fixed16 dz) {
  if (dx == 0 && dz == 0)
    return 0;

  int32_t signs_diff = (sign_v2a(dx) != sign_v2a(dz)) ? 1 : 0;
  int32_t quadrant = (dz < 0 ? 2 : 0) + signs_diff;

  dx = fixed16_abs(dx);
  dz = fixed16_abs(dz);

  int32_t subquadrant = quadrant * 2 + ((dx < dz) ? (1 - signs_diff) : signs_diff);

  // In sub-quadrants where |dz| dominates, swap dx and dz.
  if ((subquadrant - 1) & 2) {
    Fixed16 tmp = dx;
    dx = dz;
    dz = tmp;
  }

  Fixed16 dx_dz = div16(dx, dz);
  if (subquadrant & 1) {
    dx_dz = FIXED16_ONE - dx_dz;
  }

  Fixed16 subquadrant_f = int_to_fixed16(subquadrant);
  Angle14 angle = (2 * FIXED16_ONE - (subquadrant_f + dx_dz)) >> 5;
  return angle & ANGLE14_MASK;
}

// get_angle_difference

Angle14 get_angle_difference(Angle14 angle0, Angle14 angle1) {
  angle0 &= ANGLE14_MASK;
  angle1 &= ANGLE14_MASK;

  Angle14 d_angle = angle1 - angle0;

  if (abs(d_angle) <= 8191) {
    return d_angle;
  }
  return d_angle >= 0 ? d_angle - ANGLE14_FULL_CIRCLE : d_angle + ANGLE14_FULL_CIRCLE;
}

// arc_cos_fixed

Angle14 arc_cos_fixed(Fixed16 sin_angle, Angle14 angle) {
  if (sin_angle >= 0) {
    s_neg_arc_cos = 0;
  } else {
    s_neg_arc_cos = 1;
    // Unusual original behavior: uses -angle, NOT -sin_angle.
    sin_angle = -angle;
  }

  int32_t i = 0;
  for (; i < 4095; i++) {
    if (sin_angle >= g_cos_table[i]) {
      break;
    }
  }

  Angle14 res_angle = 4095 - i;
  if (s_neg_arc_cos) {
    res_angle += ANGLE14_HALF_CIRCLE;
  }
  return res_angle;
}
