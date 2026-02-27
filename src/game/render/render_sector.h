// ===========================================================================
// Sector Traversal Orchestrator
// ===========================================================================
#ifndef Q16_RENDER_SECTOR_H
#define Q16_RENDER_SECTOR_H

#include "render/adjoin.h"
#include "render/camera.h"
#include "render/depth.h"
#include "render/display_list.h"
#include "render/flat.h"
#include "render/frustum.h"
#include "render/lighting.h"
#include "render/object_sort.h"
#include "render/sbuffer.h"
#include "render/wall_process.h"
#include "render/window.h"
#include "types/forward.h"
#include <stdint.h>

// Top-level per-frame entry point that drives the entire CPU-side rendering
// pipeline. Walks the sector graph via portal recursion, processes walls,
// fills the S-Buffer, builds the display list, and sorts objects.

// Aggregate render state shared across all recursive calls.
typedef struct {
  CameraState camera;
  FrustumStack frustum_stack;
  SBuffer sbuffer;
  DepthBuffer depth;
  RenderWindow window;
  FlatState flat;
  LightingState lighting;
  DisplayList display_list;

  // Per-frame draw counter for double-draw prevention.
  uint32_t draw_frame;

  // Current portal recursion depth (0 at top level).
  int32_t adjoin_depth;

  // Runtime-configurable maximum portal depth (default MAX_ADJOIN_DEPTH).
  int32_t max_adjoin_depth;

  // Global portal traversal budget per frame. Counts total portal recursions;
  // traversal stops when exhausted. Matches the reference GPU renderer pattern
  // (s_maxPortals = 4096) instead of per-sector visit limits.
  int32_t portals_traversed;
  int32_t max_portals;

  // Per-frame visited-sector tracking (optional, allocated by caller).
  bool *visited_sectors;
  int32_t visited_capacity;

  // Single-frame debug trace: when true, log portal traversal to stderr.
  bool debug_trace;

  // Debug: bitmask of active culling stages (default CULL_ALL).
  // Toggle individual bits to bypass stages for debugging.
  uint32_t cull_mask;

  // Debug: per-stage cull counters (reset each frame).
  int32_t cull_count_backface;
  int32_t cull_count_frustum;
  int32_t cull_count_sbuffer;
  int32_t cull_count_dfs;
  int32_t cull_count_budget;
} RenderState;

// Initialize all subsystems. Screen dimensions and limits must be known.
// Returns false on any allocation failure.
bool render_state_init(RenderState *rs, int32_t screen_width, int32_t screen_height,
                       int32_t max_adjoin_depth);

// Free all subsystem memory.
void render_state_destroy(RenderState *rs);

// Reset all per-frame state (S-Buffer, depth, window, display list, flat).
// Call once at the start of each frame before render_draw_frame.
void render_state_reset(RenderState *rs);

// Render a complete frame starting from the player's sector.
// Populates the display list with all visible geometry.
void render_draw_frame(RenderState *rs, Sector *player_sector, float eye_x, float eye_y,
                       float eye_z, Angle14 yaw, Angle14 pitch);

// Recursive sector draw. Exposed for testing; normally called by
// render_draw_frame internally.
void render_draw_sector(RenderState *rs, Sector *sector, const Frustum *frustum);

#endif /* Q16_RENDER_SECTOR_H */
