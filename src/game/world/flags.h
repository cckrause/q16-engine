// ===========================================================================
// Engine Format Switch
// ===========================================================================
#ifndef Q16_WORLD_FLAGS_H
#define Q16_WORLD_FLAGS_H

#include <stdint.h>

// Selects flag bit positions for DF (Dark Forces / LEV) or OL (Outlaws / LVT).
// One build targets one format. Toggle here to switch.

#define ENGINE_FORMAT_DF 0
#define ENGINE_FORMAT_OL 1
#define ENGINE_FORMAT    ENGINE_FORMAT_OL

// ===========================================================================
// Sector Flags Word 1
// ===========================================================================
// Bit positions differ between DF and OL for PIT, EXT_ADJ, EXT_FLOOR_ADJ.
// Runtime-only flags (PLAYER, RENDERED) use fixed high bits in both formats
// since they are never read from level files.

#if ENGINE_FORMAT == ENGINE_FORMAT_OL

typedef enum SectorFlag1 {
  SEC_FLAG1_EXTERIOR = 0x01,      // OL: SKY (bit 0)
  SEC_FLAG1_PIT = 0x02,           // OL: PIT (bit 1)
  SEC_FLAG1_EXT_ADJ = 0x04,       // OL: SKY_ADJOIN (bit 2)
  SEC_FLAG1_EXT_FLOOR_ADJ = 0x08, // OL: PIT_ADJOIN (bit 3)

  SEC_FLAG1_SLOPED_FLOOR = 0x40000,
  SEC_FLAG1_SLOPED_CEILING = 0x80000,

  // Runtime-only flags (not from file, same positions as DF)
  SEC_FLAG1_PLAYER = 0x10000,
  SEC_FLAG1_SECRET = 0x20000,
  SEC_FLAG1_RENDERED = 0x100000,
} SectorFlag1;

#else // ENGINE_FORMAT_DF

typedef enum SectorFlag1 {
  SEC_FLAG1_EXTERIOR = 0x00001,
  SEC_FLAG1_DOOR = 0x00002,
  SEC_FLAG1_MAG_SEAL = 0x00004,
  SEC_FLAG1_EXT_ADJ = 0x00008,
  SEC_FLAG1_ICE_FLOOR = 0x00010,
  SEC_FLAG1_SNOW_FLOOR = 0x00020,
  SEC_FLAG1_EXP_WALL = 0x00040,
  SEC_FLAG1_PIT = 0x00080,
  SEC_FLAG1_EXT_FLOOR_ADJ = 0x00100,
  SEC_FLAG1_CRUSHING = 0x00200,
  SEC_FLAG1_NOWALL_DRAW = 0x00400,
  SEC_FLAG1_LOW_DAMAGE = 0x00800,
  SEC_FLAG1_HIGH_DAMAGE = 0x01000,
  SEC_FLAG1_NO_SMART_OBJ = 0x02000,
  SEC_FLAG1_SMART_OBJ = 0x04000,
  SEC_FLAG1_SAFE_SECTOR = 0x08000,
  SEC_FLAG1_PLAYER = 0x10000,
  SEC_FLAG1_SECRET = 0x20000,
  SEC_FLAG1_SLOPED_FLOOR = 0x40000,
  SEC_FLAG1_SLOPED_CEILING = 0x80000,
  SEC_FLAG1_RENDERED = 0x100000,
  SEC_FLAG1_SUBSECTOR = 0x200000,
} SectorFlag1;

#endif

// ===========================================================================
// Sector Flags Word 2
// ===========================================================================
// Reserved ? no gameplay-critical bits defined.

// ===========================================================================
// Sector Flags Word 3
// ===========================================================================
// Reserved.

// ===========================================================================
// Wall Flags Word 1
// ===========================================================================
// Bit 0 (ADJ_MID_TEX / MIDTEX) is the same in both formats.
// OL adds NOPASS (bit 11), FORCEPASS (bit 12), PROJECTILE_OK (bit 15).

#if ENGINE_FORMAT == ENGINE_FORMAT_OL

typedef enum WallFlag1 {
  WF1_ADJ_MID_TEX = 0x0001, // OL: MIDTEX (bit 0)
  WF1_ILLUM_SIGN = 0x0002,  // bit 1
  WF1_FLIP_HORIZ = 0x0004,  // bit 2
  WF1_CHANGE_WALL_LIGHT = 0x0008,
  WF1_TEX_ANCHORED = 0x0010,
  WF1_WALL_MORPHS = 0x0020,
  WF1_SCROLL_TOP_TEX = 0x0040,
  WF1_SCROLL_MID_TEX = 0x0080,
  WF1_SCROLL_BOT_TEX = 0x0100,
  WF1_SCROLL_SIGN_TEX = 0x0200,
  WF1_NOPASS = 0x0800,    // OL-only: blocks passage despite adjoin
  WF1_FORCEPASS = 0x1000, // OL-only: forces passage ignoring height check
  WF1_SIGN_ANCHORED = 0x2000,
  WF1_PROJECTILE_OK = 0x8000, // OL-only: projectiles pass through
} WallFlag1;

#else // ENGINE_FORMAT_DF

typedef enum WallFlag1 {
  WF1_ADJ_MID_TEX = 0x0001,
  WF1_ILLUM_SIGN = 0x0002,
  WF1_FLIP_HORIZ = 0x0004,
  WF1_CHANGE_WALL_LIGHT = 0x0008,
  WF1_TEX_ANCHORED = 0x0010,
  WF1_WALL_MORPHS = 0x0020,
  WF1_SCROLL_TOP_TEX = 0x0040,
  WF1_SCROLL_MID_TEX = 0x0080,
  WF1_SCROLL_BOT_TEX = 0x0100,
  WF1_SCROLL_SIGN_TEX = 0x0200,
  WF1_HIDE_ON_MAP = 0x0400,
  WF1_SHOW_NORMAL_ON_MAP = 0x0800,
  WF1_SIGN_ANCHORED = 0x1000,
  WF1_DAMAGE_WALL = 0x2000,
  WF1_SHOW_AS_LEDGE_ON_MAP = 0x4000,
  WF1_SHOW_AS_DOOR_ON_MAP = 0x8000,
} WallFlag1;

#endif

// ===========================================================================
// Wall Flags Word 3
// ===========================================================================
// Bits 0-3 have the same semantics in both formats.
// OL renames bit 2: ENEMIES_ONLY_CANNOT_WALK (DF: PLAYER_WALK_ONLY).

typedef enum WallFlag3 {
  WF3_ALWAYS_WALK = 0x0001,
  WF3_SOLID_WALL = 0x0002,
#if ENGINE_FORMAT == ENGINE_FORMAT_OL
  WF3_ENEMIES_CANNOT_WALK = 0x0004,
#else
  WF3_PLAYER_WALK_ONLY = 0x0004,
#endif
  WF3_CANNOT_FIRE_THROUGH = 0x0008,
} WallFlag3;

// ===========================================================================
// Entity Flags
// ===========================================================================
typedef enum EntityFlag {
  ETFLAG_NONE = 0x0000,
  ETFLAG_PLAYER = 0x0001,
  ETFLAG_AI_ACTOR = 0x0002,
  ETFLAG_PROJECTILE = 0x0004,
  ETFLAG_PICKUP = 0x0008,
  ETFLAG_LANDMINE = 0x0010,
  ETFLAG_LANDMINE_WPN = 0x0020,
  ETFLAG_CORPSE = 0x0040,
  ETFLAG_KEEP_CORPSE = 0x0080,
  ETFLAG_FLYING = 0x0100,
  ETFLAG_REMOTE = 0x0200,
  ETFLAG_SMART_OBJ = 0x0400,
  ETFLAG_GENERAL_MOHC = 0x0800,
  ETFLAG_HAS_GRAVITY = 0x1000,
  ETFLAG_SCENERY = 0x2000,
  ETFLAG_CAN_DISABLE = 0x4000,
} EntityFlag;

// ===========================================================================
// Object Flags
// ===========================================================================
typedef enum ObjFlag {
  OBJ_FLAG_NEEDS_TRANSFORM = 0x0001,
  OBJ_FLAG_MOVABLE = 0x0002,
  OBJ_FLAG_FULLBRIGHT = 0x0004,
  OBJ_FLAG_AIM = 0x0008,
  OBJ_FLAG_BOSS = 0x0010,
} ObjFlag;

// ===========================================================================
// Object Type
// ===========================================================================
typedef enum ObjType {
  OBJ_TYPE_SPIRIT = 0,
  OBJ_TYPE_SPRITE = 1,
  OBJ_TYPE_FRAME = 2,
  OBJ_TYPE_3D = 4,
} ObjType;

// ===========================================================================
// Sector Dirty Flags
// ===========================================================================
typedef enum SectorDirtyFlag {
  SDF_HEIGHTS = 0x01,
  SDF_AMBIENT = 0x02,
  SDF_VERTICES = 0x04,
  SDF_WALL_OFFSETS = 0x08,
  SDF_WALL_SHAPE = 0x10,
  SDF_FLAT_OFFSETS = 0x20,
  SDF_CHANGE_OBJ = 0x40,
  SDF_ALL = 0x7F,
} SectorDirtyFlag;

// ===========================================================================
// Wall Draw Flags
// ===========================================================================
typedef enum WallDrawFlag {
  WDF_MIDDLE = 0x01,
  WDF_TOP = 0x02,
  WDF_BOT = 0x04,
} WallDrawFlag;

// ===========================================================================
// Texture opacity flags
// ===========================================================================
#define TEX_OPACITY_TRANS  0x08
#define TEX_OPACITY_OPAQUE 0x00

// ===========================================================================
// Constants used by world structs
// ===========================================================================
#define SEC_SKY_HEIGHT FIXED(100)

#endif /* Q16_WORLD_FLAGS_H */
