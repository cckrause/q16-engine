// ===========================================================================
// Frustum System
// ===========================================================================
#ifndef Q16_RENDER_FRUSTUM_H
#define Q16_RENDER_FRUSTUM_H

#include "render/render_limits.h"
#include <stdbool.h>
#include <stdint.h>

// Camera frustum and per-portal child frustums. Each frustum is a set of
// half-planes in 2D (XZ view-space) used for wall/object rejection.
// Portal frustums are pushed onto a stack during adjoin recursion.

typedef struct {
  float nx, nz; // inward-pointing normal in view-space XZ
  float d;      // signed distance from origin (dot(normal, point) >= d → inside)
} FrustumPlane2D;

typedef struct {
  FrustumPlane2D planes[MAX_FRUSTUM_PLANES];
  int32_t plane_count;
} Frustum;

typedef struct {
  Frustum stack[FRUSTUM_STACK_SIZE];
  int32_t depth; // current stack top (0 = empty)
} FrustumStack;

// Build the camera frustum from projection parameters.
// guard_band: multiplicative factor applied to left/right/top/bot planes.
// near_dist: near plane distance.
void frustum_build_camera(Frustum *out, float focal_length, float half_width,
                          float y_plane_top, float y_plane_bot, float guard_band,
                          float near_dist);

// Build a portal frustum from the clipped portal polygon edges.
// Each edge of the portal (in view-space XZ) defines a half-plane through the origin.
// verts: polygon vertices in view-space XZ, vert_count: number of vertices (3-8 typical).
void frustum_build_portal(Frustum *out, const float *verts_x, const float *verts_z,
                          int32_t vert_count);

// Test a 2D point against a frustum. Returns true if inside all planes.
bool frustum_test_point(const Frustum *f, float vx, float vz);

// Test a 2D line segment against a frustum. Returns true if any part is inside.
bool frustum_test_segment(const Frustum *f, float x0, float z0, float x1, float z1);

// Clip a 2D line segment to the frustum. Returns false if fully clipped.
// On success, (x0,z0)-(x1,z1) are updated to the clipped segment.
// t0_out/t1_out: parametric clip positions along the original segment [0,1].
bool frustum_clip_segment(const Frustum *f, float *x0, float *z0, float *x1, float *z1,
                          float *t0_out, float *t1_out);

// Near-plane clip: if one vertex of a wall is behind the camera, interpolate
// to find the intersection with z = near_dist.
// Returns false if both vertices are behind the camera (fully clipped).
bool frustum_clip_near(float *x0, float *z0, float *x1, float *z1, float near_dist,
                       float *t_out);

// --- Stack operations ------------------------------------------------------

void frustum_stack_init(FrustumStack *fs);
bool frustum_stack_push(FrustumStack *fs, const Frustum *f);
bool frustum_stack_pop(FrustumStack *fs);
const Frustum *frustum_stack_top(const FrustumStack *fs);

#endif /* Q16_RENDER_FRUSTUM_H */
