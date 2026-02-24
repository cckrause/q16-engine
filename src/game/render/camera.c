// ===========================================================================
// Camera System
// ===========================================================================
// View transform and projection for the portal renderer.

#include "render/camera.h"
#include "math/trig_table.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Projection setup

void camera_set_projection(CameraState *cam, int32_t width, int32_t height,
                           float fov_degrees, float aspect_ratio) {
  cam->screen_width = width;
  cam->screen_height = height;
  cam->half_width = (float)width * 0.5f;
  cam->half_height = (float)height * 0.5f;

  cam->focal_length = cam->half_width;

  // 320x200 VGA had 1.2:1 non-square pixels; aspect_ratio corrects for this.
  cam->focal_len_aspect = cam->focal_length * (200.0f / 320.0f) * aspect_ratio;

  if (fov_degrees > 0.0f && fov_degrees < 180.0f && fov_degrees != 90.0f) {
    float fov_scale = 1.0f / tanf((float)(fov_degrees * 0.5 * M_PI / 180.0));
    cam->focal_length *= fov_scale;
    cam->focal_len_aspect *= fov_scale;
  }

  cam->proj_offset_x = cam->half_width;
  cam->proj_offset_y_base = cam->half_height;
  cam->proj_offset_y = cam->half_height;
  cam->pitch_offset = 0.0f;

  cam->y_plane_top = -(cam->half_height / cam->half_width);
  cam->y_plane_bot = (cam->half_height / cam->half_width);
}

// View transform

void camera_compute_transform(CameraState *cam, float eye_x, float eye_y, float eye_z,
                              Angle14 yaw, Angle14 pitch) {
  cam->pos_x = eye_x;
  cam->pos_y = eye_y;
  cam->pos_z = eye_z;
  cam->yaw = yaw;
  cam->pitch = pitch;

  // Trig from the fixed-point LUT, converted to float.
  // Negated yaw: the view transform rotates the world opposite to camera yaw.
  Fixed16 sin_f, cos_f;
  Angle14 neg_yaw = (-yaw) & ANGLE14_MASK;
  sin_cos_fixed(neg_yaw, &sin_f, &cos_f);

  cam->cos_yaw = fixed16_to_float(cos_f);
  cam->sin_yaw = fixed16_to_float(sin_f);
  cam->neg_sin_yaw = -cam->sin_yaw;

  cam->view_mtx[0] = cam->cos_yaw;
  cam->view_mtx[1] = cam->neg_sin_yaw;
  cam->view_mtx[2] = cam->sin_yaw;
  cam->view_mtx[3] = cam->cos_yaw;

  float ox = -eye_x;
  float oz = -eye_z;
  cam->trans_x = ox * cam->cos_yaw + oz * cam->sin_yaw;
  cam->trans_z = oz * cam->cos_yaw + ox * cam->neg_sin_yaw;

  // Y-shear: uses half_width (not focal_length) because the shear is a
  // screen-space offset that must scale with the actual screen dimension.
  if (pitch != 0) {
    float pitch_rad = (float)pitch * (float)(2.0 * M_PI / (double)ANGLE14_FULL_CIRCLE);
    cam->pitch_offset = tanf(pitch_rad) * cam->half_width;
  } else {
    cam->pitch_offset = 0.0f;
  }
  cam->proj_offset_y = cam->proj_offset_y_base + cam->pitch_offset;

  if (cam->half_width > 0.0f) {
    cam->y_plane_bot = (cam->half_height - cam->pitch_offset) / cam->half_width;
    cam->y_plane_top = -(cam->half_height + cam->pitch_offset) / cam->half_width;
  }
}
