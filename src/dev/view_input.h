// ===========================================================================
// View Input — SDL2 input handling for the wireframe viewer
// ===========================================================================
#ifndef Q16_DEV_VIEW_INPUT_H
#define Q16_DEV_VIEW_INPUT_H

#include <stdbool.h>
#include <stdint.h>

// --- Camera and toggle state -----------------------------------------------

typedef struct {
  float cam_x, cam_y, cam_z;
  float yaw_deg, pitch_deg;
  bool show_minimap;
  bool running;
} ViewInput;

// One-shot actions produced by a single frame's input. The main loop
// applies these to RenderState / GlBackend so the input module stays
// decoupled from renderer internals.
typedef struct {
  bool cycle_color_mode;
  bool debug_trace;
  int32_t adjoin_depth_delta;
  uint32_t cull_toggle;
  bool cull_reset;
} ViewInputActions;

// --- Lifecycle -------------------------------------------------------------

void view_input_init(ViewInput *vi, float cam_x, float cam_y, float cam_z);

// Process all pending SDL events and continuous key state for one frame.
// Updates camera position/orientation in *vi and returns one-shot actions.
ViewInputActions view_input_update(ViewInput *vi, float dt);

#endif /* Q16_DEV_VIEW_INPUT_H */
