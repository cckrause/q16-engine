// ===========================================================================
// Window / Column Tracking
// ===========================================================================
// Per-column vertical bounds for portal rendering.

#include "render/window.h"
#include <stdlib.h>
#include <string.h>

bool render_window_init(RenderWindow *rw, int32_t width, int32_t height,
                        int32_t max_depth) {
  int32_t total = width * (max_depth + 1);
  rw->top = (int32_t *)malloc((size_t)total * sizeof(int32_t));
  rw->bot = (int32_t *)malloc((size_t)total * sizeof(int32_t));
  if (!rw->top || !rw->bot) {
    free(rw->top);
    free(rw->bot);
    rw->top = NULL;
    rw->bot = NULL;
    return false;
  }

  rw->width = width;
  rw->height = height;
  rw->max_depth = max_depth;
  rw->current_depth = 1;
  rw->min_x = 0;
  rw->max_x = width - 1;
  rw->min_y = 0;
  rw->max_y = height - 1;
  return true;
}

void render_window_destroy(RenderWindow *rw) {
  free(rw->top);
  free(rw->bot);
  rw->top = NULL;
  rw->bot = NULL;
}

void render_window_reset(RenderWindow *rw) {
  rw->current_depth = 1;
  rw->min_x = 0;
  rw->max_x = rw->width - 1;
  rw->min_y = 0;
  rw->max_y = rw->height - 1;

  int32_t *top = render_window_top(rw);
  int32_t *bot = render_window_bot(rw);
  for (int32_t x = 0; x < rw->width; x++) {
    top[x] = 0;
    bot[x] = rw->height - 1;
  }
}

int32_t *render_window_top(const RenderWindow *rw) {
  int32_t level = rw->current_depth - 1;
  return &rw->top[level * rw->width];
}

int32_t *render_window_bot(const RenderWindow *rw) {
  int32_t level = rw->current_depth - 1;
  return &rw->bot[level * rw->width];
}

int32_t *render_window_top_at(const RenderWindow *rw, int32_t level) {
  return &rw->top[level * rw->width];
}

int32_t *render_window_bot_at(const RenderWindow *rw, int32_t level) {
  return &rw->bot[level * rw->width];
}

bool render_window_enter_adjoin(RenderWindow *rw, int32_t min_x, int32_t max_x) {
  if (rw->current_depth > rw->max_depth)
    return false;

  int32_t *parent_top = render_window_top(rw);
  int32_t *parent_bot = render_window_bot(rw);

  rw->current_depth++;

  int32_t *child_top = render_window_top(rw);
  int32_t *child_bot = render_window_bot(rw);

  int32_t count = max_x - min_x + 1;
  if (count > 0) {
    memcpy(&child_top[min_x], &parent_top[min_x], (size_t)count * sizeof(int32_t));
    memcpy(&child_bot[min_x], &parent_bot[min_x], (size_t)count * sizeof(int32_t));
  }
  return true;
}

void render_window_exit_adjoin(RenderWindow *rw) {
  if (rw->current_depth > 1)
    rw->current_depth--;
}
