// ===========================================================================
// Camera System
// ===========================================================================
#ifndef Q16_RENDER_CAMERA_H
#define Q16_RENDER_CAMERA_H

#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// Computes the view transform and projection parameters each frame.
// The camera is derived from the player eye position plus head-bob offsets.
// Yaw rotates in the XZ plane; pitch is a Y-shear (no vertical rotation).

typedef struct {
  // --- World position -------------------------------------------------------
  float pos_x; // world X
  float pos_y; // world Y (eye height, negative = up)
  float pos_z; // world Z

  // --- Orientation ----------------------------------------------------------
  Angle14 yaw;
  Angle14 pitch;

  // --- Derived trig (from -yaw) ---------------------------------------------
  float cos_yaw;
  float sin_yaw;
  float neg_sin_yaw;

  // --- View-space translation (camera pos rotated by -yaw) ------------------
  float trans_x;
  float trans_z;

  // --- 2x2 yaw rotation matrix (XZ only, row-major) ------------------------
  //  | cos_yaw   neg_sin_yaw |
  //  | sin_yaw   cos_yaw     |
  float view_mtx[4];

  // --- Projection -----------------------------------------------------------
  int32_t screen_width;
  int32_t screen_height;
  float half_width;
  float half_height;
  float focal_length;       // horizontal focal length (FOV-based, invariant to width)
  float focal_len_aspect;   // vertical focal length (aspect-corrected)
  float proj_offset_x;      // projection center X (== half_width)
  float proj_offset_y;      // projection center Y (shifted by pitch Y-shear)
  float proj_offset_y_base; // projection center Y before pitch

  // --- Pitch Y-shear -------------------------------------------------------
  float pitch_offset; // tan(pitch) * half_width (screen-space vertical shift)
  float y_plane_top;  // top clipping plane slope
  float y_plane_bot;  // bottom clipping plane slope

  // --- Sector reference -----------------------------------------------------
  // The sector the camera is currently in (set externally by caller).
  Sector *sector;
} CameraState;

// Set the screen resolution and compute projection constants.
// FOV is in degrees (0 < fov < 180). Pass 90 for the default.
// aspect_ratio is display width / display height (e.g. 1.333 for 4:3).
void camera_set_projection(CameraState *cam, int32_t width, int32_t height,
                           float fov_degrees, float aspect_ratio);

// Compute the view transform for a given eye position and orientation.
// Updates cos/sin, trans, view_mtx, and pitch Y-shear.
void camera_compute_transform(CameraState *cam, float eye_x, float eye_y, float eye_z,
                              Angle14 yaw, Angle14 pitch);

// Transform a world-space XZ point to view space using the current camera.
static inline void camera_transform_vertex_xz(const CameraState *cam, float wx, float wz,
                                              float *vx, float *vz) {
  *vx = wx * cam->cos_yaw + wz * cam->sin_yaw + cam->trans_x;
  *vz = wx * cam->neg_sin_yaw + wz * cam->cos_yaw + cam->trans_z;
}

// Transform a world-space XYZ point to view space.
static inline void camera_transform_vertex(const CameraState *cam, float wx, float wy,
                                           float wz, float *vx, float *vy, float *vz) {
  *vx = wx * cam->cos_yaw + wz * cam->sin_yaw + cam->trans_x;
  *vy = wy - cam->pos_y;
  *vz = wx * cam->neg_sin_yaw + wz * cam->cos_yaw + cam->trans_z;
}

// Project a view-space point to screen coordinates.
static inline void camera_project(const CameraState *cam, float vx, float vy, float vz,
                                  float *sx, float *sy) {
  float inv_z = 1.0f / vz;
  *sx = vx * cam->focal_length * inv_z + cam->proj_offset_x;
  *sy = vy * cam->focal_len_aspect * inv_z + cam->proj_offset_y;
}

// Project only the X component.
static inline float camera_project_x(const CameraState *cam, float vx, float vz) {
  return vx * cam->focal_length / vz + cam->proj_offset_x;
}

#endif /* Q16_RENDER_CAMERA_H */
