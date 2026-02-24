// ===========================================================================
// 1D Depth Buffer
// ===========================================================================
#ifndef Q16_RENDER_DEPTH_H
#define Q16_RENDER_DEPTH_H

#include "render/render_limits.h"
#include <stdbool.h>
#include <stdint.h>

// Per-column (not per-pixel) depth values. Each screen column stores the
// depth of the closest wall rendered at that column. Used for sprite
// occlusion testing. One depth array per adjoin recursion level.

typedef float DepthValue;

typedef struct {
  DepthValue *buffer;      // flat allocation: width * (max_depth + 1)
  int32_t width;
  int32_t max_depth;
  int32_t current_depth;   // current adjoin recursion level (1-based)
} DepthBuffer;

// Allocate the depth buffer for a given screen width and max adjoin depth.
// Returns false on allocation failure.
bool depth_buffer_init(DepthBuffer *db, int32_t width, int32_t max_depth);

// Free all memory.
void depth_buffer_destroy(DepthBuffer *db);

// Reset all depth values to max (infinity) and set current_depth to 1.
void depth_buffer_reset(DepthBuffer *db);

// Get a pointer to the current depth level's column array.
DepthValue *depth_buffer_current(const DepthBuffer *db);

// Get a pointer to the depth array at a specific level (0-based internal).
DepthValue *depth_buffer_at_level(const DepthBuffer *db, int32_t level);

// Enter an adjoin: increment depth, copy parent range to child.
// min_x/max_x: column range to copy.
// Returns false if max depth exceeded.
bool depth_buffer_enter_adjoin(DepthBuffer *db, int32_t min_x, int32_t max_x);

// Exit an adjoin: copy child range back to parent (for non-subsectors),
// then decrement depth.
// copy_back: if true, copy child depth values back to parent (normal sectors).
//            if false, skip copy (subsector with prev_draw_frame2 guard).
void depth_buffer_exit_adjoin(DepthBuffer *db, int32_t min_x, int32_t max_x,
                              bool copy_back);

#endif /* Q16_RENDER_DEPTH_H */
