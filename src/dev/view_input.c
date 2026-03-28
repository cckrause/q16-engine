// ===========================================================================
// View Input — SDL2 input handling for the wireframe viewer
// ===========================================================================

#include "view_input.h"

#include <SDL.h>
#include <math.h>

#include "render/render_limits.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MOVE_SPEED 12.0f
#define MOUSE_SENS 0.4f

// --- Lifecycle -------------------------------------------------------------

void view_input_init(ViewInput *vi, float cam_x, float cam_y, float cam_z) {
  vi->cam_x = cam_x;
  vi->cam_y = cam_y;
  vi->cam_z = cam_z;
  vi->yaw_deg = 0.0f;
  vi->pitch_deg = 0.0f;
  vi->show_minimap = true;
  vi->running = true;
}

// --- Per-frame update ------------------------------------------------------

static ViewInputActions process_events(ViewInput *vi) {
  ViewInputActions acts = {0};

  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
    case SDL_QUIT:
      vi->running = false;
      break;

    case SDL_KEYDOWN:
      if (ev.key.keysym.sym == SDLK_ESCAPE)
        vi->running = false;
      if (ev.key.keysym.sym == SDLK_c)
        acts.cycle_color_mode = true;
      if (ev.key.keysym.sym == SDLK_PAGEUP)
        acts.adjoin_depth_delta += 1;
      if (ev.key.keysym.sym == SDLK_PAGEDOWN)
        acts.adjoin_depth_delta -= 1;
      if (ev.key.keysym.sym == SDLK_m)
        vi->show_minimap = !vi->show_minimap;
      if (ev.key.keysym.sym == SDLK_l)
        acts.debug_trace = true;

      {
        uint32_t toggle = 0;
        if (ev.key.keysym.sym == SDLK_1)
          toggle = CULL_BACKFACE;
        if (ev.key.keysym.sym == SDLK_2)
          toggle = CULL_FRUSTUM;
        if (ev.key.keysym.sym == SDLK_3)
          toggle = CULL_SBUFFER;
        if (ev.key.keysym.sym == SDLK_4)
          toggle = CULL_DFS_MARKING;
        if (ev.key.keysym.sym == SDLK_5)
          toggle = CULL_PORTAL_BUDGET;
        if (ev.key.keysym.sym == SDLK_6)
          toggle = CULL_FRUSTUM_CLIP;
        if (ev.key.keysym.sym == SDLK_7)
          toggle = CULL_PORTAL_FRUSTUM;
        if (toggle)
          acts.cull_toggle |= toggle;
        if (ev.key.keysym.sym == SDLK_0)
          acts.cull_reset = true;
      }
      break;

    case SDL_MOUSEMOTION:
      vi->yaw_deg += (float)ev.motion.xrel * MOUSE_SENS;
      vi->pitch_deg -= (float)ev.motion.yrel * MOUSE_SENS;
      if (vi->pitch_deg > 60.0f)
        vi->pitch_deg = 60.0f;
      if (vi->pitch_deg < -60.0f)
        vi->pitch_deg = -60.0f;
      break;
    }
  }

  return acts;
}

static void apply_movement(ViewInput *vi, float dt) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  float speed = MOVE_SPEED * dt;

  float yaw_rad = vi->yaw_deg * (float)(M_PI / 180.0);
  float fwd_x = sinf(yaw_rad);
  float fwd_z = cosf(yaw_rad);
  float right_x = cosf(yaw_rad);
  float right_z = -sinf(yaw_rad);

  if (keys[SDL_SCANCODE_W]) {
    vi->cam_x += fwd_x * speed;
    vi->cam_z += fwd_z * speed;
  }
  if (keys[SDL_SCANCODE_S]) {
    vi->cam_x -= fwd_x * speed;
    vi->cam_z -= fwd_z * speed;
  }
  if (keys[SDL_SCANCODE_D]) {
    vi->cam_x += right_x * speed;
    vi->cam_z += right_z * speed;
  }
  if (keys[SDL_SCANCODE_A]) {
    vi->cam_x -= right_x * speed;
    vi->cam_z -= right_z * speed;
  }
  if (keys[SDL_SCANCODE_SPACE])
    vi->cam_y -= speed;
  if (keys[SDL_SCANCODE_LCTRL])
    vi->cam_y += speed;
}

ViewInputActions view_input_update(ViewInput *vi, float dt) {
  ViewInputActions acts = process_events(vi);
  apply_movement(vi, dt);
  return acts;
}
