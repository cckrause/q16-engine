// ===========================================================================
// Frustum System
// ===========================================================================
// Camera and portal frustum construction, segment clipping.

#include "render/frustum.h"
#include <math.h>
#include <string.h>

// Camera frustum

void frustum_build_camera(Frustum *out, float focal_length, float half_width,
                          float y_plane_top, float y_plane_bot, float guard_band,
                          float near_dist) {
  out->plane_count = 0;

  /*
   * Left plane: the frustum edge runs from origin through (-half_width, focal_length).
   * Inward normal = (focal_length, half_width), normalized and tightened by guard_band.
   * Right plane is the mirror.
   */
  float left_len = sqrtf(focal_length * focal_length + half_width * half_width);
  if (left_len > 0.0f) {
    float inv = guard_band / left_len;
    out->planes[out->plane_count].nx = focal_length * inv;
    out->planes[out->plane_count].nz = half_width * inv;
    out->planes[out->plane_count].d = 0.0f;
    out->plane_count++;

    out->planes[out->plane_count].nx = -focal_length * inv;
    out->planes[out->plane_count].nz = half_width * inv;
    out->planes[out->plane_count].d = 0.0f;
    out->plane_count++;
  }

  out->planes[out->plane_count].nx = 0.0f;
  out->planes[out->plane_count].nz = 1.0f;
  out->planes[out->plane_count].d = near_dist;
  out->plane_count++;

  (void)y_plane_top;
  (void)y_plane_bot;
}

// Portal frustum

void frustum_build_portal(Frustum *out, const float *verts_x, const float *verts_z,
                          int32_t vert_count) {
  out->plane_count = 0;
  if (vert_count < 2)
    return;

  // Each frustum plane passes through the camera (origin) and one portal
  // vertex. The inward normal is the perpendicular of the origin-to-vertex
  // direction, oriented so the other portal vertices lie on the positive side.
  for (int32_t i = 0; i < vert_count && out->plane_count < MAX_FRUSTUM_PLANES; i++) {
    float nx = verts_z[i];
    float nz = -verts_x[i];

    // Pick the other vertex to determine the correct normal direction.
    int32_t j = (i == 0) ? vert_count - 1 : 0;
    float dot_other = verts_x[j] * nx + verts_z[j] * nz;
    if (dot_other < 0.0f) {
      nx = -nx;
      nz = -nz;
    }

    float len = sqrtf(nx * nx + nz * nz);
    if (len < 1e-8f)
      continue;

    float inv = 1.0f / len;
    out->planes[out->plane_count].nx = nx * inv;
    out->planes[out->plane_count].nz = nz * inv;
    out->planes[out->plane_count].d = 0.0f;
    out->plane_count++;
  }
}

// Rejection and clipping

bool frustum_test_point(const Frustum *f, float vx, float vz) {
  for (int32_t i = 0; i < f->plane_count; i++) {
    float dist = vx * f->planes[i].nx + vz * f->planes[i].nz;
    if (dist < f->planes[i].d)
      return false;
  }
  return true;
}

bool frustum_test_segment(const Frustum *f, float x0, float z0, float x1, float z1) {
  for (int32_t i = 0; i < f->plane_count; i++) {
    float d0 = x0 * f->planes[i].nx + z0 * f->planes[i].nz - f->planes[i].d;
    float d1 = x1 * f->planes[i].nx + z1 * f->planes[i].nz - f->planes[i].d;
    if (d0 < 0.0f && d1 < 0.0f)
      return false;
  }
  return true;
}

bool frustum_clip_segment(const Frustum *f, float *x0, float *z0, float *x1, float *z1,
                          float *t0_out, float *t1_out) {
  float t0 = 0.0f;
  float t1 = 1.0f;

  float dx = *x1 - *x0;
  float dz = *z1 - *z0;

  for (int32_t i = 0; i < f->plane_count; i++) {
    float d0 = (*x0) * f->planes[i].nx + (*z0) * f->planes[i].nz - f->planes[i].d;
    float dn = dx * f->planes[i].nx + dz * f->planes[i].nz;

    if (fabsf(dn) < 1e-10f) {
      if (d0 < 0.0f)
        return false;
      continue;
    }

    float t = -d0 / dn;

    if (dn < 0.0f) {
      if (t < t1)
        t1 = t;
    } else {
      if (t > t0)
        t0 = t;
    }

    if (t0 > t1)
      return false;
  }

  float ox0 = *x0, oz0 = *z0;
  *x0 = ox0 + dx * t0;
  *z0 = oz0 + dz * t0;
  *x1 = ox0 + dx * t1;
  *z1 = oz0 + dz * t1;

  if (t0_out)
    *t0_out = t0;
  if (t1_out)
    *t1_out = t1;
  return true;
}

// Near-plane clip

bool frustum_clip_near(float *x0, float *z0, float *x1, float *z1, float near_dist,
                       float *t_out) {
  bool behind0 = (*z0 < near_dist);
  bool behind1 = (*z1 < near_dist);

  if (behind0 && behind1)
    return false;

  if (!behind0 && !behind1) {
    if (t_out)
      *t_out = 0.0f;
    return true;
  }

  float dz = *z1 - *z0;
  if (fabsf(dz) < 1e-10f)
    return false;

  float t = (near_dist - *z0) / dz;
  float clip_x = *x0 + t * (*x1 - *x0);

  if (behind0) {
    *x0 = clip_x;
    *z0 = near_dist;
    if (t_out)
      *t_out = t;
  } else {
    *x1 = clip_x;
    *z1 = near_dist;
    if (t_out)
      *t_out = t;
  }
  return true;
}

// Stack operations

void frustum_stack_init(FrustumStack *fs) {
  fs->depth = 0;
}

bool frustum_stack_push(FrustumStack *fs, const Frustum *f) {
  if (fs->depth >= FRUSTUM_STACK_SIZE)
    return false;
  memcpy(&fs->stack[fs->depth], f, sizeof(Frustum));
  fs->depth++;
  return true;
}

bool frustum_stack_pop(FrustumStack *fs) {
  if (fs->depth <= 0)
    return false;
  fs->depth--;
  return true;
}

const Frustum *frustum_stack_top(const FrustumStack *fs) {
  if (fs->depth <= 0)
    return NULL;
  return &fs->stack[fs->depth - 1];
}
