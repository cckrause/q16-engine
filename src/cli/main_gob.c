// ===========================================================================
// GOB Archive Inspector
// ===========================================================================
// Dumps directory contents of a GOB file.
//
// Usage:  ./build/q16_gob [path]
//         Default path: mock/df/DARK.GOB

#include <stdio.h>
#include <stdlib.h>

#include "archive/archive.h"
#include "util/strings.h"

#define DEFAULT_GOB_PATH "mock/df/DARK.GOB"

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

int main(int argc, char *argv[]) {
  const char *path = (argc > 1) ? argv[1] : DEFAULT_GOB_PATH;

  printf("GOB Archive Inspector\n");
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

  printf("  %-6s  %-13s  %10s\n", "INDEX", "NAME", "SIZE");
  printf("  %-6s  %-13s  %10s\n", "-----", "-------------", "----------");

  for (int32_t i = 0; i < count; i++) {
    const char *name = archive_get_file_name(ar, i);
    uint32_t len = archive_get_file_length(ar, i);
    char size_buf[32];

    printf("  %-6d  %-13s  %10s\n", i, name, format_size(len, size_buf, 32));

    total_bytes += len;
    if (len > largest_size) {
      largest_size = len;
      largest_index = i;
    }
  }

  printf("\n  --- summary ---\n");

  char size_buf[32];
  printf("  total files : %d\n", count);
  printf("  total data  : %s\n", format_size((uint32_t)total_bytes, size_buf, 32));

  if (largest_index >= 0) {
    printf("  largest file: %s (%s)\n", archive_get_file_name(ar, largest_index),
           format_size(largest_size, size_buf, 32));
  }

  printf("\n  --- by extension ---\n");

  struct {
    char ext[8];
    int32_t count;
    uint64_t bytes;
  } ext_stats[32];
  int32_t ext_count = 0;

  for (int32_t i = 0; i < count; i++) {
    const char *ext = get_extension(archive_get_file_name(ar, i));
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
    } else if (ext_count < 32) {
      snprintf(ext_stats[ext_count].ext, 8, "%s", ext);
      ext_stats[ext_count].count = 1;
      ext_stats[ext_count].bytes = len;
      ext_count++;
    }
  }

  printf("  %-6s  %5s  %10s\n", "EXT", "COUNT", "TOTAL");
  printf("  %-6s  %5s  %10s\n", "------", "-----", "----------");
  for (int32_t j = 0; j < ext_count; j++) {
    printf("  .%-5s  %5d  %10s\n", ext_stats[j].ext, ext_stats[j].count,
           format_size((uint32_t)ext_stats[j].bytes, size_buf, 32));
  }

  archive_close(ar);

  return 0;
}
