// ===========================================================================
// q16 Engine — Win9x Entry Point & Glide 2.x Level Renderer
// ===========================================================================
// Loads a level from a LAB archive, runs the CPU render pipeline,
// and draws the display list output as colored Glide triangles.
//
// Controls:
//   WASD        — move in XZ plane (relative to camera yaw)
//   Mouse       — look (yaw + pitch)
//   Space       — fly up
//   Ctrl        — fly down
//   Escape      — quit

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "glide2/glide.h"
#include "glide_loader.h"

#include "archive/archive.h"
#include "io/stream.h"
#include "math/core_math.h"
#include "memory/game_memory.h"
#include "render/render_limits.h"
#include "render/render_sector.h"
#include "util/strings.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "debug/debug_vis.h"
#include "world/sector.h"
#include "world/wall.h"

#define SCREEN_W       640
#define SCREEN_H       480
#define DEFAULT_FOV    90.0f
#define DEFAULT_ASPECT (4.0f / 3.0f)
#define MOVE_SPEED     20.0f
#define MOUSE_SENS     0.4f
#define Z_NEAR         1.0f
#define Z_FAR          5000.0f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *DEFAULT_LEVEL_FILE = "house2.lvt";

static bool s_running = true;
static bool s_depth_test_overlay = false;
static bool s_depth_buffer_enabled = true;
static RenderState *s_render_state = NULL;

// --- WndProc ---------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_CLOSE:
  case WM_DESTROY:
    s_running = false;
    PostQuitMessage(0);
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) {
      s_running = false;
      PostQuitMessage(0);
    }
    if (wParam == VK_F1)
      s_depth_test_overlay = !s_depth_test_overlay;
    if (wParam == VK_F2) {
      s_depth_buffer_enabled = !s_depth_buffer_enabled;
      if (s_depth_buffer_enabled)
        gl_grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
      else
        gl_grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
    }
    if (wParam == VK_F3 && s_render_state)
      s_render_state->cull_mask ^= CULL_SBUFFER;
    if (wParam == VK_F4 && s_render_state)
      s_render_state->cull_mask ^= CULL_DFS_MARKING;
    if (wParam == VK_F5 && s_render_state)
      s_render_state->cull_mask ^= CULL_PORTAL_FRUSTUM;
    if (wParam == VK_F6 && s_render_state) {
      if (s_render_state->max_adjoin_depth > 0)
        s_render_state->max_adjoin_depth = 0;
      else
        s_render_state->max_adjoin_depth = MAX_ADJOIN_DEPTH_GLIDE;
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

// --- Depth buffer diagnostic -----------------------------------------------

static void draw_depth_test_quads(void) {
  GrVertex far_quad[4];
  GrVertex near_quad[4];
  memset(far_quad, 0, sizeof(far_quad));
  memset(near_quad, 0, sizeof(near_quad));

  float far_z = 100.0f;
  float far_coords[4][2] = {{50, 50}, {270, 50}, {270, 270}, {50, 270}};
  for (int32_t i = 0; i < 4; i++) {
    far_quad[i].x = far_coords[i][0];
    far_quad[i].y = far_coords[i][1];
    far_quad[i].oow = 1.0f / far_z;
    far_quad[i].r = 50.0f;
    far_quad[i].g = 50.0f;
    far_quad[i].b = 255.0f;
    far_quad[i].a = 255.0f;
  }

  float near_z = 10.0f;
  float near_coords[4][2] = {{110, 110}, {210, 110}, {210, 210}, {110, 210}};
  for (int32_t i = 0; i < 4; i++) {
    near_quad[i].x = near_coords[i][0];
    near_quad[i].y = near_coords[i][1];
    near_quad[i].oow = 1.0f / near_z;
    near_quad[i].r = 255.0f;
    near_quad[i].g = 50.0f;
    near_quad[i].b = 50.0f;
    near_quad[i].a = 255.0f;
  }

  // Near (red) first, then far (blue).
  // W-buffer with GR_CMP_LESS: smaller oow = farther (larger w = rejected).
  gl_grDrawTriangle(&near_quad[0], &near_quad[1], &near_quad[2]);
  gl_grDrawTriangle(&near_quad[0], &near_quad[2], &near_quad[3]);
  gl_grDrawTriangle(&far_quad[0], &far_quad[1], &far_quad[2]);
  gl_grDrawTriangle(&far_quad[0], &far_quad[2], &far_quad[3]);
}

// --- Glide display list draw -----------------------------------------------

static void draw_display_list(const DisplayList *dl, const CameraState *cam) {
  for (int32_t pass = 0; pass < 1; pass++) {
    const DisplayListEntry *entries = (pass == 0) ? dl->opaque : dl->transparent;
    int32_t count = (pass == 0) ? dl->opaque_count : dl->transparent_count;

    for (int32_t i = 0; i < count; i++) {
      const DisplayListEntry *e = &entries[i];
      int32_t part_id = (int32_t)(e->data.flags_part & 0x0F);

      float y_bot = e->pos.y_bot - cam->pos_y;
      float y_top = e->pos.y_top - cam->pos_y;
      if (fabsf(y_top - y_bot) < 0.001f)
        continue;

      float v0x = e->pos.v0x;
      float v0z = e->pos.v0z;
      float v1x = e->pos.v1x;
      float v1z = e->pos.v1z;

      if (v0z < Z_NEAR && v1z < Z_NEAR)
        continue;

      float cz0 = v0z < Z_NEAR ? Z_NEAR : v0z;
      float cz1 = v1z < Z_NEAR ? Z_NEAR : v1z;

      float cr, cg, cb;
      debug_sector_color((int32_t)e->data.sector_id, &cr, &cg, &cb);
      float bright = debug_part_brightness(part_id) * 200.0f;
      cr *= bright;
      cg *= bright;
      cb *= bright;

      float corners_vx[4] = {v0x, v1x, v1x, v0x};
      float corners_vy[4] = {y_bot, y_bot, y_top, y_top};
      float corners_vz[4] = {cz0, cz1, cz1, cz0};

      GrVertex v[4];
      memset(v, 0, sizeof(v));

      for (int32_t j = 0; j < 4; j++) {
        float inv_z = 1.0f / corners_vz[j];
        v[j].x = corners_vx[j] * cam->focal_length * inv_z + cam->proj_offset_x;
        v[j].y = corners_vy[j] * cam->focal_len_aspect * inv_z + cam->proj_offset_y;
        v[j].oow = inv_z;
        v[j].r = cr;
        v[j].g = cg;
        v[j].b = cb;
        v[j].a = 255.0f;
      }

      gl_grDrawTriangle(&v[0], &v[1], &v[2]);
      gl_grDrawTriangle(&v[0], &v[2], &v[3]);
    }
  }
}

// --- Entry point -----------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
                   int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nCmdShow;

  const char *archive_path = (__argc > 1) ? __argv[1] : DEFAULT_LEVEL_FILE;
  const char *level_name = (__argc > 2) ? __argv[2] : NULL;

  bool standalone = (level_name == NULL) || str_ends_with_nocase(archive_path, ".lev") ||
                    str_ends_with_nocase(archive_path, ".lvt");

  game_memory_init();

  LevelState state;
  memset(&state, 0, sizeof(state));
  bool ok = false;

  if (standalone) {
    StreamReader sr = stream_from_file(archive_path);
    if (!sr.read) {
      MessageBox(NULL, "Failed to open level file", "q16 Engine", MB_OK | MB_ICONERROR);
      game_memory_shutdown();
      return 1;
    }
    ok = level_load_geometry(&sr, &state);
    stream_close(&sr);
  } else {
    Archive *ar = archive_open(archive_path);
    if (!ar) {
      MessageBox(NULL, "Failed to open archive", "q16 Engine", MB_OK | MB_ICONERROR);
      game_memory_shutdown();
      return 1;
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "%s.LEV", level_name);
    if (!archive_file_exists(ar, filename)) {
      snprintf(filename, sizeof(filename), "%s.LVT", level_name);
      if (!archive_file_exists(ar, filename)) {
        MessageBox(NULL, "Level not found in archive", "q16 Engine",
                   MB_OK | MB_ICONERROR);
        archive_close(ar);
        game_memory_shutdown();
        return 1;
      }
    }

    if (!archive_open_file(ar, filename)) {
      archive_close(ar);
      game_memory_shutdown();
      return 1;
    }

    StreamReader sr = stream_from_archive(ar);
    ok = level_load_geometry(&sr, &state);
    archive_close_file(ar);
    archive_close(ar);
  }

  if (!ok) {
    MessageBox(NULL, "Failed to parse level", "q16 Engine", MB_OK | MB_ICONERROR);
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  // --- Glide init ----------------------------------------------------------

  if (glide_load() != 0) {
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  WNDCLASS wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = "GlideQ16Engine";
  RegisterClass(&wc);

  RECT wr_size = {0, 0, SCREEN_W, SCREEN_H};
  AdjustWindowRect(&wr_size, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd =
      CreateWindow("GlideQ16Engine", "q16 Engine", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                   CW_USEDEFAULT, CW_USEDEFAULT, wr_size.right - wr_size.left,
                   wr_size.bottom - wr_size.top, NULL, NULL, hInstance, NULL);
  ShowCursor(TRUE);

  gl_grGlideInit();
  gl_grSstSelect(0);

  FxBool glide_ok = gl_grSstWinOpen((FxU32)hwnd, GR_RESOLUTION_640x480, GR_REFRESH_60Hz,
                                    GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1);
  if (!glide_ok) {
    MessageBox(NULL,
               "Failed to initialise Glide!\n"
               "Make sure a Voodoo card (or nGlide wrapper) is present.",
               "q16 Engine", MB_OK | MB_ICONERROR);
    DestroyWindow(hwnd);
    glide_unload();
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  gl_grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                    GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
  gl_grAlphaBlendFunction(GR_BLEND_ONE, GR_BLEND_ZERO, GR_BLEND_ONE, GR_BLEND_ZERO);
  gl_grDepthBufferMode(GR_DEPTHBUFFER_WBUFFER);
  gl_grDepthBufferFunction(GR_CMP_LESS);
  gl_grDepthMask(FXTRUE);
  gl_grCullMode(GR_CULL_DISABLE);

  // --- Render state init ---------------------------------------------------

  RenderState rs;
  s_render_state = &rs;
  if (!render_state_init(&rs, SCREEN_W, SCREEN_H, MAX_ADJOIN_DEPTH_GLIDE)) {
    MessageBox(NULL, "render_state_init failed", "q16 Engine", MB_OK | MB_ICONERROR);
    gl_grGlideShutdown();
    ShowCursor(TRUE);
    DestroyWindow(hwnd);
    glide_unload();
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  camera_set_projection(&rs.camera, SCREEN_W, SCREEN_H, DEFAULT_FOV, DEFAULT_ASPECT);

  float cam_x = 0.0f, cam_y = 0.0f, cam_z = 0.0f;
  float yaw_deg = 0.0f, pitch_deg = 0.0f;

  if (state.sector_count > 0) {
    Sector *s0 = &state.sectors[0];
    cam_x = fixed16_to_float(s0->bounds_min.x + s0->bounds_max.x) * 0.5f;
    cam_z = fixed16_to_float(s0->bounds_min.z + s0->bounds_max.z) * 0.5f;
    float fh = fixed16_to_float(s0->floor_height);
    float ch = fixed16_to_float(s0->ceiling_height);
    cam_y = fh - (fh - ch) * 0.7f;
  }

  RECT wr;
  GetWindowRect(hwnd, &wr);
  int32_t center_x = (wr.left + wr.right) / 2;
  int32_t center_y = (wr.top + wr.bottom) / 2;
  SetCursorPos(center_x, center_y);

  LARGE_INTEGER perf_freq, last_time;
  QueryPerformanceFrequency(&perf_freq);
  QueryPerformanceCounter(&last_time);

  // --- Main loop -----------------------------------------------------------

  MSG msg;

  while (s_running) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        s_running = false;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!s_running)
      break;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - last_time.QuadPart) / (float)perf_freq.QuadPart;
    last_time = now;
    if (dt > 0.1f)
      dt = 0.1f;

    // --- Mouse look ---
    POINT cursor;
    GetCursorPos(&cursor);
    int32_t mouse_dx = cursor.x - center_x;
    int32_t mouse_dy = cursor.y - center_y;
    SetCursorPos(center_x, center_y);

    yaw_deg += (float)mouse_dx * MOUSE_SENS;
    pitch_deg -= (float)mouse_dy * MOUSE_SENS;
    if (pitch_deg > 60.0f)
      pitch_deg = 60.0f;
    if (pitch_deg < -60.0f)
      pitch_deg = -60.0f;

    // --- Keyboard movement ---
    float speed = MOVE_SPEED * dt;
    float yaw_rad = yaw_deg * (float)(M_PI / 180.0);
    float fwd_x = sinf(yaw_rad);
    float fwd_z = cosf(yaw_rad);
    float right_x = cosf(yaw_rad);
    float right_z = -sinf(yaw_rad);

    if (GetAsyncKeyState('W') & 0x8000) {
      cam_x += fwd_x * speed;
      cam_z += fwd_z * speed;
    }
    if (GetAsyncKeyState('S') & 0x8000) {
      cam_x -= fwd_x * speed;
      cam_z -= fwd_z * speed;
    }
    if (GetAsyncKeyState('D') & 0x8000) {
      cam_x += right_x * speed;
      cam_z += right_z * speed;
    }
    if (GetAsyncKeyState('A') & 0x8000) {
      cam_x -= right_x * speed;
      cam_z -= right_z * speed;
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)
      cam_y -= speed;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
      cam_y += speed;

    // --- CPU render pipeline ---
    Sector *cam_sector = sector_find_at(&state, float_to_fixed16(cam_x),
                                        float_to_fixed16(cam_y), float_to_fixed16(cam_z));
    Angle14 yaw_a = degrees_to_angle14(yaw_deg);
    Angle14 pitch_a = degrees_to_angle14(pitch_deg);

    render_draw_frame(&rs, cam_sector, cam_x, cam_y, cam_z, yaw_a, pitch_a);

    // --- Debug HUD (window title) ---
    {
      char title[256];
      snprintf(title, sizeof(title), "q16 | W:%s SB:%s DFS:%s PF:%s Adj:%d  op:%d tr:%d",
               s_depth_buffer_enabled ? "ON" : "off",
               (rs.cull_mask & CULL_SBUFFER) ? "ON" : "off",
               (rs.cull_mask & CULL_DFS_MARKING) ? "ON" : "off",
               (rs.cull_mask & CULL_PORTAL_FRUSTUM) ? "ON" : "off", rs.max_adjoin_depth,
               rs.display_list.opaque_count, rs.display_list.transparent_count);
      SetWindowText(hwnd, title);
    }

    // --- Glide draw ---
    gl_grBufferClear(0x00102030, 0, GR_WDEPTHVALUE_FARTHEST);
    if (s_depth_test_overlay)
      draw_depth_test_quads();
    else
      draw_display_list(&rs.display_list, &rs.camera);
    gl_grBufferSwap(1);
  }

  // --- Cleanup -------------------------------------------------------------

  s_render_state = NULL;
  render_state_destroy(&rs);
  gl_grGlideShutdown();
  ShowCursor(TRUE);
  DestroyWindow(hwnd);
  glide_unload();
  game_level_clear();
  game_memory_shutdown();

  return 0;
}
