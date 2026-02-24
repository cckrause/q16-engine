// ===========================================================================
// Adjoin System (Portals)
// ===========================================================================
#ifndef Q16_RENDER_ADJOIN_H
#define Q16_RENDER_ADJOIN_H

#include "render/render_limits.h"
#include "render/wall_process.h"
#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// Manages portal recursion state: edge pairs that define the visible opening,
// saved renderer state for restoration after recursion, and the adjoin
// segment list that drives traversal order.

// Describes the visible opening of a portal wall across a screen X span.
typedef struct {
  int32_t x0;
  int32_t x1;

  // Ceiling edge (Y values at each screen X endpoint).
  float y_ceil0;
  float y_ceil1;
  float dy_ceil_dx;

  // Floor edge.
  float y_floor0;
  float y_floor1;
  float dy_floor_dx;

  // Pixel-snapped values.
  int32_t y_pixel_c0;
  int32_t y_pixel_c1;
  int32_t y_pixel_f0;
  int32_t y_pixel_f1;
} EdgePair;

// Renderer state saved before recursing into a portal.
typedef struct {
  int32_t window_min_y;
  int32_t window_max_y;
  int32_t window_min_x;
  int32_t window_max_x;
  float sector_ambient;
  float scaled_ambient;
} AdjoinSaveState;

// One entry in the per-sector adjoin list.
typedef struct {
  WallSegment *seg;   // the wall segment that generated this adjoin
  EdgePair edge_pair; // visible opening geometry
  Sector *next_sector;
} AdjoinEntry;

// Collected adjoin list for a single sector's wall pass.
typedef struct {
  AdjoinEntry entries[MAX_ADJOIN_SEG];
  int32_t count;
} AdjoinList;

// Initialize an adjoin list (reset count to zero).
void adjoin_list_reset(AdjoinList *al);

// Add an adjoin entry. Returns false if the list is full.
bool adjoin_list_add(AdjoinList *al, WallSegment *seg, const EdgePair *edge,
                     Sector *next_sector);

// Compute the EdgePair for a portal wall segment.
// cam_focal_aspect: camera focal_len_aspect.
// cam_proj_y: camera proj_offset_y.
// floor_h, ceil_h: current sector heights (float).
// next_floor_h, next_ceil_h: neighbor sector heights (float).
// cam_y: camera Y position.
// seg: the projected wall segment.
// out: filled on success.
void adjoin_compute_edge_pair(float cam_focal_aspect, float cam_proj_y, float cam_y,
                              float floor_h, float ceil_h, float next_floor_h,
                              float next_ceil_h, const WallSegment *seg, EdgePair *out);

// Save the current renderer state before recursing.
void adjoin_save_state(AdjoinSaveState *state, int32_t min_x, int32_t max_x,
                       int32_t min_y, int32_t max_y, float ambient, float scaled_ambient);

// Restore renderer state after recursion.
void adjoin_restore_state(const AdjoinSaveState *state, int32_t *min_x, int32_t *max_x,
                          int32_t *min_y, int32_t *max_y, float *ambient,
                          float *scaled_ambient);

#endif /* Q16_RENDER_ADJOIN_H */
