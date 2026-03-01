// ===========================================================================
// Debug Visualization — Color helpers for development overlays
// ===========================================================================

#include "debug/debug_vis.h"
#include "render/render_limits.h"
#include <math.h>

// --- Color generation ------------------------------------------------------

void debug_hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
  int32_t hi = (int32_t)(h / 60.0f) % 6;
  float f = h / 60.0f - (float)hi;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  switch (hi) {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  default:
    *r = v;
    *g = p;
    *b = q;
    break;
  }
}

void debug_sector_color(int32_t sector_id, float *r, float *g, float *b) {
  float hue = fmodf((float)sector_id * 137.508f, 360.0f);
  debug_hsv_to_rgb(hue, 0.7f, 0.9f, r, g, b);
}

float debug_part_brightness(int32_t part_id) {
  switch (part_id) {
  case PART_MID_WALL:
    return 1.0f;
  case PART_TOP_WALL:
    return 0.85f;
  case PART_BOT_WALL:
    return 0.85f;
  case PART_FLOOR:
    return 0.6f;
  case PART_CEILING:
    return 0.5f;
  case PART_FLOOR_CAP:
    return 0.4f;
  case PART_CEILING_CAP:
    return 0.35f;
  case PART_MID_SIGN:
  case PART_TOP_SIGN:
  case PART_BOT_SIGN:
    return 1.0f;
  default:
    return 0.7f;
  }
}

void debug_part_color(int32_t part_id, float *r, float *g, float *b) {
  switch (part_id) {
  case PART_MID_WALL:
    *r = 1.0f;
    *g = 1.0f;
    *b = 1.0f;
    break; // white
  case PART_TOP_WALL:
    *r = 1.0f;
    *g = 1.0f;
    *b = 0.3f;
    break; // yellow
  case PART_BOT_WALL:
    *r = 0.3f;
    *g = 1.0f;
    *b = 1.0f;
    break; // cyan
  case PART_FLOOR:
    *r = 0.2f;
    *g = 0.8f;
    *b = 0.2f;
    break; // green
  case PART_CEILING:
    *r = 0.8f;
    *g = 0.2f;
    *b = 0.2f;
    break; // red
  case PART_FLOOR_CAP:
    *r = 0.1f;
    *g = 0.4f;
    *b = 0.1f;
    break; // dark green
  case PART_CEILING_CAP:
    *r = 0.4f;
    *g = 0.1f;
    *b = 0.1f;
    break; // dark red
  case PART_MID_SIGN:
  case PART_TOP_SIGN:
  case PART_BOT_SIGN:
    *r = 1.0f;
    *g = 0.5f;
    *b = 0.0f;
    break; // orange
  default:
    *r = 0.5f;
    *g = 0.5f;
    *b = 0.5f;
    break; // grey
  }
}
