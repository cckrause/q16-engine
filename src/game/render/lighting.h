// ===========================================================================
// Lighting System
// ===========================================================================
#ifndef Q16_RENDER_LIGHTING_H
#define Q16_RENDER_LIGHTING_H

#include "render/render_limits.h"
#include <stdbool.h>
#include <stdint.h>

// Combines sector ambient, distance attenuation, camera light source, and
// per-wall offsets into a single light level [0, MAX_LIGHT_LEVEL].
// GPU submission uses the float light level; the 8-bit colormap lookup
// is a HAL concern.

typedef struct {
  float world_ambient;          // global ambient override (night vision)
  float camera_light_source;    // headlamp intensity (0 = off)
  bool  flat_lighting;          // true = all sectors use flat_ambient
  float flat_ambient;           // forced ambient when flat_lighting is true
  uint8_t light_source_ramp[LIGHT_SOURCE_LEVELS]; // camera light falloff LUT
} LightingState;

// Compute a light level for a wall or flat column.
// sector_ambient: sector's ambient level [0, MAX_LIGHT_LEVEL]
// depth: view-space distance at this column
// wall_light: per-wall light offset (can be negative)
// Returns a float light level clamped to [0, MAX_LIGHT_LEVEL].
float lighting_compute(const LightingState *ls, float sector_ambient,
                       float depth, float wall_light);

// Determine if a sector should be drawn fullbright (ambient >= MAX_LIGHT_LEVEL).
static inline bool lighting_is_fullbright(float sector_ambient) {
  return sector_ambient >= (float)MAX_LIGHT_LEVEL;
}

// Per-vertex directional lighting for 3DO models.
// nrm_vs: vertex normals in view space (array of [nx,ny,nz] triples).
// out_intensity: output light levels [0, MAX_LIGHT_LEVEL] per vertex.
// sector_ambient: base ambient for the model's sector.
void lighting_shade_vertices(float sector_ambient,
                             const float *nrm_vs, int32_t vertex_count,
                             float *out_intensity);

#endif /* Q16_RENDER_LIGHTING_H */
