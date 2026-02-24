#include "test_harness.h"

#include "render/lighting.h"
#include <math.h>
#include <string.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_lighting(void) {
  TEST_SUITE_BEGIN("lighting");

  LightingState ls;
  memset(&ls, 0, sizeof(ls));
  ls.world_ambient = 0.0f;
  ls.camera_light_source = 0.0f;
  ls.flat_lighting = false;
  ls.flat_ambient = 0.0f;
  memset(ls.light_source_ramp, 0, sizeof(ls.light_source_ramp));

  // Fullbright detection.
  TEST_CHECK("fullbright at 31", lighting_is_fullbright(31.0f));
  TEST_CHECK("not fullbright at 15", !lighting_is_fullbright(15.0f));
  TEST_CHECK("fullbright at 32", lighting_is_fullbright(32.0f));

  // Fullbright sector: always returns MAX_LIGHT_LEVEL.
  float bright = lighting_compute(&ls, 31.0f, 50.0f, 0.0f);
  TEST_CHECK("fullbright sector = 31", NEAR(bright, 31.0f, 0.01f));

  // Dark sector, no camera light, close depth: light = ambient.
  float dark = lighting_compute(&ls, 10.0f, 0.0f, 0.0f);
  TEST_CHECK("dark close >= 10", dark >= 10.0f);

  // Distance attenuation reduces light.
  float near_light = lighting_compute(&ls, 15.0f, 10.0f, 0.0f);
  float far_light  = lighting_compute(&ls, 15.0f, 100.0f, 0.0f);
  TEST_CHECK("farther = dimmer", far_light <= near_light);

  // Per-wall light offset.
  float boosted = lighting_compute(&ls, 10.0f, 10.0f, 5.0f);
  float base    = lighting_compute(&ls, 10.0f, 10.0f, 0.0f);
  TEST_CHECK("wall light adds", boosted > base);

  // Negative wall light.
  float dimmed = lighting_compute(&ls, 10.0f, 10.0f, -20.0f);
  TEST_CHECK("neg wall light clamps >= 0", dimmed >= 0.0f);

  // Clamping to [0, MAX_LIGHT_LEVEL].
  float over = lighting_compute(&ls, 25.0f, 0.0f, 20.0f);
  TEST_CHECK("clamped <= 31", over <= 31.0f);

  // Flat lighting override.
  ls.flat_lighting = true;
  ls.flat_ambient = 20.0f;
  float flat = lighting_compute(&ls, 5.0f, 0.0f, 0.0f);
  TEST_CHECK("flat override >= 20", flat >= 20.0f);
  ls.flat_lighting = false;

  // 3DO vertex lighting.
  {
    // Normal pointing toward -X light: should add brightness.
    float normals[3] = { -1.0f, 0.0f, 0.0f };
    float intensity;
    lighting_shade_vertices(10.0f, normals, 1, &intensity);
    TEST_CHECK("vertex light > ambient", intensity > 10.0f);
    TEST_CHECK("vertex light <= 31", intensity <= 31.0f);
  }

  {
    // Normal pointing away from all lights: only ambient.
    float normals[3] = { 1.0f, 1.0f, 1.0f };
    float intensity;
    lighting_shade_vertices(10.0f, normals, 1, &intensity);
    TEST_CHECK("away from lights = ambient", NEAR(intensity, 10.0f, 0.01f));
  }

  TEST_SUITE_END();
}
