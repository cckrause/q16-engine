#include "test_harness.h"

#include "render/frustum.h"
#include <math.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_frustum(void) {
  TEST_SUITE_BEGIN("frustum");

  // Build a camera frustum for a 320-wide screen with focal_length = 160.
  Frustum f;
  frustum_build_camera(&f, 160.0f, 160.0f, -1.0f, 1.0f, 0.98f, 0.001f);
  TEST_CHECK("camera frustum has 3 planes", f.plane_count == 3);

  // Point on the Z axis is inside (between left/right planes, ahead of near).
  TEST_CHECK("origin inside", frustum_test_point(&f, 0.0f, 1.0f));

  // Point behind camera (z < near) is outside.
  TEST_CHECK("behind camera", !frustum_test_point(&f, 0.0f, -1.0f));

  // Point far ahead and centered is inside.
  TEST_CHECK("far ahead", frustum_test_point(&f, 0.0f, 100.0f));

  // Point far to the left is outside.
  TEST_CHECK("far left outside", !frustum_test_point(&f, -1000.0f, 1.0f));

  // Segment test: segment crossing the frustum is visible.
  TEST_CHECK("crossing seg visible",
             frustum_test_segment(&f, -200.0f, 10.0f, 200.0f, 10.0f));

  // Segment fully behind camera is invisible.
  TEST_CHECK("behind seg invisible",
             !frustum_test_segment(&f, -10.0f, -5.0f, 10.0f, -5.0f));

  // Frustum clip: clip a wide segment to the frustum.
  {
    float x0 = -1000.0f, z0 = 10.0f, x1 = 1000.0f, z1 = 10.0f;
    float t0, t1;
    bool ok = frustum_clip_segment(&f, &x0, &z0, &x1, &z1, &t0, &t1);
    TEST_CHECK("wide seg clips", ok);
    TEST_CHECK("clipped x0 < x1", x0 < x1);
    TEST_CHECK("clipped z0 = 10", NEAR(z0, 10.0f, 0.1f));
  }

  // Near-plane clip: one vertex behind.
  {
    float x0 = 0.0f, z0 = -1.0f, x1 = 0.0f, z1 = 10.0f;
    float t;
    bool ok = frustum_clip_near(&x0, &z0, &x1, &z1, 0.001f, &t);
    TEST_CHECK("near clip succeeds", ok);
    TEST_CHECK("near clipped z0 >= 0.001", z0 >= 0.001f);
  }

  // Near-plane clip: both behind.
  {
    float x0 = 0.0f, z0 = -5.0f, x1 = 0.0f, z1 = -1.0f;
    bool ok = frustum_clip_near(&x0, &z0, &x1, &z1, 0.001f, NULL);
    TEST_CHECK("both behind = false", !ok);
  }

  // Near-plane clip: both in front.
  {
    float x0 = -5.0f, z0 = 5.0f, x1 = 5.0f, z1 = 10.0f;
    float t;
    bool ok = frustum_clip_near(&x0, &z0, &x1, &z1, 0.001f, &t);
    TEST_CHECK("both in front = true", ok);
    TEST_CHECK("no clip t = 0", NEAR(t, 0.0f, 1e-5f));
  }

  // Stack operations.
  FrustumStack fs;
  frustum_stack_init(&fs);
  TEST_CHECK("empty stack top = NULL", frustum_stack_top(&fs) == NULL);
  TEST_CHECK("empty pop fails", !frustum_stack_pop(&fs));

  bool pushed = frustum_stack_push(&fs, &f);
  TEST_CHECK("push succeeds", pushed);
  TEST_CHECK("top not NULL", frustum_stack_top(&fs) != NULL);
  TEST_CHECK("top plane_count matches", frustum_stack_top(&fs)->plane_count == 3);

  bool popped = frustum_stack_pop(&fs);
  TEST_CHECK("pop succeeds", popped);
  TEST_CHECK("stack empty again", frustum_stack_top(&fs) == NULL);

  TEST_SUITE_END();
}
