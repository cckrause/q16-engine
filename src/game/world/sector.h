// ===========================================================================
// SlopeInfo — Sloped floor/ceiling definition (Outlaws extension)
// ===========================================================================
#ifndef Q16_WORLD_SECTOR_H
#define Q16_WORLD_SECTOR_H

#include "types/forward.h"
#include "world/flags.h"

// Active when sector_idx >= 0, wall_idx >= 0, and angle != 0.
// angle is fixed-point: 4096 = 90 degrees.
typedef struct {
  int32_t sector_idx;
  int32_t wall_idx;
  int32_t angle;
} SlopeInfo;

// ===========================================================================
// Sector
// ===========================================================================
// The central spatial container. Every point in the world belongs to exactly
// one sector. Sectors are convex polygons connected by portal walls (adjoins).
struct Sector {
  // --- Identity ---
  int32_t id;
  char name[32]; // INF addressing name (e.g. "complete", "boss", "mohc")
  Sector *self;  // always == this

  // --- Geometry: Heights ---
  Fixed16 floor_height; // floor altitude (positive = down in Y-axis)
  Fixed16 ceiling_height;
  Fixed16 sec_height; // second height offset from floor
                      //   < 0: water surface at floor + sec_height
                      //   > 0: raised false floor
                      //   = 0: unused

  // --- Geometry: Collision Heights (derived) ---
  Fixed16 col_floor_height;    // effective floor (PIT -> +SEC_SKY_HEIGHT)
  Fixed16 col_ceil_height;     // effective ceiling (EXTERIOR -> -SEC_SKY_HEIGHT)
  Fixed16 col_sec_height;      // effective second-height floor
  Fixed16 col_sec_ceil_height; // effective second-height ceiling

  // --- Vertices ---
  Vec2Fixed *vertices_ws; // world-space vertex positions (XZ plane)
  Vec2Fixed *vertices_vs; // view-space (computed during rendering)
  int32_t vertex_count;

  // --- Walls ---
  Wall *walls; // contiguous array [0..wall_count-1]
  int32_t wall_count;
  int32_t start_wall;    // global index of walls[0] in level wall array
  int32_t draw_wall_cnt; // walls drawn this frame

  // --- Textures ---
  Texture *floor_tex;
  Texture *ceil_tex;
  Vec2Fixed floor_offset; // floor texture UV offset
  Vec2Fixed ceil_offset;  // ceiling texture UV offset

  // --- Objects ---
  SecObject **object_list; // sparse array — NULLs mark free slots
  int32_t object_count;    // number of non-NULL entries
  int32_t object_capacity; // allocated length (grows by 5)

  // --- Flags ---
  uint32_t flags1; // SectorFlag1
  uint32_t flags2;
  uint32_t flags3;

  // --- Lighting ---
  Fixed16 ambient; // 0 = dark, FIXED(31) = max brightness

  // --- Bounds (AABB) ---
  Vec2Fixed bounds_min;
  Vec2Fixed bounds_max;

  // --- Rendering ---
  uint32_t prev_draw_frame;
  uint32_t prev_draw_frame2;
  uint32_t dirty_flags; // SectorDirtyFlag bitmask

  // --- INF ---
  Allocator *inf_link; // linked list of InfLink items (NULL if no INF)

  // --- Collision ---
  int32_t collision_frame; // dedup counter

  // --- Navigation ---
  int32_t layer;      // automap layer assignment
  int32_t search_key; // flood-fill traversal marker

  // --- Slopes (Outlaws) ---
  SlopeInfo slope_floor;
  SlopeInfo slope_ceiling;

  // --- Misc ---
  Sector *col_min_sector; // sector with lowest floor found during collision
};

#endif /* Q16_WORLD_SECTOR_H */
