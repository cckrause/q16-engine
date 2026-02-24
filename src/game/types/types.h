#ifndef Q16_TYPES_H
#define Q16_TYPES_H

#include "types/fixed16.h"
#include <stdint.h>

// --- Boolean ---------------------------------------------------------------
// 32-bit boolean for struct layout compatibility with the original engine.
// 0 = false, non-zero = true. Do not confuse with C99 _Bool.
typedef int32_t JBool;
#define JTRUE  1
#define JFALSE 0

// --- Angle -----------------------------------------------------------------
// 14-bit angle stored in a 32-bit integer.
// Full circle = 16384 (2^14). 1 unit ~= 0.022 degrees.
typedef int32_t Angle14;

#define ANGLE14_FULL_CIRCLE 16384
#define ANGLE14_HALF_CIRCLE 8192
#define ANGLE14_QUARTER     4096
#define ANGLE14_MASK        16383 // FULL_CIRCLE - 1, used to wrap angles

// --- Tick ------------------------------------------------------------------
// Game tick counter. ~145 ticks per second.
typedef uint32_t Tick;

#define TICKS_PER_SECOND 145
#define TASK_SLEEP       0xFFFFFFFFu
#define IDELAY_HOLD      0xFFFFFFFEu
#define IDELAY_TERMINATE 0xFFFFFFFDu
#define IDELAY_COMPLETE  0xFFFFFFFCu
#define TICKS(seconds)   ((Tick)((seconds) * TICKS_PER_SECOND))

// --- Sound identifiers -----------------------------------------------------
typedef int32_t SoundSourceId;
typedef int32_t SoundEffectId;
#define NULL_SOUND ((SoundSourceId) - 1)

// --- Key items -------------------------------------------------------------
typedef enum KeyItem {
  KEY_NONE = 0,
  KEY_RED = 1,
  KEY_YELLOW = 2,
  KEY_BLUE = 3,
} KeyItem;

// --- 2D vector (fixed-point, XZ plane) -------------------------------------
typedef struct Vec2Fixed {
  Fixed16 x;
  Fixed16 z;
} Vec2Fixed;

// --- 3D vector (fixed-point) -----------------------------------------------
typedef struct Vec3Fixed {
  Fixed16 x;
  Fixed16 y;
  Fixed16 z;
} Vec3Fixed;

#endif /* Q16_TYPES_H */
