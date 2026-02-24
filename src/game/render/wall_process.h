// ===========================================================================
// Wall Processing
// ===========================================================================
#ifndef Q16_RENDER_WALL_PROCESS_H
#define Q16_RENDER_WALL_PROCESS_H

#include "render/camera.h"
#include "render/frustum.h"
#include "render/render_limits.h"
#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// Transforms wall vertices from world space to view space, applies backface
// and frustum culling, clips to the near plane, projects to screen X, and
// computes texture mapping parameters. The output WallSegment structs are
// the CPU-side representation consumed by the S-Buffer and display list.

// Per-wall result after culling, clipping, and projection.
typedef struct {
  Wall *src_wall;

  // Screen-space horizontal span.
  int32_t wall_x0;
  int32_t wall_x1;

  // View-space depth at each screen edge (for depth interpolation).
  float z0;
  float z1;

  // View-space XZ at each endpoint (after clipping).
  float vx0, vz0;
  float vx1, vz1;

  // Texture U mapping.
  float u_coord0; // texture U at wall_x0
  float du_dx;    // texture U step per screen pixel

  // Depth interpolation: z = numerator / (x - x_offset).
  float depth_slope;
  float depth_numerator;

  // Wall normal in view space (for S-Buffer depth comparisons).
  float normal_x;
  float normal_z;
  float normal_d; // dot(normal, wall_point)

  // Adjoin classification.
  uint32_t draw_flags; // WallDrawFlag bitmask (WDF_MIDDLE, WDF_TOP, WDF_BOT)
  bool is_adjoin;      // true if wall has next_sector
  bool is_solid;       // true if solid wall or deadjoin
  bool has_dadjoin;    // true if wall has dadjoin_sector (Outlaws double adjoin)

  // Index into the level wall array (for S-Buffer wall_id).
  int32_t wall_index;
} WallSegment;

// Process a single wall: transform, cull, clip, project.
// Returns true if the wall is visible and seg has been filled.
// cam: current camera state.
// frustum: active frustum (camera or portal).
// wall: source wall.
// wall_index: global index in the level wall array.
// floor_h, ceil_h: owning sector's heights (float, converted from Fixed16).
// next_floor_h, next_ceil_h: adjoin sector heights (ignored if not adjoin).
// seg: output segment (only valid if function returns true).
bool wall_process(const CameraState *cam, const Frustum *frustum, Wall *wall,
                  int32_t wall_index, float floor_h, float ceil_h, float next_floor_h,
                  float next_ceil_h, WallSegment *seg);

// Merge-sort an array of wall segments by wall_x0.
// segments: array to sort (in-place).
// count: number of valid segments.
void wall_merge_sort(WallSegment *segments, int32_t count);

#endif /* Q16_RENDER_WALL_PROCESS_H */
