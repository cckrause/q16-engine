#include "test_harness.h"

#include "render/camera.h"
#include <math.h>
#include <string.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_camera(void) {
  TEST_SUITE_BEGIN("camera");

  CameraState cam;
  memset(&cam, 0, sizeof(cam));

  // Projection setup at 320x200, 90-degree FOV, 4:3 aspect.
  camera_set_projection(&cam, 320, 200, 90.0f, 1.333f);
  TEST_CHECK("half_width = 160", NEAR(cam.half_width, 160.0f, 0.01f));
  TEST_CHECK("half_height = 100", NEAR(cam.half_height, 100.0f, 0.01f));
  TEST_CHECK("focal_length = 160 (90deg)", NEAR(cam.focal_length, 160.0f, 0.01f));
  TEST_CHECK("proj_offset_x = 160", NEAR(cam.proj_offset_x, 160.0f, 0.01f));
  TEST_CHECK("proj_offset_y = 100", NEAR(cam.proj_offset_y, 100.0f, 0.01f));

  // View transform: camera at origin, looking along +Z (yaw = 0).
  camera_compute_transform(&cam, 0.0f, 0.0f, 0.0f, 0, 0);
  TEST_CHECK("cos_yaw ~1 at yaw 0", NEAR(cam.cos_yaw, 1.0f, 1e-3f));
  TEST_CHECK("sin_yaw ~0 at yaw 0", NEAR(cam.sin_yaw, 0.0f, 1e-3f));
  TEST_CHECK("trans_x = 0", NEAR(cam.trans_x, 0.0f, 1e-3f));
  TEST_CHECK("trans_z = 0", NEAR(cam.trans_z, 0.0f, 1e-3f));
  TEST_CHECK("no pitch offset", NEAR(cam.pitch_offset, 0.0f, 1e-3f));

  // Vertex transform: point at (0, 0, 10) should stay at (0, 0, 10).
  {
    float vx, vz;
    camera_transform_vertex_xz(&cam, 0.0f, 10.0f, &vx, &vz);
    TEST_CHECK("fwd point vx ~0", NEAR(vx, 0.0f, 0.01f));
    TEST_CHECK("fwd point vz ~10", NEAR(vz, 10.0f, 0.01f));
  }

  // Project: point at (0, 0, 10) should project to screen center.
  {
    float sx, sy;
    camera_project(&cam, 0.0f, 0.0f, 10.0f, &sx, &sy);
    TEST_CHECK("center proj sx = 160", NEAR(sx, 160.0f, 0.1f));
    TEST_CHECK("center proj sy = 100", NEAR(sy, 100.0f, 0.1f));
  }

  // Project: point at (10, 0, 10) should be right of center.
  {
    float sx, sy;
    camera_project(&cam, 10.0f, 0.0f, 10.0f, &sx, &sy);
    TEST_CHECK("right point sx > 160", sx > 160.0f);
    TEST_CHECK("right point sy = 100", NEAR(sy, 100.0f, 0.1f));
  }

  // camera_project_x consistency.
  {
    float sx_full, sy;
    camera_project(&cam, 5.0f, 0.0f, 10.0f, &sx_full, &sy);
    float sx_x_only = camera_project_x(&cam, 5.0f, 10.0f);
    TEST_CHECK("project_x matches project", NEAR(sx_full, sx_x_only, 0.001f));
  }

  // Camera with offset position: world point (10, 0, 20), camera at (10, 0, 20)
  // should transform to (0, 0, 0).
  camera_compute_transform(&cam, 10.0f, 0.0f, 20.0f, 0, 0);
  {
    float vx, vz;
    camera_transform_vertex_xz(&cam, 10.0f, 20.0f, &vx, &vz);
    TEST_CHECK("same pos vx ~0", NEAR(vx, 0.0f, 0.01f));
    TEST_CHECK("same pos vz ~0", NEAR(vz, 0.0f, 0.01f));
  }

  TEST_SUITE_END();
}
