// ===========================================================================
// Display List Construction
// ===========================================================================
#ifndef Q16_RENDER_DISPLAY_LIST_H
#define Q16_RENDER_DISPLAY_LIST_H

#include "render/render_limits.h"
#include "types/forward.h"
#include <stdbool.h>
#include <stdint.h>

// Packs wall, flat, and sign geometry into position+data arrays consumed by
// the HAL. Two separate lists: opaque (walls, flats) and transparent
// (signs, adjoin mid-textures). Each entry represents one quad.

// Position: wall endpoint XZ pairs in view space + explicit Y heights.
typedef struct {
  float v0x, v0z;
  float v1x, v1z;
  float y_bot, y_top; // world-space Y bounds for this wall part
} DisplayListPos;

// Packed data per entry (matches GPU uvec4 layout).
typedef struct {
  uint32_t flags_part; // partId [3:0], flags [31:4]
  uint32_t sector_id;
  uint32_t light_info;  // wallLight+32 [5:0], flip [6], portal [31:7]
  uint32_t wall_tex_id; // wallId [15:0], textureId [31:16]
} DisplayListData;

// Portal clip plane (one vec4 per portal plane).
typedef struct {
  float x, y, z, w;
} DisplayListPlane;

// Display list entry combining position and packed data.
typedef struct {
  DisplayListPos pos;
  DisplayListData data;
} DisplayListEntry;

typedef struct {
  // Opaque pass entries.
  DisplayListEntry *opaque;
  int32_t opaque_count;

  // Transparent pass entries.
  DisplayListEntry *transparent;
  int32_t transparent_count;

  // Portal clip planes.
  DisplayListPlane *planes;
  int32_t plane_count;

  int32_t max_entries;
  int32_t max_planes;
} DisplayList;

// Allocate display list buffers. Returns false on allocation failure.
bool display_list_init(DisplayList *dl, int32_t max_entries, int32_t max_planes);

void display_list_destroy(DisplayList *dl);

// Reset for a new frame.
void display_list_reset(DisplayList *dl);

// Flags field packing helpers.

// Pack the flags_part field from part ID and per-entry flags.
static inline uint32_t display_list_pack_flags(int32_t part_id, bool stretch_top,
                                               bool stretch, bool fullbright,
                                               bool opaque_flag, bool sky_adj, bool sky,
                                               int32_t next_sector_id) {
  uint32_t v = (uint32_t)(part_id & 0x0F);
  if (stretch_top)
    v |= (1u << 4);
  if (stretch)
    v |= (1u << 5);
  if (fullbright)
    v |= (1u << 6);
  if (opaque_flag)
    v |= (1u << 7);
  if (sky_adj)
    v |= (1u << 8);
  if (sky)
    v |= (1u << 9);
  v |= ((uint32_t)(next_sector_id & 0x3FFFFF)) << 10;
  return v;
}

// Pack the light_info field.
static inline uint32_t display_list_pack_light(int32_t wall_light, bool flip,
                                               int32_t portal_offset,
                                               int32_t portal_count) {
  uint32_t v = (uint32_t)((wall_light + 32) & 0x3F);
  if (flip)
    v |= (1u << 6);
  uint32_t portal =
      ((uint32_t)(portal_offset & 0xFFFF) << 4) | ((uint32_t)(portal_count & 0xF));
  v |= (portal << 7);
  return v;
}

// Pack the wall_tex_id field.
static inline uint32_t display_list_pack_wall_tex(int32_t wall_id, int32_t tex_id) {
  return ((uint32_t)(wall_id & 0xFFFF)) | (((uint32_t)(tex_id & 0xFFFF)) << 16);
}

// Add an entry to the opaque list. Returns false if full.
bool display_list_add_opaque(DisplayList *dl, const DisplayListEntry *entry);

// Add an entry to the transparent list. Returns false if full.
bool display_list_add_transparent(DisplayList *dl, const DisplayListEntry *entry);

// Add a portal clip plane. Returns the index, or -1 if full.
int32_t display_list_add_plane(DisplayList *dl, float x, float y, float z, float w);

#endif /* Q16_RENDER_DISPLAY_LIST_H */
