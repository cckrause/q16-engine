// ===========================================================================
// LAB Archive Inspector
// ===========================================================================
// Dumps directory contents of a LAB file.
//
// Usage:  ./build/q16_lab [path]
//         Default path: mock/ol/outlaws.lab

#include <stdio.h>
#include <stdlib.h>

#include "archive/archive.h"
#include "util/strings.h"

#define DEFAULT_LAB_PATH "mock/ol/outlaws.lab"

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
  const char *path = (argc > 1) ? argv[1] : DEFAULT_LAB_PATH;

  printf("LAB Archive Inspector\n");
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
