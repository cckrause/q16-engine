// ===========================================================================
// Flat Edge Tracking
// ===========================================================================
// Records per-column ceiling/floor boundaries as walls are drawn, so the
// display list builder knows which screen regions need flat geometry.

#include "render/flat.h"
#include <stdlib.h>
#include <string.h>

bool flat_init(FlatState *fs, int32_t width) {
  fs->column_top = (int32_t *)malloc((size_t)width * sizeof(int32_t));
  fs->column_bot = (int32_t *)malloc((size_t)width * sizeof(int32_t));
  if (!fs->column_top || !fs->column_bot) {
    free(fs->column_top);
    free(fs->column_bot);
    fs->column_top = NULL;
    fs->column_bot = NULL;
    return false;
  }

  fs->width = width;
  fs->is_exterior_ceiling = false;
  fs->is_pit_floor = false;
  return true;
}

void flat_destroy(FlatState *fs) {
  free(fs->column_top);
  free(fs->column_bot);
  fs->column_top = NULL;
  fs->column_bot = NULL;
}

void flat_reset(FlatState *fs, int32_t screen_height) {
  for (int32_t x = 0; x < fs->width; x++) {
    fs->column_top[x] = 0;
    fs->column_bot[x] = screen_height - 1;
  }
}

void flat_update_solid(FlatState *fs, int32_t x0, int32_t x1,
                       const int32_t *ceil_y, const int32_t *floor_y) {
  for (int32_t x = x0; x <= x1; x++) {
    int32_t col = x - x0;
    fs->column_top[x] = ceil_y[col];
    fs->column_bot[x] = floor_y[col];
  }
}

void flat_update_top(FlatState *fs, int32_t x0, int32_t x1,
                     const int32_t *ceil_y) {
  for (int32_t x = x0; x <= x1; x++) {
    int32_t col = x - x0;
    if (ceil_y[col] > fs->column_top[x])
      fs->column_top[x] = ceil_y[col];
  }
}

void flat_update_bot(FlatState *fs, int32_t x0, int32_t x1,
                     const int32_t *floor_y) {
  for (int32_t x = x0; x <= x1; x++) {
    int32_t col = x - x0;
    if (floor_y[col] < fs->column_bot[x])
      fs->column_bot[x] = floor_y[col];
  }
}
