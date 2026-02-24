// ===========================================================================
// LevelState
// ===========================================================================
#ifndef Q16_WORLD_LEVEL_H
#define Q16_WORLD_LEVEL_H

#include "types/forward.h"

// Central level data container. Holds all loaded geometry and metadata.
// Referenced globally as s_levelState.
#define MAX_LEVEL_PALETTES 8

struct LevelState {
  // --- Level Metadata (parsed from LEV/LVT header) ---
  char level_name[32];
  char palette_name[16]; // primary palette (LEV) or first palette (LVT)
  char music_name[16];

  // --- Multiple Palettes (LVT) ---
  char palette_names[MAX_LEVEL_PALETTES][16];
  int32_t palette_count;

  // --- Geometry Arrays ---
  Sector *sectors;
  int32_t sector_count;
  Wall *walls; // contiguous; sectors index into this
  int32_t wall_count;
  Vec2Fixed *vertices;
  int32_t vertex_count;

  // --- Level Metadata ---
  int32_t secret_count; // total secret sectors
  int32_t min_layer;    // lowest automap layer
  int32_t max_layer;    // highest automap layer

  // --- Parallax (Sky) ---
  Fixed16 parallax0;
  Fixed16 parallax1;

  // --- Special Sectors ---
  Sector *boss_sector;     // triggers boss events (NULL if none)
  Sector *mohc_sector;     // General Mohc final boss sector
  Sector *complete_sector; // triggers level completion

  // --- Textures ---
  Texture *textures; // level texture list
  int32_t texture_count;
  Texture *object_textures; // object texture list (sprites, 3DOs)
  int32_t object_texture_count;
};

#endif /* Q16_WORLD_LEVEL_H */
