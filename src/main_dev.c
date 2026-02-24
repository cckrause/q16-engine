// ===========================================================================
// Development Entry Point
// ===========================================================================
// Native (Mac/Linux) builds only — no Win32, no Glide. Plain main() for
// inspecting data structures and experimenting on the host machine.
//
// Unit tests live in src/tests/ — run with: ./build/q16_tests
// Build: cmake -B build && cmake --build build

#include <stdio.h>
#include <string.h>

#include "archive/archive.h"
#include "io/stream.h"
#include "memory/game_memory.h"
#include "types/forward.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "world/object.h"
#include "world/sector.h"
#include "world/texture.h"
#include "world/wall.h"

#define ARCHIVE_PATH "mock/ol/OLGEO.LAB"
#define LEVEL_FILE   "HIDEOUT.LVT"

static void print_level_info(const LevelState *state) {
  printf("\n=== level info ===\n");
  printf("  name       : %s\n", state->level_name);

  if (state->palette_count > 1) {
    printf("  palettes   : %d\n", state->palette_count);
    for (int32_t i = 0; i < state->palette_count && i < MAX_LEVEL_PALETTES; i++) {
      printf("    [%d] %s\n", i, state->palette_names[i]);
    }
  } else {
    printf("  palette    : %s\n", state->palette_name);
  }

  printf("  music      : %s\n", state->music_name);
  printf("  parallax   : %.2f, %.2f\n", fixed16_to_float(state->parallax0),
         fixed16_to_float(state->parallax1));
  printf("  textures   : %d\n", state->texture_count);
  printf("  sectors    : %d\n", state->sector_count);
  printf("  walls      : %d\n", state->wall_count);
  printf("  vertices   : %d\n", state->vertex_count);
  printf("  secrets    : %d\n", state->secret_count);
  printf("  layers     : %d to %d\n", state->min_layer, state->max_layer);

  if (state->complete_sector) {
    printf("  complete   : sector %d\n", state->complete_sector->id);
  }
  if (state->boss_sector) {
    printf("  boss       : sector %d\n", state->boss_sector->id);
  }
  if (state->mohc_sector) {
    printf("  mohc       : sector %d\n", state->mohc_sector->id);
  }

  printf("\n  --- textures (first 20) ---\n");
  int32_t tex_limit = state->texture_count < 20 ? state->texture_count : 20;
  for (int32_t i = 0; i < tex_limit; i++) {
    const char *name = state->textures[i].name;
    printf("  [%3d] %s\n", i, name[0] ? name : "<NoTexture>");
  }
  if (state->texture_count > 20) {
    printf("  ... and %d more\n", state->texture_count - 20);
  }

  printf("\n  --- sectors (first 20) ---\n");
  printf("  %-8s  %-16s  %8s  %8s  %6s  %5s  %5s  %s\n", "ID", "NAME", "FLOOR", "CEIL",
         "AMB", "WALLS", "VERTS", "FLAGS");

  int32_t sec_limit = state->sector_count < 20 ? state->sector_count : 20;
  for (int32_t i = 0; i < sec_limit; i++) {
    const Sector *s = &state->sectors[i];
    printf("  %-8d  %-16s  %8.2f  %8.2f  %6.0f  %5d  %5d  0x%X", s->id,
           s->name[0] ? s->name : "-", fixed16_to_float(s->floor_height),
           fixed16_to_float(s->ceiling_height), fixed16_to_float(s->ambient),
           s->wall_count, s->vertex_count, s->flags1);
    if (s->slope_floor.sector_idx >= 0) {
      printf("  slope_f(%d,%d,%d)", s->slope_floor.sector_idx, s->slope_floor.wall_idx,
             s->slope_floor.angle);
    }
    if (s->slope_ceiling.sector_idx >= 0) {
      printf("  slope_c(%d,%d,%d)", s->slope_ceiling.sector_idx,
             s->slope_ceiling.wall_idx, s->slope_ceiling.angle);
    }
    printf("\n");
  }
  if (state->sector_count > 20) {
    printf("  ... and %d more sectors\n", state->sector_count - 20);
  }
}

int main(void) {
  printf("q16 engine dev build\n");

  printf("\n=== struct sizes ===\n");
  printf("  Sector  : %zu bytes\n", sizeof(struct Sector));
  printf("  Wall    : %zu bytes\n", sizeof(struct Wall));
  printf("  Object  : %zu bytes\n", sizeof(struct SecObject));
  printf("  Level   : %zu bytes\n", sizeof(struct LevelState));

  // Open archive
  Archive *ar = archive_open(ARCHIVE_PATH);
  if (!ar) {
    fprintf(stderr, "ERROR: failed to open archive '%s'\n", ARCHIVE_PATH);
    return 1;
  }
  printf("\narchive: %s (%d files)\n", ARCHIVE_PATH, archive_get_file_count(ar));

  if (!archive_file_exists(ar, LEVEL_FILE)) {
    fprintf(stderr, "ERROR: '%s' not found in archive\n", LEVEL_FILE);
    archive_close(ar);
    return 1;
  }

  if (!archive_open_file(ar, LEVEL_FILE)) {
    fprintf(stderr, "ERROR: failed to open '%s'\n", LEVEL_FILE);
    archive_close(ar);
    return 1;
  }

  // Init memory and parse level geometry
  game_memory_init();

  StreamReader sr = stream_from_archive(ar);
  LevelState state;
  bool ok = level_load_geometry(&sr, &state);

  archive_close_file(ar);

  if (!ok) {
    fprintf(stderr, "ERROR: failed to parse '%s'\n", LEVEL_FILE);
    game_level_clear();
    game_memory_shutdown();
    archive_close(ar);
    return 1;
  }

  print_level_info(&state);

  // Cleanup
  game_level_clear();
  game_memory_shutdown();
  archive_close(ar);

  return 0;
}
