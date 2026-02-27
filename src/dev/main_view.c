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
//   L         — one-frame portal trace to stderr
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
#include "io/stream.h"
#include "memory/game_memory.h"
#include "render/render_sector.h"
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

// Angle14 helpers.
static Angle14 degrees_to_angle14(float deg) {
  float wrapped = fmodf(deg, 360.0f);
  if (wrapped < 0.0f)
    wrapped += 360.0f;
  return (Angle14)(wrapped * (float)ANGLE14_FULL_CIRCLE / 360.0f) & ANGLE14_MASK;
}

// Ray-casting point-in-polygon: cast a ray in +X from (px,pz) and count
// how many wall edges it crosses. Odd count = inside.
static bool point_in_sector(const Sector *s, float px, float pz) {
  int32_t crossings = 0;
  for (int32_t i = 0; i < s->wall_count; i++) {
    float z0 = fixed16_to_float(s->walls[i].w0->z);
    float z1 = fixed16_to_float(s->walls[i].w1->z);
    if ((z0 <= pz && z1 > pz) || (z1 <= pz && z0 > pz)) {
      float x0 = fixed16_to_float(s->walls[i].w0->x);
      float x1 = fixed16_to_float(s->walls[i].w1->x);
      float t = (pz - z0) / (z1 - z0);
      float cross_x = x0 + t * (x1 - x0);
      if (px < cross_x)
        crossings++;
    }
  }
  return (crossings & 1) != 0;
}

// Find the sector containing a world-space point.
// AABB pre-filter, exact polygon test, Y-height ranking.
// When multiple sectors overlap at the same XZ (stacked sectors),
// prefer the one whose floor/ceiling range contains cam_y.
static Sector *find_sector(LevelState *state, float wx, float wy, float wz) {
  Fixed16 fx = float_to_fixed16(wx);
  Fixed16 fz = float_to_fixed16(wz);

  Sector *best = NULL;
  bool best_y_match = false;
  Sector *fallback = NULL;
  float best_dist_sq = 1e18f;

  for (int32_t i = 0; i < state->sector_count; i++) {
    Sector *s = &state->sectors[i];
    if (fx < s->bounds_min.x || fx > s->bounds_max.x || fz < s->bounds_min.z ||
        fz > s->bounds_max.z)
      continue;

    if (!point_in_sector(s, wx, wz)) {
      float cx = fixed16_to_float(s->bounds_min.x + s->bounds_max.x) * 0.5f;
      float cz = fixed16_to_float(s->bounds_min.z + s->bounds_max.z) * 0.5f;
      float dx = wx - cx, dz = wz - cz;
      float d = dx * dx + dz * dz;
      if (d < best_dist_sq) {
        best_dist_sq = d;
        fallback = s;
      }
      continue;
    }

    // Y-down: ceiling_height <= cam_y <= floor_height
    float fh = fixed16_to_float(s->floor_height);
    float ch = fixed16_to_float(s->ceiling_height);
    bool y_ok = (wy >= ch && wy <= fh);

    if (y_ok) {
      if (!best || !best_y_match) {
        best = s;
        best_y_match = true;
      } else {
        // Both match Y — prefer tighter vertical fit.
        float cur_h =
            fixed16_to_float(best->floor_height) - fixed16_to_float(best->ceiling_height);
        float new_h = fh - ch;
        if (new_h < cur_h)
          best = s;
      }
    } else if (!best) {
      best = s;
    }
  }

  if (best)
    return best;
  return fallback ? fallback : &state->sectors[0];
}

static bool path_ends_with(const char *path, const char *suffix) {
  size_t plen = strlen(path);
  size_t slen = strlen(suffix);
  if (slen > plen)
    return false;
  for (size_t i = 0; i < slen; i++) {
    char a = path[plen - slen + i];
    char b = suffix[i];
    if (a >= 'A' && a <= 'Z')
      a += 32;
    if (b >= 'A' && b <= 'Z')
      b += 32;
    if (a != b)
      return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  const char *archive_path = (argc > 1) ? argv[1] : DEFAULT_ARCHIVE;
  const char *level_name = (argc > 2) ? argv[2] : DEFAULT_LEVEL;

  // Detect standalone .LEV/.LVT file (bypass archive).
  bool standalone =
      path_ends_with(archive_path, ".lev") || path_ends_with(archive_path, ".lvt");

  game_memory_init();

  LevelState state;
  memset(&state, 0, sizeof(state));
  bool ok = false;

  if (standalone) {
    StreamReader sr = stream_from_file(archive_path);
    if (!sr.read) {
      fprintf(stderr, "ERROR: failed to open '%s'\n", archive_path);
      game_memory_shutdown();
      return 1;
    }
    ok = level_load_geometry(&sr, &state);
    stream_close(&sr);
  } else {
    Archive *ar = archive_open(archive_path);
    if (!ar) {
      fprintf(stderr, "ERROR: failed to open archive '%s'\n", archive_path);
      game_memory_shutdown();
      return 1;
    }

    char filename[48];
    snprintf(filename, sizeof(filename), "%s.LEV", level_name);
    if (!archive_file_exists(ar, filename)) {
      snprintf(filename, sizeof(filename), "%s.LVT", level_name);
      if (!archive_file_exists(ar, filename)) {
        fprintf(stderr, "ERROR: neither '%s.LEV' nor '%s.LVT' found in archive\n",
                level_name, level_name);
        archive_close(ar);
        game_memory_shutdown();
        return 1;
      }
    }

    if (!archive_open_file(ar, filename)) {
      fprintf(stderr, "ERROR: failed to open '%s' in archive\n", filename);
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
    fprintf(stderr, "ERROR: failed to parse '%s'\n", archive_path);
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  printf("loaded: %s — %d sectors, %d walls, %d vertices\n", state.level_name,
         state.sector_count, state.wall_count, state.vertex_count);

  // SDL + OpenGL init.
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    game_level_clear();
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
    fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    SDL_Quit();
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
  if (!gl_ctx) {
    fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
    fprintf(stderr, "gladLoadGLLoader failed\n");
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
    game_memory_shutdown();
    return 1;
  }

  printf("OpenGL %s, %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

  SDL_GL_SetSwapInterval(1);

  // Init render state.
  RenderState rs;
  if (!render_state_init(&rs, WINDOW_W, WINDOW_H, MAX_ADJOIN_DEPTH)) {
    fprintf(stderr, "render_state_init failed\n");
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
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
    fprintf(stderr, "gl_backend_init failed\n");
    render_state_destroy(&rs);
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    game_level_clear();
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
      fprintf(stderr, "adjoin depth: %d\n", rs.max_adjoin_depth);
    }
    if (acts.cull_toggle) {
      rs.cull_mask ^= acts.cull_toggle;
      fprintf(stderr,
              "[cull] 1:bface=%s 2:ftest=%s 3:sbuf=%s 4:dfs=%s 5:budg=%s "
              "6:fclip=%s 7:pfrust=%s\n",
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
      fprintf(stderr, "[cull] RESET — all stages ON\n");
    }

    Sector *cam_sector = find_sector(&state, vi.cam_x, vi.cam_y, vi.cam_z);

    Angle14 yaw_a = degrees_to_angle14(vi.yaw_deg);
    Angle14 pitch_a = degrees_to_angle14(vi.pitch_deg);

    // Run CPU render pipeline.
    if (rs.debug_trace)
      fprintf(stderr, "\n=== TRACE FRAME  sec=%d  yaw=%.1f  cam=(%.2f,%.2f,%.2f) ===\n",
              cam_sector->id, vi.yaw_deg, vi.cam_x, vi.cam_y, vi.cam_z);
    render_draw_frame(&rs, cam_sector, vi.cam_x, vi.cam_y, vi.cam_z, yaw_a, pitch_a);
    if (rs.debug_trace) {
      fprintf(stderr, "=== END TRACE  opaque=%d ===\n\n", rs.display_list.opaque_count);
      rs.debug_trace = false;
    }
    static int frame_count = 0;
    static int prev_sector_id = -1;
    if (cam_sector->id != prev_sector_id) {
      fprintf(stderr,
              "[frame %d] SECTOR CHANGE %d -> %d  opaque=%d cam=(%.1f,%.1f,%.1f)\n",
              frame_count, prev_sector_id, cam_sector->id, rs.display_list.opaque_count,
              vi.cam_x, vi.cam_y, vi.cam_z);
      prev_sector_id = cam_sector->id;
    } else if (frame_count % 120 == 0) {
      fprintf(stderr, "[frame %d] sector=%d opaque=%d depth=%d cam=(%.1f,%.1f,%.1f)",
              frame_count, cam_sector->id, rs.display_list.opaque_count,
              rs.max_adjoin_depth, vi.cam_x, vi.cam_y, vi.cam_z);
      if (rs.cull_count_sbuffer || rs.cull_count_dfs || rs.cull_count_budget)
        fprintf(stderr, "  culled: sbuf=%d dfs=%d budget=%d",
                rs.cull_count_sbuffer, rs.cull_count_dfs, rs.cull_count_budget);
      fprintf(stderr, "\n");
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
  free(visited_buf);
  gl_backend_destroy(&gl);
  render_state_destroy(&rs);
  SDL_GL_DeleteContext(gl_ctx);
  SDL_DestroyWindow(window);
  SDL_Quit();
  game_level_clear();
  game_memory_shutdown();

  return 0;
}
