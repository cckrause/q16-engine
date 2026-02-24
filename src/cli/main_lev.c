// ===========================================================================
// Level Inspector
// ===========================================================================
// Streaming parse of a LEV/LVT file from an archive.
//
// Usage:  ./build/q16_lev [archive_path] [level_name]
//         Default: mock/df/DARK.GOB  SECBASE
//
// Auto-detects format (LEV/LVT) from file contents.

#include <stdio.h>
#include <string.h>

#include "archive/archive.h"
#include "io/stream.h"
#include "memory/game_memory.h"
#include "types/types.h"
#include "world/flags.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "world/sector.h"
#include "world/texture.h"
#include "world/wall.h"

#define DEFAULT_ARCHIVE "mock/df/DARK.GOB"
#define DEFAULT_NAME    "SECBASE"

int main(int argc, char *argv[]) {
  const char *archive_path = (argc > 1) ? argv[1] : DEFAULT_ARCHIVE;
  const char *level_name = (argc > 2) ? argv[2] : DEFAULT_NAME;

  Archive *ar = archive_open(archive_path);
  if (!ar) {
    fprintf(stderr, "ERROR: failed to open archive '%s'\n", archive_path);
    return 1;
  }

  // Try .LEV first, then .LVT
  char filename[48];
  snprintf(filename, sizeof(filename), "%s.LEV", level_name);
  if (!archive_file_exists(ar, filename)) {
    snprintf(filename, sizeof(filename), "%s.LVT", level_name);
    if (!archive_file_exists(ar, filename)) {
      fprintf(stderr,
              "ERROR: neither '%s.LEV' nor '%s.LVT' found in archive\n",
              level_name, level_name);
      archive_close(ar);
      return 1;
    }
  }

  printf("Level Inspector\n");
  printf("  archive: %s\n", archive_path);
  printf("  file:    %s\n\n", filename);

  if (!archive_open_file(ar, filename)) {
    fprintf(stderr, "ERROR: failed to open '%s' in archive\n", filename);
    archive_close(ar);
    return 1;
  }

  game_memory_init();

  StreamReader sr = stream_from_archive(ar);
  LevelState state;
  bool ok = level_load_geometry(&sr, &state);

  archive_close_file(ar);

  if (!ok) {
    fprintf(stderr, "ERROR: failed to parse '%s'\n", filename);
    game_level_clear();
    game_memory_shutdown();
    archive_close(ar);
    return 1;
  }

  printf("  level name : %s\n", state.level_name);
  if (state.palette_count > 1) {
    printf("  palettes   : %d\n", state.palette_count);
    for (int32_t i = 0; i < state.palette_count && i < MAX_LEVEL_PALETTES;
         i++) {
      printf("    [%d] %s\n", i, state.palette_names[i]);
    }
  } else {
    printf("  palette    : %s\n", state.palette_name);
  }
  printf("  music      : %s\n", state.music_name);
  printf("  parallax   : %.2f, %.2f\n", fixed16_to_float(state.parallax0),
         fixed16_to_float(state.parallax1));
  printf("  textures   : %d\n", state.texture_count);
  printf("  sectors    : %d\n", state.sector_count);
  printf("  walls      : %d\n", state.wall_count);
  printf("  vertices   : %d\n", state.vertex_count);
  printf("  secrets    : %d\n", state.secret_count);
  printf("  layers     : %d to %d\n", state.min_layer, state.max_layer);

  if (state.complete_sector) {
    printf("  complete   : sector %d\n", state.complete_sector->id);
  }
  if (state.boss_sector) {
    printf("  boss       : sector %d\n", state.boss_sector->id);
  }
  if (state.mohc_sector) {
    printf("  mohc       : sector %d\n", state.mohc_sector->id);
  }

  printf("\n  --- textures ---\n");
  for (int32_t i = 0; i < state.texture_count && i < 20; i++) {
    const char *name = state.textures[i].name;
    printf("  [%3d] %s\n", i, name[0] ? name : "<NoTexture>");
  }
  if (state.texture_count > 20) {
    printf("  ... and %d more\n", state.texture_count - 20);
  }

  printf("\n  --- sectors (first 20) ---\n");
  printf("  %-8s  %-16s  %8s  %8s  %6s  %5s  %5s  %s\n", "ID", "NAME",
         "FLOOR", "CEIL", "AMB", "WALLS", "VERTS", "FLAGS");

  int32_t limit = state.sector_count < 20 ? state.sector_count : 20;
  for (int32_t i = 0; i < limit; i++) {
    Sector *s = &state.sectors[i];
    printf("  %-8d  %-16s  %8.2f  %8.2f  %6.0f  %5d  %5d  0x%X",
           s->id, s->name[0] ? s->name : "-",
           fixed16_to_float(s->floor_height),
           fixed16_to_float(s->ceiling_height),
           fixed16_to_float(s->ambient), s->wall_count, s->vertex_count,
           s->flags1);
    if (s->slope_floor.sector_idx >= 0) {
      printf("  slope_f(%d,%d,%d)", s->slope_floor.sector_idx,
             s->slope_floor.wall_idx, s->slope_floor.angle);
    }
    if (s->slope_ceiling.sector_idx >= 0) {
      printf("  slope_c(%d,%d,%d)", s->slope_ceiling.sector_idx,
             s->slope_ceiling.wall_idx, s->slope_ceiling.angle);
    }
    printf("\n");
  }
  if (state.sector_count > 20) {
    printf("  ... and %d more sectors\n", state.sector_count - 20);
  }

  game_level_clear();
  game_memory_shutdown();
  archive_close(ar);

  return 0;
}
