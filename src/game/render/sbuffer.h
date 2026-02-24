// ===========================================================================
// S-Buffer (Segment Buffer)
// ===========================================================================
#ifndef Q16_RENDER_SBUFFER_H
#define Q16_RENDER_SBUFFER_H

#include "render/render_limits.h"
#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// 2D occlusion structure replacing the 1D column depth buffer for GPU-based
// rendering. Operates entirely on the XZ plane using a unit-square projection
// that maps the full 360-degree view onto [0, 4).
//
// Segments are stored in a sorted doubly-linked list. Overlaps are resolved
// by a separating-axis test on wall normals. Sprites query the buffer
// without modifying it.

typedef struct SBufferSeg {
  float start; // projected start [0, 4)
  float end;   // projected end [0, 4)

  // Wall normal in view-space XZ (for depth comparisons).
  float normal_x;
  float normal_z;
  float normal_d; // dot(normal, wall_point) for depth test

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

// Initialize the S-Buffer with a pool of the given capacity.
// Returns false on allocation failure.
bool sbuffer_init(SBuffer *sb, int32_t pool_capacity);

void sbuffer_destroy(SBuffer *sb);

// Reset for a new frame: free all segments, reinitialize the linked list.
void sbuffer_reset(SBuffer *sb);

// Project a view-space XZ point onto the [0, 4) unit-square range.
// 0.5 = -X direction, 1.5 = +Z, 2.5 = +X, 3.5 = -Z.
float sbuffer_project(float vx, float vz);

// Insert a wall segment into the S-Buffer.
// Overlaps with existing segments are resolved by the separating-axis depth
// test. Occluded portions are clipped or discarded. Adjacent segments with
// the same wall_id are merged.
// Returns the inserted segment (or NULL if fully occluded).
SBufferSeg *sbuffer_insert(SBuffer *sb, float start, float end, float nx, float nz,
                           float nd, int32_t wall_id, bool is_portal, Wall *src_wall);

// Query visible sub-segments for a sprite's projected range.
// Does NOT modify the S-Buffer.
// out_spans: caller-provided array.
// max_spans: capacity of out_spans.
// Returns the number of spans written.
int32_t sbuffer_clip_sprite(const SBuffer *sb, float start, float end,
                            SBufferSpriteSpan *out_spans, int32_t max_spans);

// Iterate visible segments (for display list emission).
// Returns the first segment after the sentinel head. NULL if empty.
const SBufferSeg *sbuffer_first(const SBuffer *sb);

// Advance to the next segment. Returns NULL at the tail sentinel.
const SBufferSeg *sbuffer_next(const SBuffer *sb, const SBufferSeg *seg);

#endif /* Q16_RENDER_SBUFFER_H */
