// ===========================================================================
// Flat Edge Tracking
// ===========================================================================
#ifndef Q16_RENDER_FLAT_H
#define Q16_RENDER_FLAT_H

#include "render/render_limits.h"
#include "render/wall_process.h"
#include <stdbool.h>
#include <stdint.h>

// Tracks the horizontal boundaries of floor and ceiling regions as walls
// are processed. After all walls in a sector have been drawn, these edges
// define the screen areas that need flat (floor/ceiling) geometry.
//
// The GPU path does not rasterize scanlines; instead it records the edges
// so the display list builder can emit floor/ceiling quads.

typedef struct {
  int32_t x0;
  int32_t x1;
  float   y;      // floating-point Y at x0
  float   dy_dx;  // slope: Y step per pixel
} FlatEdge;

typedef struct {
  // Ceiling and floor edge arrays, one entry per screen column.
  // column_top[x] / column_bot[x] track the pixel where the flat starts.
  int32_t *column_top;
  int32_t *column_bot;
  int32_t width;

  // Per-sector sky flags (set before processing walls).
  bool is_exterior_ceiling; // SEC_FLAG1_EXTERIOR — sky instead of ceiling
  bool is_pit_floor;        // SEC_FLAG1_PIT — sky instead of floor
} FlatState;

// Allocate column arrays for the given screen width.
// Returns false on allocation failure.
bool flat_init(FlatState *fs, int32_t width);

void flat_destroy(FlatState *fs);

// Reset to full-screen defaults before processing a sector's walls.
// top[x] = 0, bot[x] = height - 1.
void flat_reset(FlatState *fs, int32_t screen_height);

// Update column edges after a solid wall has been drawn.
// Columns in [x0, x1] have their top/bot set to the wall's vertical extent.
void flat_update_solid(FlatState *fs, int32_t x0, int32_t x1,
                       const int32_t *ceil_y, const int32_t *floor_y);

// Update column edges after an adjoin wall's upper portion.
void flat_update_top(FlatState *fs, int32_t x0, int32_t x1,
                     const int32_t *ceil_y);

// Update column edges after an adjoin wall's lower portion.
void flat_update_bot(FlatState *fs, int32_t x0, int32_t x1,
                     const int32_t *floor_y);

#endif /* Q16_RENDER_FLAT_H */
