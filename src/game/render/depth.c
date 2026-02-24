// ===========================================================================
// 1D Depth Buffer
// ===========================================================================
// Per-column depth tracking for sprite occlusion during portal rendering.

#include "render/depth.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>

bool depth_buffer_init(DepthBuffer *db, int32_t width, int32_t max_depth) {
  int32_t total = width * (max_depth + 1);
  db->buffer = (DepthValue *)malloc((size_t)total * sizeof(DepthValue));
  if (!db->buffer)
    return false;

  db->width = width;
  db->max_depth = max_depth;
  db->current_depth = 1;
  return true;
}

void depth_buffer_destroy(DepthBuffer *db) {
  free(db->buffer);
  db->buffer = NULL;
  db->width = 0;
}

void depth_buffer_reset(DepthBuffer *db) {
  db->current_depth = 1;
  int32_t total = db->width * (db->max_depth + 1);
  for (int32_t i = 0; i < total; i++) {
    db->buffer[i] = FLT_MAX;
  }
}

DepthValue *depth_buffer_current(const DepthBuffer *db) {
  int32_t level = db->current_depth - 1;
  return &db->buffer[level * db->width];
}

DepthValue *depth_buffer_at_level(const DepthBuffer *db, int32_t level) {
  return &db->buffer[level * db->width];
}

bool depth_buffer_enter_adjoin(DepthBuffer *db, int32_t min_x, int32_t max_x) {
  if (db->current_depth > db->max_depth)
    return false;

  DepthValue *parent = depth_buffer_current(db);
  db->current_depth++;
  DepthValue *child = depth_buffer_current(db);

  int32_t count = max_x - min_x + 1;
  if (count > 0) {
    memcpy(&child[min_x], &parent[min_x], (size_t)count * sizeof(DepthValue));
  }
  return true;
}

void depth_buffer_exit_adjoin(DepthBuffer *db, int32_t min_x, int32_t max_x,
                              bool copy_back) {
  if (copy_back && db->current_depth > 1) {
    DepthValue *child = depth_buffer_current(db);
    db->current_depth--;
    DepthValue *parent = depth_buffer_current(db);

    int32_t count = max_x - min_x + 1;
    if (count > 0) {
      memcpy(&parent[min_x], &child[min_x], (size_t)count * sizeof(DepthValue));
    }
  } else {
    if (db->current_depth > 1)
      db->current_depth--;
  }
}
