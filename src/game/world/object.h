// ===========================================================================
// SecObject
// ===========================================================================
#ifndef Q16_WORLD_OBJECT_H
#define Q16_WORLD_OBJECT_H

#include "types/forward.h"
#include "world/flags.h"

// Every entity in the world: player, enemies, sprites, 3D models,
// projectiles, pickups. The visual data is a tagged union selected by `type`.
struct SecObject {
  // --- Position ---
  Vec3Fixed pos_ws; // world-space position (Y negative = up)
  Angle14 yaw;      // rotation around Y axis
  Angle14 pitch;    // rotation around X axis
  Angle14 roll;     // rotation around Z axis

  // --- Collision Dimensions ---
  Fixed16 world_width;  // collision half-width (XZ radius), -1 = uninitialized
  Fixed16 world_height; // collision height (Y extent), -1 = uninitialized

  // --- Type & Visual Data ---
  ObjType type;
  union {
    JediWax *wax;     // OBJ_TYPE_SPRITE
    WaxFrame *fme;    // OBJ_TYPE_FRAME
    JediModel *model; // OBJ_TYPE_3D
    void *ptr;        // OBJ_TYPE_SPIRIT (NULL)
  };

  // --- Flags ---
  uint32_t entity_flags; // EntityFlag bitmask
  uint32_t flags;        // ObjFlag bitmask

  // --- Sector Assignment ---
  Sector *sector; // sector this object resides in
  int32_t index;  // index into sector.object_list[]

  // --- Self-Pointer ---
  SecObject *self; // always == this

  // --- Animation State ---
  int32_t frame; // current animation frame index
  int32_t anim;  // current animation/state index

  // --- Logic Chain ---
  Allocator *logic;                  // allocator of Logic* items
  ProjectileLogic *projectile_logic; // non-NULL if this is a projectile

  // --- Transform ---
  Fixed16 transform[9]; // 3x3 rotation matrix

  // --- Serialization ---
  int32_t serialize_index;
};

#endif /* Q16_WORLD_OBJECT_H */
