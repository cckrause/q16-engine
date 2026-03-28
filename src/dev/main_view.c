// ===========================================================================
// q16_view — SDL2 + OpenGL 3.3 Wireframe Level Viewer
// ===========================================================================
// Loads a level from a LAB archive, runs the CPU render pipeline,
// and draws the display list output as colored wireframe.
//
// Controls:
//   WASD      — move in XZ plane (relative to camera yaw)
//   Mouse     — look (yaw + pitch)
//   Space     — fly up
//   Ctrl      — fly down
//   C         — cycle color mode (part / sector / depth)
//   PgUp/PgDn — adjust max portal depth
//   M         — toggle minimap
//   L         — one-frame portal trace
//   1-7       — toggle culling stages (backface/frustum/sbuffer/dfs/budget/fclip/portal)
//   0         — reset all culling stages to ON
//   Escape    — quit
//
// Build (Mac):
//   cmake -B build && cmake --build build --target q16_view
// Run:
//   ./build/q16_view [path/to/archive.LAB] [LEVEL.LVT]

#include <SDL.h>
#include <glad/glad.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gl_backend.h"
#include "view_input.h"

#include "archive/archive.h"
#include "debug/debug_log.h"
#include "io/stream.h"
#include "math/core_math.h"
#include "memory/game_memory.h"
#include "render/render_sector.h"
#include "util/strings.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "world/sector.h"
#include "world/wall.h"

#define WINDOW_W       960
#define WINDOW_H       600
#define DEFAULT_FOV    90.0f
#define DEFAULT_ASPECT (4.0f / 3.0f)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *DEFAULT_ARCHIVE = "mock/ol/OLGEO.LAB";
static const char *DEFAULT_LEVEL = "TOWN";

int main(int argc, char *argv[]) {
  const char *archive_path = (argc > 1) ? argv[1] : DEFAULT_ARCHIVE;
  const char *level_name = (argc > 2) ? argv[2] : DEFAULT_LEVEL;

  bool standalone =
      str_ends_with_nocase(archive_path, ".lev") || str_ends_with_nocase(archive_path, ".lvt");

  game_memory_init();
  debug_log_init(NULL);
  LOG_INFO("view", "INIT archive=%s level=%s", archive_path, level_name);

  LevelState state;
  memset(&state, 0, sizeof(state));
  bool ok = false;

  if (standalone) {
    StreamReader sr = stream_from_file(archive_path);
    if (!sr.read) {
      LOG_ERROR("view", "failed to open '%s'", archive_path);
      debug_log_shutdown();
      game_memory_shutdown();
      return 1;
    }
    ok = level_load_geometry(&sr, &state);
    stream_close(&sr);
  } else {
    Archive *ar = archive_open(archive_path);
    if (!ar) {
      LOG_ERROR("view", "failed to open archive '%s'", archive_path);
      debug_log_shutdown();
      game_memory_shutdown();
      return 1;
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "%s.LEV", level_name);
    if (!archive_file_exists(ar, filename)) {
      snprintf(filename, sizeof(filename), "%s.LVT", level_name);
      if (!archive_file_exists(ar, filename)) {
        LOG_ERROR("view", "neither '%s.LEV' nor '%s.LVT' found in archive",
               level_name, level_name);
        archive_close(ar);
        debug_log_shutdown();
        game_memory_shutdown();
        return 1;
      }
    }

    if (!archive_open_file(ar, filename)) {
      LOG_ERROR("view", "failed to open '%s' in archive", filename);
      archive_close(ar);
      debug_log_shutdown();
      game_memory_shutdown();
      return 1;
    }

    StreamReader sr = stream_from_archive(ar);
    ok = level_load_geometry(&sr, &state);
    archive_close_file(ar);
    archive_close(ar);
  }

  if (!ok) {
    LOG_ERROR("view", "failed to parse '%s'", archive_path);
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  LOG_INFO("view", "LEVEL_OK %s — %d sectors, %d walls, %d vertices",
         state.level_name, state.sector_count, state.wall_count, state.vertex_count);

  // SDL + OpenGL init.
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG_ERROR("view", "SDL_Init: %s", SDL_GetError());
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow(
      "q16 viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

  if (!window) {
    LOG_ERROR("view", "SDL_CreateWindow: %s", SDL_GetError());
    SDL_Quit();
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
  if (!gl_ctx) {
    LOG_ERROR("view", "SDL_GL_CreateContext: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    LOG_ERROR("view", "gladLoadGLLoader failed");
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  LOG_INFO("view", "OpenGL %s, %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  SDL_GL_SetSwapInterval(1);

  // Init render state.
  RenderState rs;
  if (!render_state_init(&rs, WINDOW_W, WINDOW_H, MAX_ADJOIN_DEPTH)) {
    LOG_ERROR("view", "render_state_init failed");
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  camera_set_projection(&rs.camera, WINDOW_W, WINDOW_H, DEFAULT_FOV, DEFAULT_ASPECT);

  // Allocate visited-sector tracking for minimap visualization.
  bool *visited_buf = (bool *)calloc((size_t)state.sector_count, sizeof(bool));
  rs.visited_sectors = visited_buf;
  rs.visited_capacity = state.sector_count;

  GlBackend gl;
  if (!gl_backend_init(&gl, MAX_DISP_ITEMS)) {
    LOG_ERROR("view", "gl_backend_init failed");
    render_state_destroy(&rs);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    debug_log_shutdown();
    game_memory_shutdown();
    return 1;
  }

  // Camera starts at sector 0's centre, 70% up from the floor.
  float init_x = 0.0f, init_y = 0.0f, init_z = 0.0f;
  if (state.sector_count > 0) {
    Sector *s0 = &state.sectors[0];
    init_x = fixed16_to_float(s0->bounds_min.x + s0->bounds_max.x) * 0.5f;
    init_z = fixed16_to_float(s0->bounds_min.z + s0->bounds_max.z) * 0.5f;
    float fh = fixed16_to_float(s0->floor_height);
    float ch = fixed16_to_float(s0->ceiling_height);
    init_y = fh - (fh - ch) * 0.7f;
  }

  ViewInput vi;
  view_input_init(&vi, init_x, init_y, init_z);

  SDL_SetRelativeMouseMode(SDL_TRUE);

  LOG_INFO("view", "INIT_COMPLETE");

  Uint64 last_time = SDL_GetPerformanceCounter();
  Uint64 freq = SDL_GetPerformanceFrequency();

  while (vi.running) {
    Uint64 now = SDL_GetPerformanceCounter();
    float dt = (float)(now - last_time) / (float)freq;
    last_time = now;
    if (dt > 0.1f)
      dt = 0.1f;

    ViewInputActions acts = view_input_update(&vi, dt);

    // Apply one-shot actions to renderer / GL state.
    if (acts.cycle_color_mode)
      gl.color_mode = (gl.color_mode + 1) % GL_COLOR_MODE_COUNT;
    if (acts.debug_trace)
      rs.debug_trace = true;
    if (acts.adjoin_depth_delta) {
      rs.max_adjoin_depth += acts.adjoin_depth_delta;
      if (rs.max_adjoin_depth > MAX_ADJOIN_DEPTH)
        rs.max_adjoin_depth = MAX_ADJOIN_DEPTH;
      if (rs.max_adjoin_depth < 0)
        rs.max_adjoin_depth = 0;
      LOG_INFO("view", "adjoin depth: %d", rs.max_adjoin_depth);
    }
    if (acts.cull_toggle) {
      rs.cull_mask ^= acts.cull_toggle;
      LOG_INFO("view",
             "[cull] 1:bface=%s 2:ftest=%s 3:sbuf=%s 4:dfs=%s 5:budg=%s "
             "6:fclip=%s 7:pfrust=%s",
             (rs.cull_mask & CULL_BACKFACE) ? "ON" : "off",
             (rs.cull_mask & CULL_FRUSTUM) ? "ON" : "off",
             (rs.cull_mask & CULL_SBUFFER) ? "ON" : "off",
             (rs.cull_mask & CULL_DFS_MARKING) ? "ON" : "off",
             (rs.cull_mask & CULL_PORTAL_BUDGET) ? "ON" : "off",
             (rs.cull_mask & CULL_FRUSTUM_CLIP) ? "ON" : "off",
             (rs.cull_mask & CULL_PORTAL_FRUSTUM) ? "ON" : "off");
    }
    if (acts.cull_reset) {
      rs.cull_mask = CULL_ALL;
      LOG_INFO("view", "[cull] RESET — all stages ON");
    }

    Sector *cam_sector = sector_find_at(&state, float_to_fixed16(vi.cam_x),
                                        float_to_fixed16(vi.cam_y), float_to_fixed16(vi.cam_z));

    Angle14 yaw_a = degrees_to_angle14(vi.yaw_deg);
    Angle14 pitch_a = degrees_to_angle14(vi.pitch_deg);

    // Run CPU render pipeline.
    if (rs.debug_trace)
      LOG_INFO("view", "=== TRACE FRAME  sec=%d  yaw=%.1f  cam=(%.2f,%.2f,%.2f) ===",
             cam_sector->id, vi.yaw_deg, vi.cam_x, vi.cam_y, vi.cam_z);
    render_draw_frame(&rs, cam_sector, vi.cam_x, vi.cam_y, vi.cam_z, yaw_a, pitch_a);
    if (rs.debug_trace) {
      LOG_INFO("view", "=== END TRACE  opaque=%d ===", rs.display_list.opaque_count);
      rs.debug_trace = false;
    }
    static int frame_count = 0;
    static int prev_sector_id = -1;
    if (cam_sector->id != prev_sector_id) {
      LOG_INFO("view", "[frame %d] SECTOR CHANGE %d -> %d  opaque=%d cam=(%.1f,%.1f,%.1f)",
             frame_count, prev_sector_id, cam_sector->id, rs.display_list.opaque_count,
             vi.cam_x, vi.cam_y, vi.cam_z);
      prev_sector_id = cam_sector->id;
    } else if (frame_count % 120 == 0) {
      char buf[256];
      int len = snprintf(buf, sizeof(buf),
                         "[frame %d] sector=%d opaque=%d depth=%d cam=(%.1f,%.1f,%.1f)",
                         frame_count, cam_sector->id, rs.display_list.opaque_count,
                         rs.max_adjoin_depth, vi.cam_x, vi.cam_y, vi.cam_z);
      if (rs.cull_count_sbuffer || rs.cull_count_dfs || rs.cull_count_budget)
        snprintf(buf + len, sizeof(buf) - (size_t)len, "  culled: sbuf=%d dfs=%d budget=%d",
                 rs.cull_count_sbuffer, rs.cull_count_dfs, rs.cull_count_budget);
      LOG_INFO("view", "%s", buf);
    }
    frame_count++;

    // GL draw.
    int draw_w, draw_h;
    SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);
    glViewport(0, 0, draw_w, draw_h);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    gl_backend_draw(&gl, &rs, state.sectors, state.sector_count);

    if (vi.show_minimap) {
      float yaw_rad = vi.yaw_deg * (float)(M_PI / 180.0);
      gl_backend_draw_minimap(&gl, state.sectors, state.sector_count, rs.visited_sectors,
                              cam_sector->id, vi.cam_x, vi.cam_z, yaw_rad, draw_w, draw_h);
    }

    SDL_GL_SwapWindow(window);
  }

  // Cleanup.
  LOG_INFO("view", "SHUTDOWN");
  free(visited_buf);
  gl_backend_destroy(&gl);
  render_state_destroy(&rs);
  SDL_GL_DeleteContext(gl_ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();
  game_level_clear();
  debug_log_shutdown();
  game_memory_shutdown();

  return 0;
}
