// ===========================================================================
// Window / Column Tracking System
// ===========================================================================
#ifndef Q16_RENDER_WINDOW_H
#define Q16_RENDER_WINDOW_H

#include "render/render_limits.h"
#include <stdbool.h>
#include <stdint.h>

// Tracks the vertical pixel range still available for rendering at each
// screen column. Walls narrow the window as they are drawn; adjoins use
// the window to limit what the child sector can render.
// One window_top/window_bot pair per adjoin recursion level.

typedef struct {
  int32_t *top; // flat allocation: width * (max_depth + 1)
  int32_t *bot; // flat allocation: width * (max_depth + 1)
  int32_t width;
  int32_t height;
  int32_t max_depth;
  int32_t current_depth; // 1-based

  // Global pixel bounds (tightened by adjoin_compute_window_bounds).
  int32_t min_x;
  int32_t max_x;
  int32_t min_y;
  int32_t max_y;
} RenderWindow;

// Allocate window arrays. Returns false on allocation failure.
bool render_window_init(RenderWindow *rw, int32_t width, int32_t height,
                        int32_t max_depth);

void render_window_destroy(RenderWindow *rw);

// Reset to full screen: top[x] = 0, bot[x] = height-1 for all columns.
// Sets depth to 1 and global bounds to the full screen.
void render_window_reset(RenderWindow *rw);

// Get pointers to the current depth level's top/bot arrays.
int32_t *render_window_top(const RenderWindow *rw);
int32_t *render_window_bot(const RenderWindow *rw);

// Get pointers at a specific level (0-based internal).
int32_t *render_window_top_at(const RenderWindow *rw, int32_t level);
int32_t *render_window_bot_at(const RenderWindow *rw, int32_t level);

// --- Narrow operations (used during wall drawing) --------------------------

// Solid wall: mark column as fully occluded.
static inline void render_window_narrow_solid(RenderWindow *rw, int32_t x) {
  int32_t *top = render_window_top(rw);
  int32_t *bot = render_window_bot(rw);
  top[x] = rw->height;
  bot[x] = -1;
}

// Top wall (adjoin upper portion): push top down past the wall.
static inline void render_window_narrow_top(RenderWindow *rw, int32_t x, int32_t ceil_y) {
  int32_t *top = render_window_top(rw);
  if (ceil_y + 1 > top[x])
    top[x] = ceil_y + 1;
}

// Bottom wall (adjoin lower portion): push bottom up past the wall.
static inline void render_window_narrow_bot(RenderWindow *rw, int32_t x,
                                            int32_t floor_y) {
  int32_t *bot = render_window_bot(rw);
  if (floor_y - 1 < bot[x])
    bot[x] = floor_y - 1;
}

// Enter an adjoin: increment depth, copy parent window to child.
bool render_window_enter_adjoin(RenderWindow *rw, int32_t min_x, int32_t max_x);

// Exit an adjoin: decrement depth.
void render_window_exit_adjoin(RenderWindow *rw);

#endif /* Q16_RENDER_WINDOW_H */
