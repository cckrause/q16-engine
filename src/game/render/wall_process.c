// ===========================================================================
// Wall Processing
// ===========================================================================
// Transforms, culls, clips, and projects wall geometry into screen-space
// segments for S-Buffer insertion and display list emission.

#include "render/wall_process.h"
#include "world/flags.h"
#include "world/sector.h"
#include "world/wall.h"
#include <math.h>
#include <string.h>

// Backface test

static bool wall_is_backfacing(float vx0, float vz0, float vx1, float vz1) {
  // For CW-wound sectors (Dark Forces convention, viewed from above with
  // +Z forward), the interior is on the side where cross < 0.
  // Cross >= 0 means the camera is on the interior side = front-facing.
  float dx = vx1 - vx0;
  float dz = vz1 - vz0;
  return (dx * vz0 - dz * vx0) < 0.0f;
}

// Adjoin classification

static uint32_t wall_classify_adjoin(float floor_h, float ceil_h, float next_floor_h,
                                     float next_ceil_h) {
  uint32_t flags = 0;
  if (next_ceil_h > ceil_h)
    flags |= WDF_TOP;
  if (next_floor_h < floor_h)
    flags |= WDF_BOT;
  return flags;
}

// Depth interpolation

static void wall_compute_depth_params(float sx0, float sz0, float sx1, float sz1,
                                      int32_t screen_x0, int32_t screen_x1,
                                      float *out_slope, float *out_numerator) {
  int32_t dx = screen_x1 - screen_x0;
  if (dx == 0) {
    *out_slope = 0.0f;
    *out_numerator = sz0;
    return;
  }

  // Linear interpolation of 1/z across screen X.
  float inv_z0 = 1.0f / sz0;
  float inv_z1 = 1.0f / sz1;
  *out_slope = (inv_z1 - inv_z0) / (float)dx;
  *out_numerator = inv_z0;
}

// Texture U coordinate mapping

static void wall_compute_texture_u(float vx0, float vz0, float vx1, float vz1,
                                   float t_clip0, float t_clip1, float texel_length,
                                   int32_t screen_x0, int32_t screen_x1, float *out_u0,
                                   float *out_du_dx) {
  // The texture U spans [0, texel_length] along the wall in world space.
  // Clipping shifts the start/end proportionally.
  float u_start = t_clip0 * texel_length;
  float u_end = t_clip1 * texel_length;

  int32_t dx = screen_x1 - screen_x0;
  if (dx == 0) {
    *out_u0 = u_start;
    *out_du_dx = 0.0f;
    return;
  }

  // Perspective-correct U interpolation approximation.
  // Full correctness would interpolate u/z, but the original engine uses
  // a linear approximation per-segment that is acceptable for narrow walls.
  *out_u0 = u_start;
  *out_du_dx = (u_end - u_start) / (float)dx;

  (void)vx0;
  (void)vz0;
  (void)vx1;
  (void)vz1;
}

// Wall normal in view space

static void wall_compute_normal(float vx0, float vz0, float vx1, float vz1, float *out_nx,
                                float *out_nz, float *out_nd) {
  float dx = vx1 - vx0;
  float dz = vz1 - vz0;

  // 2D normal: perpendicular to wall direction, pointing toward camera.
  *out_nx = -dz;
  *out_nz = dx;

  float len = sqrtf((*out_nx) * (*out_nx) + (*out_nz) * (*out_nz));
  if (len > 1e-8f) {
    float inv = 1.0f / len;
    *out_nx *= inv;
    *out_nz *= inv;
  }

  // Signed distance from origin to the wall's supporting line.
  *out_nd = (*out_nx) * vx0 + (*out_nz) * vz0;
}

bool wall_process(const CameraState *cam, const Frustum *frustum, Wall *wall,
                  int32_t wall_index, float floor_h, float ceil_h, float next_floor_h,
                  float next_ceil_h, WallSegment *seg) {
  // Transform both wall vertices to view space.
  float vx0, vz0, vx1, vz1;
  camera_transform_vertex_xz(cam, fixed16_to_float(wall->w0->x),
                             fixed16_to_float(wall->w0->z), &vx0, &vz0);
  camera_transform_vertex_xz(cam, fixed16_to_float(wall->w1->x),
                             fixed16_to_float(wall->w1->z), &vx1, &vz1);

  if (wall_is_backfacing(vx0, vz0, vx1, vz1))
    return false;

  if (!frustum_test_segment(frustum, vx0, vz0, vx1, vz1))
    return false;

  // Near-plane clip. Track parametric t for texture coordinate adjustment.
  float t_clip0 = 0.0f;
  float t_clip1 = 1.0f;
  float cx0 = vx0, cz0 = vz0, cx1 = vx1, cz1 = vz1;

  float t_near = 0.0f;
  if (!frustum_clip_near(&cx0, &cz0, &cx1, &cz1, NEAR_PLANE_EPSILON, &t_near))
    return false;

  // Update parametric range if near-plane clipped a vertex.
  if (cz0 != vz0 || cx0 != vx0) {
    float orig_dx = vx1 - vx0;
    float orig_dz = vz1 - vz0;
    float orig_len_sq = orig_dx * orig_dx + orig_dz * orig_dz;
    if (orig_len_sq > 1e-12f) {
      t_clip0 = ((cx0 - vx0) * orig_dx + (cz0 - vz0) * orig_dz) / orig_len_sq;
    }
  }
  if (cz1 != vz1 || cx1 != vx1) {
    float orig_dx = vx1 - vx0;
    float orig_dz = vz1 - vz0;
    float orig_len_sq = orig_dx * orig_dx + orig_dz * orig_dz;
    if (orig_len_sq > 1e-12f) {
      t_clip1 = ((cx1 - vx0) * orig_dx + (cz1 - vz0) * orig_dz) / orig_len_sq;
    }
  }

  // Frustum clip (left/right planes).
  float t0_frust = 0.0f, t1_frust = 1.0f;
  if (!frustum_clip_segment(frustum, &cx0, &cz0, &cx1, &cz1, &t0_frust, &t1_frust))
    return false;

  // Adjust parametric range for frustum clipping.
  float range = t_clip1 - t_clip0;
  float base = t_clip0;
  t_clip0 = base + t0_frust * range;
  t_clip1 = base + t1_frust * range;

  // Project to screen X.
  float sx0 = camera_project_x(cam, cx0, cz0);
  float sx1 = camera_project_x(cam, cx1, cz1);

  // Ensure left-to-right screen order.
  if (sx0 > sx1) {
    float tmp;
    tmp = sx0;
    sx0 = sx1;
    sx1 = tmp;
    tmp = cx0;
    cx0 = cx1;
    cx1 = tmp;
    tmp = cz0;
    cz0 = cz1;
    cz1 = tmp;
    tmp = t_clip0;
    t_clip0 = t_clip1;
    t_clip1 = tmp;
  }

  // Quantize to integer screen columns.
  int32_t ix0 = (int32_t)ceilf(sx0);
  int32_t ix1 = (int32_t)floorf(sx1);

  // Clamp to screen bounds.
  if (ix0 < 0)
    ix0 = 0;
  if (ix1 >= cam->screen_width)
    ix1 = cam->screen_width - 1;

  if (ix0 > ix1)
    return false;

  // Fill the output segment.
  seg->src_wall = wall;
  seg->wall_index = wall_index;
  seg->wall_x0 = ix0;
  seg->wall_x1 = ix1;
  seg->z0 = cz0;
  seg->z1 = cz1;
  seg->vx0 = cx0;
  seg->vz0 = cz0;
  seg->vx1 = cx1;
  seg->vz1 = cz1;

  wall_compute_normal(cx0, cz0, cx1, cz1, &seg->normal_x, &seg->normal_z, &seg->normal_d);

  float texel_length = fixed16_to_float(wall->texel_length);
  wall_compute_texture_u(cx0, cz0, cx1, cz1, t_clip0, t_clip1, texel_length, ix0, ix1,
                         &seg->u_coord0, &seg->du_dx);

  wall_compute_depth_params(cx0, cz0, cx1, cz1, ix0, ix1, &seg->depth_slope,
                            &seg->depth_numerator);

  // Adjoin classification.
  seg->is_adjoin = (wall->next_sector != NULL);
  seg->has_dadjoin = (wall->dadjoin_sector != NULL);
  if (seg->is_adjoin) {
    seg->draw_flags = wall_classify_adjoin(floor_h, ceil_h, next_floor_h, next_ceil_h);
    // Deadjoin: next sector's opening height is zero or negative.
    // In Y-down space, ceiling is more negative than floor when open.
    // Only check the NEXT sector's own range — sectors may be connected
    // by portals without vertical overlap (multi-story, elevators), and
    // the rendering pipeline (S-Buffer, render window) handles occlusion.
    // For dadjoins, render_draw_sector overrides these based on both openings.
    bool opening_exists = (next_ceil_h < next_floor_h);
    seg->is_solid = !opening_exists;
  } else {
    seg->draw_flags = WDF_MIDDLE;
    seg->is_solid = true;
  }

  return true;
}

// Merge sort

static void wall_merge(WallSegment *src, WallSegment *dst, int32_t left, int32_t mid,
                       int32_t right) {
  int32_t i = left;
  int32_t j = mid;
  int32_t k = left;

  while (i < mid && j < right) {
    if (src[i].wall_x0 <= src[j].wall_x0) {
      dst[k++] = src[i++];
    } else {
      dst[k++] = src[j++];
    }
  }

  while (i < mid)
    dst[k++] = src[i++];
  while (j < right)
    dst[k++] = src[j++];
}

void wall_merge_sort(WallSegment *segments, int32_t count) {
  if (count <= 1)
    return;

  // Stack-based temporary buffer; falls back for large counts.
  // MAX_WALL_SEG is 768 — ~50 KB on the stack, acceptable.
  WallSegment temp[MAX_WALL_SEG];
  WallSegment *src = segments;
  WallSegment *dst = temp;

  if (count > MAX_WALL_SEG)
    return;

  for (int32_t width = 1; width < count; width *= 2) {
    for (int32_t left = 0; left < count; left += 2 * width) {
      int32_t mid = left + width;
      int32_t right = left + 2 * width;
      if (mid > count)
        mid = count;
      if (right > count)
        right = count;
      wall_merge(src, dst, left, mid, right);
    }

    WallSegment *swap = src;
    src = dst;
    dst = swap;
  }

  if (src != segments)
    memcpy(segments, src, (size_t)count * sizeof(WallSegment));
}
