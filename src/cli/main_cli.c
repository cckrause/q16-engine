// ===========================================================================
// q16 CLI — Unified command-line inspector
// ===========================================================================

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "archive/archive.h"
#include "io/stream.h"
#include "memory/game_memory.h"
#include "types/types.h"
#include "util/strings.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "world/object.h"
#include "world/sector.h"
#include "world/texture.h"
#include "world/wall.h"

// --- Helpers ---

static const char *format_size(uint32_t bytes, char *buf, int buf_len) {
  if (bytes >= 1024 * 1024) {
    snprintf(buf, (size_t)buf_len, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
  } else if (bytes >= 1024) {
    snprintf(buf, (size_t)buf_len, "%.1f KB", (double)bytes / 1024.0);
  } else {
    snprintf(buf, (size_t)buf_len, "%u B", bytes);
  }
  return buf;
}

static const char *get_extension(const char *name) {
  const char *dot = NULL;
  while (*name) {
    if (*name == '.') {
      dot = name + 1;
    }
    name++;
  }
  return dot ? dot : "";
}

// --- Archive subcommand ---

static int cmd_archive(const char *path) {
  printf("Archive Inspector\n");
  printf("  file: %s\n\n", path);

  Archive *ar = archive_open(path);
  if (!ar) {
    fprintf(stderr, "ERROR: failed to open '%s'\n", path);
    return 1;
  }

  int32_t count = archive_get_file_count(ar);
  printf("  entries: %d\n\n", count);

  uint64_t total_bytes = 0;
  uint32_t largest_size = 0;
  int32_t largest_index = -1;

  int max_name_len = 13;
  for (int32_t i = 0; i < count; i++) {
    const char *name = archive_get_file_name(ar, i);
    if (name) {
      int len = 0;
      while (name[len]) {
        len++;
      }
      if (len > max_name_len) {
        max_name_len = len;
      }
    }
  }
  if (max_name_len > 40) {
    max_name_len = 40;
  }

  printf("  %-6s  %-*s  %10s\n", "INDEX", max_name_len, "NAME", "SIZE");
  printf("  %-6s  ", "-----");
  for (int i = 0; i < max_name_len; i++) {
    putchar('-');
  }
  printf("  %10s\n", "----------");

  for (int32_t i = 0; i < count; i++) {
    const char *name = archive_get_file_name(ar, i);
    uint32_t len = archive_get_file_length(ar, i);
    char size_buf[32];

    printf("  %-6d  %-*s  %10s\n", i, max_name_len,
           name ? name : "(null)", format_size(len, size_buf, 32));

    total_bytes += len;
    if (len > largest_size) {
      largest_size = len;
      largest_index = i;
    }
  }

  char size_buf[32];

  printf("\n  --- summary ---\n");
  printf("  total files : %d\n", count);
  printf("  total data  : %s\n", format_size((uint32_t)total_bytes, size_buf, 32));

  if (largest_index >= 0) {
    printf("  largest file: %s (%s)\n", archive_get_file_name(ar, largest_index),
           format_size(largest_size, size_buf, 32));
  }

  printf("\n  --- by extension ---\n");

  struct {
    char ext[16];
    int32_t count;
    uint64_t bytes;
  } ext_stats[64];
  int32_t ext_count = 0;

  for (int32_t i = 0; i < count; i++) {
    const char *name = archive_get_file_name(ar, i);
    if (!name) {
      continue;
    }
    const char *ext = get_extension(name);
    uint32_t len = archive_get_file_length(ar, i);

    int32_t found = -1;
    for (int32_t j = 0; j < ext_count; j++) {
      if (str_equal_nocase(ext_stats[j].ext, ext)) {
        found = j;
        break;
      }
    }

    if (found >= 0) {
      ext_stats[found].count++;
      ext_stats[found].bytes += len;
    } else if (ext_count < 64) {
      snprintf(ext_stats[ext_count].ext, 16, "%s", ext);
      ext_stats[ext_count].count = 1;
      ext_stats[ext_count].bytes = len;
      ext_count++;
    }
  }

  printf("  %-10s  %5s  %10s\n", "EXT", "COUNT", "TOTAL");
  printf("  %-10s  %5s  %10s\n", "----------", "-----", "----------");
  for (int32_t j = 0; j < ext_count; j++) {
    printf("  .%-9s  %5d  %10s\n", ext_stats[j].ext, ext_stats[j].count,
           format_size((uint32_t)ext_stats[j].bytes, size_buf, 32));
  }

  archive_close(ar);
  return 0;
}

// --- Level subcommand ---

static void print_level_info(const LevelState *state) {
  printf("  level name : %s\n", state->level_name);

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

  printf("\n  --- textures ---\n");
  for (int32_t i = 0; i < state->texture_count && i < 20; i++) {
    const char *name = state->textures[i].name;
    printf("  [%3d] %s\n", i, name[0] ? name : "<NoTexture>");
  }
  if (state->texture_count > 20) {
    printf("  ... and %d more\n", state->texture_count - 20);
  }

  printf("\n  --- sectors (first 20) ---\n");
  printf("  %-8s  %-16s  %8s  %8s  %6s  %5s  %5s  %s\n",
         "ID", "NAME", "FLOOR", "CEIL", "AMB", "WALLS", "VERTS", "FLAGS");

  int32_t limit = state->sector_count < 20 ? state->sector_count : 20;
  for (int32_t i = 0; i < limit; i++) {
    const Sector *s = &state->sectors[i];
    printf("  %-8d  %-16s  %8.2f  %8.2f  %6.0f  %5d  %5d  0x%X",
           s->id, s->name[0] ? s->name : "-",
           fixed16_to_float(s->floor_height),
           fixed16_to_float(s->ceiling_height),
           fixed16_to_float(s->ambient),
           s->wall_count, s->vertex_count, s->flags1);
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
  if (state->sector_count > 20) {
    printf("  ... and %d more sectors\n", state->sector_count - 20);
  }
}

static int cmd_level(const char *archive_path, const char *level_name) {
  Archive *ar = archive_open(archive_path);
  if (!ar) {
    fprintf(stderr, "ERROR: failed to open archive '%s'\n", archive_path);
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

  print_level_info(&state);

  game_level_clear();
  game_memory_shutdown();
  archive_close(ar);
  return 0;
}

// --- Info subcommand ---

static int cmd_info(void) {
  printf("q16 Engine — Struct Sizes\n\n");
  printf("  Sector  : %zu bytes\n", sizeof(Sector));
  printf("  Wall    : %zu bytes\n", sizeof(Wall));
  printf("  Object  : %zu bytes\n", sizeof(SecObject));
  printf("  Level   : %zu bytes\n", sizeof(LevelState));
  return 0;
}

// --- Main ---

static void print_usage(const char *prog) {
  printf("q16 CLI — Unified command-line inspector\n\n");
  printf("Usage:\n");
  printf("  %s archive <path>           List archive contents\n", prog);
  printf("  %s level <archive> <name>   Inspect level geometry\n", prog);
  printf("  %s info                     Print struct sizes\n", prog);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "archive") == 0) {
    if (argc < 3) {
      fprintf(stderr, "ERROR: missing archive path\n\n");
      fprintf(stderr, "Usage: %s archive <path>\n", argv[0]);
      return 1;
    }
    return cmd_archive(argv[2]);
  }

  if (strcmp(argv[1], "level") == 0) {
    if (argc < 4) {
      fprintf(stderr, "ERROR: missing %s\n\n",
              argc < 3 ? "archive path and level name" : "level name");
      fprintf(stderr, "Usage: %s level <archive> <name>\n", argv[0]);
      return 1;
    }
    return cmd_level(argv[2], argv[3]);
  }

  if (strcmp(argv[1], "info") == 0) {
    return cmd_info();
  }

  fprintf(stderr, "ERROR: unknown command '%s'\n\n", argv[1]);
  print_usage(argv[0]);
  return 1;
}
