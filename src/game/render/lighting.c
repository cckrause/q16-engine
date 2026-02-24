// ===========================================================================
// Lighting System
// ===========================================================================
// Computes per-column and per-vertex light levels for geometry submission.

#include "render/lighting.h"
#include <math.h>

float lighting_compute(const LightingState *ls, float sector_ambient, float depth,
                       float wall_light) {
  float ambient = ls->flat_lighting ? ls->flat_ambient : sector_ambient;

  if (ambient >= (float)MAX_LIGHT_LEVEL)
    return (float)MAX_LIGHT_LEVEL;

  float light = 0.0f;

  // Headlamp contribution via distance-indexed ramp LUT.
  if (ls->camera_light_source > 0.0f || ls->world_ambient < (float)MAX_LIGHT_LEVEL) {
    int32_t depth_idx = (int32_t)(depth / 2.0f);
    if (depth_idx < 0)
      depth_idx = 0;
    if (depth_idx > LIGHT_SOURCE_LEVELS - 1)
      depth_idx = LIGHT_SOURCE_LEVELS - 1;

    float ramp_val = (float)ls->light_source_ramp[depth_idx];
    float light_source = (float)MAX_LIGHT_LEVEL - (ramp_val + ls->world_ambient);
    if (light_source > 0.0f)
      light += light_source;
  }

  if (light < ambient)
    light = ambient;

  // Distance attenuation: depth * 3/32.
  float atten = depth * (3.0f / 32.0f);
  float scaled_ambient = ambient * 0.875f;
  light -= atten;
  if (light < scaled_ambient)
    light = scaled_ambient;

  light += wall_light;

  if (light < 0.0f)
    light = 0.0f;
  if (light > (float)MAX_LIGHT_LEVEL)
    light = (float)MAX_LIGHT_LEVEL;

  return light;
}

// 3DO vertex lighting

void lighting_shade_vertices(float sector_ambient, const float *nrm_vs,
                             int32_t vertex_count, float *out_intensity) {
  // Three axis-aligned directional lights along -X, -Y, -Z.
  for (int32_t v = 0; v < vertex_count; v++) {
    float nx = nrm_vs[v * 3 + 0];
    float ny = nrm_vs[v * 3 + 1];
    float nz = nrm_vs[v * 3 + 2];

    float dir_light = 0.0f;

    float ndot = -nx;
    if (ndot > 0.0f)
      dir_light += ndot * (float)MAX_LIGHT_LEVEL;

    ndot = -ny;
    if (ndot > 0.0f)
      dir_light += ndot * (float)MAX_LIGHT_LEVEL;

    ndot = -nz;
    if (ndot > 0.0f)
      dir_light += ndot * (float)MAX_LIGHT_LEVEL;

    float intensity = sector_ambient + dir_light;
    if (intensity < 0.0f)
      intensity = 0.0f;
    if (intensity > (float)MAX_LIGHT_LEVEL)
      intensity = (float)MAX_LIGHT_LEVEL;
    out_intensity[v] = intensity;
  }
}
