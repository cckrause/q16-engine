// ===========================================================================
// S-Buffer (Segment Buffer)
// ===========================================================================
#ifndef Q16_RENDER_SBUFFER_H
#define Q16_RENDER_SBUFFER_H

#include "render/render_limits.h"
#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// 2D occlusion structure for GPU-based portal rendering. Operates on the
// XZ plane. Horizontal angle (atan2(vx,vz)) is mapped to a 1D coordinate
// in [0, 4) so that 1 unit = 90° (full circle = 4). Segment overlap in
// this coordinate is used for occlusion; depth is resolved by a
// separating-plane test on wall normals (geometrically exact for
// non-intersecting walls).

typedef struct SBufferSeg {
  float start; // projected start [0, 4)
  float end;   // projected end [0, 4)

  // Cam-space wall vertices and normal for separating-plane depth test.
  float vx0, vz0;
  float vx1, vz1;
  float normal_x, normal_z;
  float normal_d; // dot(normal, vertex) — signed distance from origin

  int32_t wall_id; // index into level wall array (-1 = sentinel)
  bool is_portal;  // true if this is an adjoin wall
  Wall *src_wall;  // source wall pointer (NULL for sentinels)

  struct SBufferSeg *prev;
  struct SBufferSeg *next;
} SBufferSeg;

// Result of clipping a sprite against the S-Buffer.
typedef struct {
  float start;
  float end;
  float depth; // approximate depth at this sub-segment
} SBufferSpriteSpan;

typedef struct {
  // Pool of segment nodes.
  SBufferSeg *pool;
  int32_t pool_capacity;
  int32_t pool_used;

  // Sentinel head/tail for the sorted doubly-linked list.
  SBufferSeg head;
  SBufferSeg tail;
} SBuffer;

bool sbuffer_init(SBuffer *sb, int32_t pool_capacity);
void sbuffer_destroy(SBuffer *sb);
void sbuffer_reset(SBuffer *sb);

// Project a view-space XZ point onto the [0, 4) unit-square range.
float sbuffer_project(float vx, float vz);

// Insert a wall segment into the S-Buffer.
// vx0/vz0, vx1/vz1: cam-space wall endpoints.
// nx/nz/nd: cam-space wall normal and signed distance.
// Overlaps resolved by separating-plane test; intersecting walls are split.
// Returns the inserted segment (or NULL if fully occluded).
SBufferSeg *sbuffer_insert(SBuffer *sb, float start, float end,
                           float vx0, float vz0, float vx1, float vz1,
                           float nx, float nz, float nd,
                           int32_t wall_id, bool is_portal, Wall *src_wall);

// Query visible sub-segments for a sprite's projected range (read-only).
int32_t sbuffer_clip_sprite(const SBuffer *sb, float start, float end,
                            SBufferSpriteSpan *out_spans, int32_t max_spans);

const SBufferSeg *sbuffer_first(const SBuffer *sb);
const SBufferSeg *sbuffer_next(const SBuffer *sb, const SBufferSeg *seg);

#endif /* Q16_RENDER_SBUFFER_H */
