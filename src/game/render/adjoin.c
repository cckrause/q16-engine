// ===========================================================================
// Adjoin System (Portals)
// ===========================================================================
// Manages edge pairs, adjoin lists, and state save/restore for recursive
// portal traversal.

#include "render/adjoin.h"
#include <math.h>
#include <string.h>

void adjoin_list_reset(AdjoinList *al) {
  al->count = 0;
}

bool adjoin_list_add(AdjoinList *al, WallSegment *seg, const EdgePair *edge,
                     Sector *next_sector) {
  if (al->count >= MAX_ADJOIN_SEG)
    return false;

  AdjoinEntry *e = &al->entries[al->count];
  e->seg = seg;
  e->edge_pair = *edge;
  e->next_sector = next_sector;
  al->count++;
  return true;
}

void adjoin_compute_edge_pair(float cam_focal_aspect, float cam_proj_y, float cam_y,
                              float floor_h, float ceil_h, float next_floor_h,
                              float next_ceil_h, const WallSegment *seg, EdgePair *out) {
  out->x0 = seg->wall_x0;
  out->x1 = seg->wall_x1;

  // The visible opening is bounded by the tighter of current/next sector
  // heights. In Y-down: ceiling values are more negative (higher up), floor
  // values are more positive (lower down). The tighter ceiling is the one
  // with the larger value (less negative = hangs lower), and the tighter
  // floor is the one with the smaller value (more negative = rises higher).
  float vis_ceil = ceil_h > next_ceil_h ? ceil_h : next_ceil_h;
  float vis_floor = floor_h < next_floor_h ? floor_h : next_floor_h;

  // Project the ceiling/floor heights to screen Y at each wall endpoint.
  // screen_y = (height - cam_y) * focal_aspect / z + proj_offset_y
  float inv_z0 = 1.0f / seg->z0;
  float inv_z1 = 1.0f / seg->z1;

  out->y_ceil0 = (vis_ceil - cam_y) * cam_focal_aspect * inv_z0 + cam_proj_y;
  out->y_ceil1 = (vis_ceil - cam_y) * cam_focal_aspect * inv_z1 + cam_proj_y;
  out->y_floor0 = (vis_floor - cam_y) * cam_focal_aspect * inv_z0 + cam_proj_y;
  out->y_floor1 = (vis_floor - cam_y) * cam_focal_aspect * inv_z1 + cam_proj_y;

  int32_t dx = out->x1 - out->x0;
  if (dx > 0) {
    float inv_dx = 1.0f / (float)dx;
    out->dy_ceil_dx = (out->y_ceil1 - out->y_ceil0) * inv_dx;
    out->dy_floor_dx = (out->y_floor1 - out->y_floor0) * inv_dx;
  } else {
    out->dy_ceil_dx = 0.0f;
    out->dy_floor_dx = 0.0f;
  }

  out->y_pixel_c0 = (int32_t)floorf(out->y_ceil0);
  out->y_pixel_c1 = (int32_t)floorf(out->y_ceil1);
  out->y_pixel_f0 = (int32_t)ceilf(out->y_floor0);
  out->y_pixel_f1 = (int32_t)ceilf(out->y_floor1);
}

void adjoin_save_state(AdjoinSaveState *state, int32_t min_x, int32_t max_x,
                       int32_t min_y, int32_t max_y, float ambient,
                       float scaled_ambient) {
  state->window_min_x = min_x;
  state->window_max_x = max_x;
  state->window_min_y = min_y;
  state->window_max_y = max_y;
  state->sector_ambient = ambient;
  state->scaled_ambient = scaled_ambient;
}

void adjoin_restore_state(const AdjoinSaveState *state, int32_t *min_x, int32_t *max_x,
                          int32_t *min_y, int32_t *max_y, float *ambient,
                          float *scaled_ambient) {
  *min_x = state->window_min_x;
  *max_x = state->window_max_x;
  *min_y = state->window_min_y;
  *max_y = state->window_max_y;
  *ambient = state->sector_ambient;
  *scaled_ambient = state->scaled_ambient;
}
