// ===========================================================================
// Debug Visualization — Color helpers for development overlays
// ===========================================================================
#ifndef Q16_DEBUG_VIS_H
#define Q16_DEBUG_VIS_H

#include <stdint.h>

// --- Color generation ------------------------------------------------------

// HSV to RGB (h in [0,360), s/v in [0,1]).
void debug_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

// Golden-angle hash: maps sector ID to a unique, well-distributed hue.
void debug_sector_color(int32_t sector_id, float *r, float *g, float *b);

// Brightness multiplier per display-list part type [0,1].
float debug_part_brightness(int32_t part_id);

// Distinct RGB color per display-list part type.
void debug_part_color(int32_t part_id, float *r, float *g, float *b);

#endif /* Q16_DEBUG_VIS_H */
