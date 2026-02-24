// ===========================================================================
// Wall
// ===========================================================================
#ifndef Q16_WORLD_WALL_H
#define Q16_WORLD_WALL_H

#include "types/forward.h"
#include "world/flags.h"

// Each sector boundary edge. Walls form a closed polygon wound clockwise
// when viewed from above.
struct Wall {
  // --- Geometry ---
  Vec2Fixed *w0;        // start vertex (into sector.vertices_ws)
  Vec2Fixed *w1;        // end vertex
  Vec2Fixed world_pos0; // copy of *w0 at parse time (for INF rotation)
  Vec2Fixed wall_dir;   // unit direction w0->w1
  Fixed16 length;       // euclidean length
  Fixed16 texel_length; // length * 8 (8:1 texel-to-DFU ratio)
  Angle14 angle;        // wall normal angle (Angle14 system)

  // --- Adjoin (Portal) ---
  Sector *next_sector; // sector on other side (NULL = solid)
  Wall *mirror_wall;   // corresponding wall in next_sector
  Sector *sector;      // the sector that owns this wall
  Sector
      *dadjoin_sector; // double-adjoin: second portal opening (Outlaws, NULL if unused)
  Wall *dmirror_wall;  // double-adjoin mirror wall (Outlaws, NULL if unused)

  // --- Textures ---
  Texture *mid_tex;
  Texture *top_tex;
  Texture *bot_tex;
  Texture *sign_tex; // overlay sign/switch texture
  Vec2Fixed mid_offset;
  Vec2Fixed top_offset;
  Vec2Fixed bot_offset;
  Vec2Fixed sign_offset;

  // --- Texel Heights (derived) ---
  Fixed16 top_texel_height; // height of top section in texels (world << 3)
  Fixed16 bot_texel_height;
  Fixed16 mid_texel_height;

  // --- Flags ---
  uint32_t flags1;     // WallFlag1
  uint32_t flags3;     // WallFlag3
  uint32_t draw_flags; // WallDrawFlag

  // --- Collision ---
  int32_t collision_frame; // dedup counter

  // --- Lighting ---
  Fixed16 wall_light; // per-wall light adjustment (added to sector ambient)

  // --- Automap ---
  JBool seen; // player has seen this wall
};

#endif /* Q16_WORLD_WALL_H */
