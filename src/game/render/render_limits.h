// ===========================================================================
// Renderer Configurable Limits
// ===========================================================================
#ifndef Q16_RENDER_LIMITS_H
#define Q16_RENDER_LIMITS_H

// Two limit profiles: Modern (large buffers, high depth) and Glide (fits
// in retro-era memory budget). Each constant has a _GLIDE variant.

// --- Adjoin (Portal) Limits ------------------------------------------------
#define MAX_ADJOIN_DEPTH       40
#define MAX_ADJOIN_DEPTH_GLIDE 40
#define MAX_ADJOIN_SEG         768
#define MAX_ADJOIN_SEG_GLIDE   384

// --- Wall Segment Limits ---------------------------------------------------
#define MAX_WALL_SEG       768
#define MAX_WALL_SEG_GLIDE 384

// --- Display List Limits ---------------------------------------------------
#define MAX_DISP_ITEMS       65536
#define MAX_DISP_ITEMS_GLIDE 5120

// --- Frustum Stack ---------------------------------------------------------
#define FRUSTUM_STACK_SIZE       256
#define FRUSTUM_STACK_SIZE_GLIDE 48

// Maximum clip planes per frustum (portal clipping).
#define MAX_FRUSTUM_PLANES 8

// --- S-Buffer Pool ---------------------------------------------------------
#define SEG_CLIP_POOL_SIZE       8192
#define SEG_CLIP_POOL_SIZE_GLIDE 2048

// --- Portal Limit ----------------------------------------------------------
#define MAX_PORTALS       4096
#define MAX_PORTALS_GLIDE 512

// --- Lighting --------------------------------------------------------------
#define MAX_LIGHT_LEVEL     31
#define LIGHT_ATTEN_SHIFT0  5 // depth >> 5
#define LIGHT_ATTEN_SHIFT1  3 // depth >> 3   (combined: depth * 3/32)
#define LIGHT_SOURCE_LEVELS 128

// --- Screen / Projection ---------------------------------------------------
#define NEAR_PLANE_EPSILON 0.001f

// --- Display List Part IDs -------------------------------------------------
#define PART_MID_WALL    0
#define PART_TOP_WALL    1
#define PART_BOT_WALL    2
#define PART_FLOOR       3
#define PART_CEILING     4
#define PART_FLOOR_CAP   5
#define PART_CEILING_CAP 6
#define PART_MID_SIGN    7
#define PART_TOP_SIGN    8
#define PART_BOT_SIGN    9

#endif /* Q16_RENDER_LIMITS_H */
