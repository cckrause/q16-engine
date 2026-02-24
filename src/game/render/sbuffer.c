// ===========================================================================
// S-Buffer (Segment Buffer)
// ===========================================================================
// 2D occlusion on the XZ plane for GPU-based portal rendering.

#include "render/sbuffer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Pool allocation

static SBufferSeg *sbuffer_alloc(SBuffer *sb) {
  if (sb->pool_used >= sb->pool_capacity) return NULL;
  SBufferSeg *seg = &sb->pool[sb->pool_used++];
  memset(seg, 0, sizeof(SBufferSeg));
  return seg;
}

// Linked-list helpers

static void sbuffer_link_after(SBufferSeg *after, SBufferSeg *seg) {
  seg->prev = after;
  seg->next = after->next;
  after->next->prev = seg;
  after->next = seg;
}

static void sbuffer_unlink(SBufferSeg *seg) {
  seg->prev->next = seg->next;
  seg->next->prev = seg->prev;
  seg->prev = NULL;
  seg->next = NULL;
}

// Depth comparison

// A is closer to the camera when normal_d is more negative (camera is at
// origin, so smaller normal_d means the wall plane is further behind the
// camera's side).
static bool sbuffer_is_in_front(const SBufferSeg *a, const SBufferSeg *b) {
  return a->normal_d < b->normal_d;
}

// Lifecycle

bool sbuffer_init(SBuffer *sb, int32_t pool_capacity) {
  sb->pool = (SBufferSeg *)malloc((size_t)pool_capacity * sizeof(SBufferSeg));
  if (!sb->pool) return false;

  sb->pool_capacity = pool_capacity;
  sb->pool_used     = 0;

  sb->head.start   = -1.0f;
  sb->head.end     = -1.0f;
  sb->head.wall_id = -1;
  sb->head.prev    = NULL;
  sb->tail.start   = 5.0f;
  sb->tail.end     = 5.0f;
  sb->tail.wall_id = -1;
  sb->tail.next    = NULL;

  sb->head.next = &sb->tail;
  sb->tail.prev = &sb->head;
  return true;
}

void sbuffer_destroy(SBuffer *sb) {
  free(sb->pool);
  sb->pool = NULL;
}

void sbuffer_reset(SBuffer *sb) {
  sb->pool_used = 0;
  sb->head.next = &sb->tail;
  sb->tail.prev = &sb->head;
}

// Unit-square projection

float sbuffer_project(float vx, float vz) {
  /*
   * Map view-space (x, z) to [0, 4):
   *   0.5 = -X, 1.5 = +Z, 2.5 = +X, 3.5 = -Z.
   * atan2(x, z) gives [-pi, pi], scaled and offset to [0, 4).
   */
  float angle = atan2f(vx, vz);
  float projected = angle * (2.0f / (float)M_PI) + 1.5f;
  if (projected < 0.0f) projected += 4.0f;
  if (projected >= 4.0f) projected -= 4.0f;
  return projected;
}

// Insert

static void sbuffer_fill_seg(SBufferSeg *seg, float start, float end,
                             float nx, float nz, float nd,
                             int32_t wall_id, bool is_portal, Wall *src_wall) {
  seg->start     = start;
  seg->end       = end;
  seg->normal_x  = nx;
  seg->normal_z  = nz;
  seg->normal_d  = nd;
  seg->wall_id   = wall_id;
  seg->is_portal = is_portal;
  seg->src_wall  = src_wall;
}

SBufferSeg *sbuffer_insert(SBuffer *sb, float start, float end,
                           float nx, float nz, float nd,
                           int32_t wall_id, bool is_portal, Wall *src_wall) {
  // Wrap-around: split at the 4.0 boundary.
  if (end < start) {
    sbuffer_insert(sb, start, 4.0f, nx, nz, nd, wall_id, is_portal, src_wall);
    return sbuffer_insert(sb, 0.0f, end, nx, nz, nd, wall_id, is_portal, src_wall);
  }

  if (end - start < 1e-6f) return NULL;

  SBufferSeg *cur = sb->head.next;
  while (cur != &sb->tail && cur->end <= start)
    cur = cur->next;

  float seg_start = start;
  float seg_end   = end;
  SBufferSeg *result = NULL;

  while (cur != &sb->tail && seg_start < seg_end) {
    if (cur->start >= seg_end) {
      SBufferSeg *seg = sbuffer_alloc(sb);
      if (!seg) return result;
      sbuffer_fill_seg(seg, seg_start, seg_end, nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(cur->prev, seg);
      return seg;
    }

    // Non-overlapping prefix before current segment.
    if (seg_start < cur->start) {
      float gap_end = cur->start;
      if (gap_end > seg_end) gap_end = seg_end;

      SBufferSeg *seg = sbuffer_alloc(sb);
      if (!seg) return result;
      sbuffer_fill_seg(seg, seg_start, gap_end, nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(cur->prev, seg);
      if (!result) result = seg;
      seg_start = gap_end;
    }

    float ov_start = seg_start > cur->start ? seg_start : cur->start;
    float ov_end   = seg_end   < cur->end   ? seg_end   : cur->end;

    if (ov_start < ov_end) {
      SBufferSeg test_new;
      test_new.normal_x = nx;
      test_new.normal_z = nz;
      test_new.normal_d = nd;

      if (sbuffer_is_in_front(&test_new, cur)) {
        if (cur->start < ov_start) {
          SBufferSeg *prefix = sbuffer_alloc(sb);
          if (prefix) {
            *prefix = *cur;
            prefix->end = ov_start;
            sbuffer_link_after(cur->prev, prefix);
          }
        }

        if (cur->end > ov_end) {
          cur->start = ov_end;
        } else {
          SBufferSeg *next = cur->next;
          if (cur->start >= ov_start && cur->end <= ov_end) {
            sbuffer_unlink(cur);
          } else if (cur->end <= ov_end) {
            cur->end = ov_start;
            if (cur->end <= cur->start) sbuffer_unlink(cur);
          }
          cur = next;
          continue;
        }

        SBufferSeg *seg = sbuffer_alloc(sb);
        if (seg) {
          sbuffer_fill_seg(seg, ov_start, ov_end, nx, nz, nd, wall_id, is_portal, src_wall);
          sbuffer_link_after(cur->prev, seg);
          if (!result) result = seg;
        }
      }
    }

    seg_start = ov_end;
    cur = cur->next;
  }

  if (seg_start < seg_end) {
    SBufferSeg *seg = sbuffer_alloc(sb);
    if (seg) {
      sbuffer_fill_seg(seg, seg_start, seg_end, nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(sb->tail.prev, seg);
      if (!result) result = seg;
    }
  }

  return result;
}

// Sprite clipping query (read-only)

int32_t sbuffer_clip_sprite(const SBuffer *sb, float start, float end,
                            SBufferSpriteSpan *out_spans, int32_t max_spans) {
  int32_t count = 0;
  const SBufferSeg *cur = sb->head.next;

  while (cur != &sb->tail && start < end && count < max_spans) {
    if (cur->end <= start) {
      cur = cur->next;
      continue;
    }
    if (cur->start >= end) break;

    float ov_start = start > cur->start ? start : cur->start;
    float ov_end   = end   < cur->end   ? end   : cur->end;

    if (ov_start < ov_end) {
      out_spans[count].start = ov_start;
      out_spans[count].end   = ov_end;
      out_spans[count].depth = -cur->normal_d;
      count++;
    }

    start = cur->end;
    cur = cur->next;
  }
  return count;
}

// Iteration

const SBufferSeg *sbuffer_first(const SBuffer *sb) {
  const SBufferSeg *seg = sb->head.next;
  return (seg != &sb->tail) ? seg : NULL;
}

const SBufferSeg *sbuffer_next(const SBuffer *sb, const SBufferSeg *seg) {
  const SBufferSeg *n = seg->next;
  return (n != &sb->tail) ? n : NULL;
}
