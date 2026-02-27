// ===========================================================================
// S-Buffer (Segment Buffer)
// ===========================================================================
// 2D occlusion on the XZ plane using separating-plane depth test.

#include "render/sbuffer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// --- Pool allocation -------------------------------------------------------

static SBufferSeg *sbuffer_alloc(SBuffer *sb) {
  if (sb->pool_used >= sb->pool_capacity) return NULL;
  SBufferSeg *seg = &sb->pool[sb->pool_used++];
  memset(seg, 0, sizeof(SBufferSeg));
  return seg;
}

// --- Linked-list helpers ---------------------------------------------------

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

// --- Separating-plane depth test -------------------------------------------

typedef enum {
  SBUF_NEW_FRONT,
  SBUF_CUR_FRONT,
  SBUF_INTERSECT
} SBufferOverlap;

// Determine which of two wall segments is in front (closer to the camera at
// origin). Uses normal-based separating-plane test: classify both endpoints of
// one wall against the other wall's plane, then compare against the camera side.
static SBufferOverlap sbuffer_segment_in_front(const SBufferSeg *new_seg,
                                               const SBufferSeg *cur) {
  // Classify new_seg's endpoints against cur's plane.
  float side_a0 = new_seg->vx0 * cur->normal_x + new_seg->vz0 * cur->normal_z
                  - cur->normal_d;
  float side_a1 = new_seg->vx1 * cur->normal_x + new_seg->vz1 * cur->normal_z
                  - cur->normal_d;

  // Camera is at origin, so its signed distance to cur's plane is -normal_d.
  float side_cam = -cur->normal_d;

  // Both endpoints on the same side as camera → new is in front.
  if (side_a0 * side_cam > 0.0f && side_a1 * side_cam > 0.0f)
    return SBUF_NEW_FRONT;

  // Both endpoints on the opposite side from camera → new is behind.
  if (side_a0 * side_cam < 0.0f && side_a1 * side_cam < 0.0f) {
    // Double-check: classify cur's endpoints against new_seg's plane.
    float side_b0 = cur->vx0 * new_seg->normal_x + cur->vz0 * new_seg->normal_z
                    - new_seg->normal_d;
    float side_b1 = cur->vx1 * new_seg->normal_x + cur->vz1 * new_seg->normal_z
                    - new_seg->normal_d;
    float side_cam2 = -new_seg->normal_d;

    if (side_b0 * side_cam2 < 0.0f && side_b1 * side_cam2 < 0.0f)
      return SBUF_NEW_FRONT;

    return SBUF_CUR_FRONT;
  }

  // One endpoint on each side — but check the reverse test first. If the
  // reverse test gives a clear answer, use it.
  float side_b0 = cur->vx0 * new_seg->normal_x + cur->vz0 * new_seg->normal_z
                  - new_seg->normal_d;
  float side_b1 = cur->vx1 * new_seg->normal_x + cur->vz1 * new_seg->normal_z
                  - new_seg->normal_d;
  float side_cam2 = -new_seg->normal_d;

  if (side_b0 * side_cam2 > 0.0f && side_b1 * side_cam2 > 0.0f)
    return SBUF_CUR_FRONT;

  if (side_b0 * side_cam2 < 0.0f && side_b1 * side_cam2 < 0.0f)
    return SBUF_NEW_FRONT;

  return SBUF_INTERSECT;
}

// Compute the crossing point (in projected space) where two walls intersect.
// Uses linear interpolation of signed distances to the plane.
static float sbuffer_crossing_point(const SBufferSeg *new_seg,
                                    const SBufferSeg *cur,
                                    float ov_start, float ov_end) {
  float side_a0 = new_seg->vx0 * cur->normal_x + new_seg->vz0 * cur->normal_z
                  - cur->normal_d;
  float side_a1 = new_seg->vx1 * cur->normal_x + new_seg->vz1 * cur->normal_z
                  - cur->normal_d;

  float denom = side_a0 - side_a1;
  if (fabsf(denom) < 1e-8f)
    return (ov_start + ov_end) * 0.5f;

  // t along the new_seg where signed distance == 0
  float t = side_a0 / denom;
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  float cross = new_seg->start + t * (new_seg->end - new_seg->start);

  if (cross <= ov_start) cross = ov_start + 1e-4f;
  if (cross >= ov_end)   cross = ov_end   - 1e-4f;

  return cross;
}

// --- Depth interpolation (for sprites only) --------------------------------

static float sbuffer_lerp_depth(float seg_start, float seg_end,
                                float z0, float z1, float proj) {
  float range = seg_end - seg_start;
  if (range < 1e-8f) return z0;
  float t = (proj - seg_start) / range;
  return z0 + t * (z1 - z0);
}

// --- Lifecycle -------------------------------------------------------------

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

// --- Projection ------------------------------------------------------------

float sbuffer_project(float vx, float vz) {
  float angle = atan2f(vx, vz);
  float projected = angle * (2.0f / (float)M_PI) + 1.5f;
  if (projected < 0.0f) projected += 4.0f;
  if (projected >= 4.0f) projected -= 4.0f;
  return projected;
}

// --- Fill helper -----------------------------------------------------------

static void sbuffer_fill_seg(SBufferSeg *seg, float start, float end,
                             float vx0, float vz0, float vx1, float vz1,
                             float nx, float nz, float nd,
                             int32_t wall_id, bool is_portal, Wall *src_wall) {
  seg->start    = start;
  seg->end      = end;
  seg->vx0      = vx0;
  seg->vz0      = vz0;
  seg->vx1      = vx1;
  seg->vz1      = vz1;
  seg->normal_x = nx;
  seg->normal_z = nz;
  seg->normal_d = nd;
  seg->wall_id  = wall_id;
  seg->is_portal = is_portal;
  seg->src_wall  = src_wall;
}

// --- Insert ----------------------------------------------------------------

SBufferSeg *sbuffer_insert(SBuffer *sb, float start, float end,
                           float vx0, float vz0, float vx1, float vz1,
                           float nx, float nz, float nd,
                           int32_t wall_id, bool is_portal, Wall *src_wall) {
  // Wrap-around: split at the 4.0 boundary.
  if (end < start) {
    sbuffer_insert(sb, start, 4.0f, vx0, vz0, vx1, vz1,
                   nx, nz, nd, wall_id, is_portal, src_wall);
    return sbuffer_insert(sb, 0.0f, end, vx0, vz0, vx1, vz1,
                          nx, nz, nd, wall_id, is_portal, src_wall);
  }

  if (end - start < 1e-6f) return NULL;

  SBufferSeg *cur = sb->head.next;
  while (cur != &sb->tail && cur->end <= start)
    cur = cur->next;

  float seg_start = start;
  float seg_end   = end;
  SBufferSeg *result = NULL;

  // Temporary stack seg for plane tests (avoids allocating before needed).
  SBufferSeg new_seg;
  sbuffer_fill_seg(&new_seg, start, end, vx0, vz0, vx1, vz1,
                   nx, nz, nd, wall_id, is_portal, src_wall);

  while (cur != &sb->tail && seg_start < seg_end) {
    // Gap before cur: insert new segment into the gap.
    if (cur->start >= seg_end) {
      SBufferSeg *seg = sbuffer_alloc(sb);
      if (!seg) return result;
      sbuffer_fill_seg(seg, seg_start, seg_end, vx0, vz0, vx1, vz1,
                       nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(cur->prev, seg);
      if (!result) result = seg;
      return result;
    }

    if (seg_start < cur->start) {
      float gap_end = cur->start < seg_end ? cur->start : seg_end;
      SBufferSeg *seg = sbuffer_alloc(sb);
      if (!seg) return result;
      sbuffer_fill_seg(seg, seg_start, gap_end, vx0, vz0, vx1, vz1,
                       nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(cur->prev, seg);
      if (!result) result = seg;
      seg_start = gap_end;
    }

    // Compute overlap range.
    float ov_start = seg_start > cur->start ? seg_start : cur->start;
    float ov_end   = seg_end   < cur->end   ? seg_end   : cur->end;

    if (ov_start < ov_end) {
      // Same wall — skip depth test.
      if (src_wall != NULL && cur->src_wall == src_wall) {
        seg_start = ov_end;
        cur = cur->next;
        continue;
      }

      SBufferOverlap overlap = sbuffer_segment_in_front(&new_seg, cur);

      if (overlap == SBUF_INTERSECT) {
        float cross = sbuffer_crossing_point(&new_seg, cur, ov_start, ov_end);

        float left_mid = (ov_start + cross) * 0.5f;
        float left_d_new = sbuffer_lerp_depth(start, end, vz0, vz1, left_mid);
        float left_d_cur = sbuffer_lerp_depth(cur->start, cur->end,
                                              cur->vz0, cur->vz1, left_mid);
        bool new_front_left = (left_d_new < left_d_cur);

        // Handle both halves atomically to avoid re-testing the same pair.
        // Save cur's wall data before modifying the list.
        SBufferSeg cur_copy = *cur;
        SBufferSeg *next_cur = cur->next;

        // Preserve cur prefix [cur->start, ov_start) if it extends before overlap.
        SBufferSeg *insert_point = cur->prev;
        if (cur->start < ov_start) {
          SBufferSeg *prefix = sbuffer_alloc(sb);
          if (prefix) {
            *prefix = cur_copy;
            prefix->end = ov_start;
            sbuffer_link_after(insert_point, prefix);
            insert_point = prefix;
          }
        }

        // Preserve cur suffix [ov_end, cur->end) if it extends past overlap.
        if (cur_copy.end > ov_end) {
          SBufferSeg *suffix = sbuffer_alloc(sb);
          if (suffix) {
            *suffix = cur_copy;
            suffix->start = ov_end;
            sbuffer_link_after(cur, suffix);
            next_cur = suffix;
          }
        }

        // Remove cur from the overlap region.
        sbuffer_unlink(cur);

        // Insert two halves: [ov_start, cross) and [cross, ov_end).
        if (new_front_left) {
          SBufferSeg *left = sbuffer_alloc(sb);
          if (left) {
            sbuffer_fill_seg(left, ov_start, cross, vx0, vz0, vx1, vz1,
                             nx, nz, nd, wall_id, is_portal, src_wall);
            sbuffer_link_after(insert_point, left);
            if (!result) result = left;
            insert_point = left;
          }
          SBufferSeg *right = sbuffer_alloc(sb);
          if (right) {
            *right = cur_copy;
            right->start = cross;
            right->end = ov_end;
            sbuffer_link_after(insert_point, right);
          }
        } else {
          SBufferSeg *left = sbuffer_alloc(sb);
          if (left) {
            *left = cur_copy;
            left->start = ov_start;
            left->end = cross;
            sbuffer_link_after(insert_point, left);
            insert_point = left;
          }
          SBufferSeg *right = sbuffer_alloc(sb);
          if (right) {
            sbuffer_fill_seg(right, cross, ov_end, vx0, vz0, vx1, vz1,
                             nx, nz, nd, wall_id, is_portal, src_wall);
            sbuffer_link_after(insert_point, right);
            if (!result) result = right;
          }
        }

        seg_start = ov_end;
        cur = next_cur;
        continue;
      }

      if (overlap == SBUF_NEW_FRONT) {
        // New segment is closer — carve out the overlap from cur.
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

          SBufferSeg *seg = sbuffer_alloc(sb);
          if (seg) {
            sbuffer_fill_seg(seg, ov_start, ov_end, vx0, vz0, vx1, vz1,
                             nx, nz, nd, wall_id, is_portal, src_wall);
            sbuffer_link_after(cur->prev, seg);
            if (!result) result = seg;
          }
          seg_start = ov_end;
          continue;
        }

        SBufferSeg *seg = sbuffer_alloc(sb);
        if (seg) {
          sbuffer_fill_seg(seg, ov_start, ov_end, vx0, vz0, vx1, vz1,
                           nx, nz, nd, wall_id, is_portal, src_wall);
          sbuffer_link_after(cur->prev, seg);
          if (!result) result = seg;
        }
      }
      // SBUF_CUR_FRONT: existing segment wins — new is occluded, skip.
    }

    seg_start = ov_end;
    cur = cur->next;
  }

  // Remaining gap after all existing segments.
  if (seg_start < seg_end) {
    SBufferSeg *seg = sbuffer_alloc(sb);
    if (seg) {
      sbuffer_fill_seg(seg, seg_start, seg_end, vx0, vz0, vx1, vz1,
                       nx, nz, nd, wall_id, is_portal, src_wall);
      sbuffer_link_after(sb->tail.prev, seg);
      if (!result) result = seg;
    }
  }

  return result;
}

// --- Sprite clipping (read-only) -------------------------------------------

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
      float mid = (ov_start + ov_end) * 0.5f;

      out_spans[count].start = ov_start;
      out_spans[count].end   = ov_end;
      out_spans[count].depth = sbuffer_lerp_depth(cur->start, cur->end,
                                                  cur->vz0, cur->vz1, mid);
      count++;
    }

    start = cur->end;
    cur = cur->next;
  }
  return count;
}

// --- Iteration -------------------------------------------------------------

const SBufferSeg *sbuffer_first(const SBuffer *sb) {
  const SBufferSeg *seg = sb->head.next;
  return (seg != &sb->tail) ? seg : NULL;
}

const SBufferSeg *sbuffer_next(const SBuffer *sb, const SBufferSeg *seg) {
  const SBufferSeg *n = seg->next;
  return (n != &sb->tail) ? n : NULL;
}
