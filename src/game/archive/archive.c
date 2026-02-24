// ===========================================================================
// Archive — Unified Read-only Archive (GOB / LAB)
// ===========================================================================
// Auto-detects format from magic bytes. Normalizes both formats into a
// single entry array for transparent access.

#include "archive/archive.h"
#include "util/strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Internal structures

typedef struct ArchiveEntry {
  uint32_t offset;
  uint32_t length;
  char *name; // heap-allocated, freed on close
} ArchiveEntry;

struct Archive {
  char *path;
  ArchiveEntry *entries;
  int32_t file_count;
  ArchiveFormat format;

  FILE *fp;
  int32_t cur_index;
  uint32_t cur_base;
  uint32_t cur_length;
  uint32_t cur_pos;
};

// Helpers

static char *heap_strdup(const char *src, int32_t max_len) {
  int32_t len = 0;
  while (len < max_len && src[len]) {
    len++;
  }
  char *dst = (char *)malloc((size_t)len + 1);
  if (dst) {
    memcpy(dst, src, (size_t)len);
    dst[len] = '\0';
  }
  return dst;
}

static void free_entries(ArchiveEntry *entries, int32_t count) {
  if (!entries) {
    return;
  }
  for (int32_t i = 0; i < count; i++) {
    free(entries[i].name);
  }
  free(entries);
}

// GOB format parser
// Magic: "GOB\n", then u32 directory offset.
// Directory: u32 count, then entries (offset u32 + length u32 + name char[13]).

#define GOB_NAME_LEN 13

static bool parse_gob(FILE *fp, ArchiveEntry **out_entries, int32_t *out_count) {
  uint32_t master_offset;
  if (fread(&master_offset, 4, 1, fp) != 1) {
    return false;
  }

  if (fseek(fp, (long)master_offset, SEEK_SET) != 0) {
    return false;
  }

  uint32_t file_count;
  if (fread(&file_count, 4, 1, fp) != 1) {
    return false;
  }

  ArchiveEntry *entries = NULL;
  if (file_count > 0) {
    entries = (ArchiveEntry *)calloc(file_count, sizeof(ArchiveEntry));
    if (!entries) {
      return false;
    }

    for (uint32_t i = 0; i < file_count; i++) {
      uint32_t offset, length;
      char name_buf[GOB_NAME_LEN];

      if (fread(&offset, 4, 1, fp) != 1 || fread(&length, 4, 1, fp) != 1 ||
          fread(name_buf, 1, GOB_NAME_LEN, fp) != GOB_NAME_LEN) {
        free_entries(entries, (int32_t)i);
        return false;
      }
      name_buf[GOB_NAME_LEN - 1] = '\0';

      entries[i].offset = offset;
      entries[i].length = length;
      entries[i].name = heap_strdup(name_buf, GOB_NAME_LEN);
    }
  }

  *out_entries = entries;
  *out_count = (int32_t)file_count;
  return true;
}

// LAB format parser
// Magic: "LABN", then u32 version, u32 fileCount, u32 stringTableSize.
// Entries: 16 bytes each (nameOffset + dataOffset + length + typeId).
// String table: null-terminated filenames.

static bool parse_lab(FILE *fp, ArchiveEntry **out_entries, int32_t *out_count) {
  uint32_t version, file_count, string_table_size;
  if (fread(&version, 4, 1, fp) != 1 || fread(&file_count, 4, 1, fp) != 1 ||
      fread(&string_table_size, 4, 1, fp) != 1) {
    return false;
  }

  typedef struct {
    uint32_t name_offset;
    uint32_t data_offset;
    uint32_t length;
    uint32_t type_id;
  } RawLabEntry;

  RawLabEntry *raw = NULL;
  if (file_count > 0) {
    raw = (RawLabEntry *)malloc(sizeof(RawLabEntry) * file_count);
    if (!raw) {
      return false;
    }
    for (uint32_t i = 0; i < file_count; i++) {
      if (fread(&raw[i].name_offset, 4, 1, fp) != 1 ||
          fread(&raw[i].data_offset, 4, 1, fp) != 1 ||
          fread(&raw[i].length, 4, 1, fp) != 1 || fread(&raw[i].type_id, 4, 1, fp) != 1) {
        free(raw);
        return false;
      }
    }
  }

  char *string_table = NULL;
  if (string_table_size > 0) {
    string_table = (char *)malloc(string_table_size);
    if (!string_table) {
      free(raw);
      return false;
    }
    if (fread(string_table, 1, string_table_size, fp) != string_table_size) {
      free(string_table);
      free(raw);
      return false;
    }
  }

  ArchiveEntry *entries = NULL;
  if (file_count > 0) {
    entries = (ArchiveEntry *)calloc(file_count, sizeof(ArchiveEntry));
    if (!entries) {
      free(string_table);
      free(raw);
      return false;
    }

    for (uint32_t i = 0; i < file_count; i++) {
      entries[i].offset = raw[i].data_offset;
      entries[i].length = raw[i].length;

      const char *src_name = "";
      if (string_table && raw[i].name_offset < string_table_size) {
        src_name = string_table + raw[i].name_offset;
      }
      entries[i].name = heap_strdup(src_name, 256);
    }
  }

  free(string_table);
  free(raw);

  *out_entries = entries;
  *out_count = (int32_t)file_count;
  return true;
}

// Lifecycle

Archive *archive_open(const char *path) {
  if (!path) {
    return NULL;
  }

  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return NULL;
  }

  uint8_t magic[4];
  if (fread(magic, 1, 4, fp) != 4) {
    fclose(fp);
    return NULL;
  }

  ArchiveFormat format;
  ArchiveEntry *entries = NULL;
  int32_t file_count = 0;
  bool ok;

  if (magic[0] == 'G' && magic[1] == 'O' && magic[2] == 'B' && magic[3] == '\n') {
    format = ARCHIVE_FMT_GOB;
    ok = parse_gob(fp, &entries, &file_count);
  } else if (magic[0] == 'L' && magic[1] == 'A' && magic[2] == 'B' && magic[3] == 'N') {
    format = ARCHIVE_FMT_LAB;
    ok = parse_lab(fp, &entries, &file_count);
  } else {
    fclose(fp);
    return NULL;
  }

  fclose(fp);

  if (!ok) {
    return NULL;
  }

  Archive *ar = (Archive *)malloc(sizeof(Archive));
  if (!ar) {
    free_entries(entries, file_count);
    return NULL;
  }

  size_t path_len = strlen(path);
  ar->path = (char *)malloc(path_len + 1);
  if (!ar->path) {
    free_entries(entries, file_count);
    free(ar);
    return NULL;
  }
  memcpy(ar->path, path, path_len + 1);

  ar->entries = entries;
  ar->file_count = file_count;
  ar->format = format;
  ar->fp = NULL;
  ar->cur_index = -1;
  ar->cur_base = 0;
  ar->cur_length = 0;
  ar->cur_pos = 0;

  return ar;
}

void archive_close(Archive *ar) {
  if (!ar) {
    return;
  }
  archive_close_file(ar);
  free_entries(ar->entries, ar->file_count);
  free(ar->path);
  free(ar);
}

ArchiveFormat archive_get_format(const Archive *ar) {
  return ar ? ar->format : ARCHIVE_FMT_GOB;
}

// Directory queries

int32_t archive_get_file_count(const Archive *ar) {
  return ar ? ar->file_count : 0;
}

const char *archive_get_file_name(const Archive *ar, int32_t index) {
  if (!ar || index < 0 || index >= ar->file_count) {
    return NULL;
  }
  return ar->entries[index].name;
}

uint32_t archive_get_file_length(const Archive *ar, int32_t index) {
  if (!ar || index < 0 || index >= ar->file_count) {
    return 0;
  }
  return ar->entries[index].length;
}

int32_t archive_get_file_index(const Archive *ar, const char *name) {
  if (!ar || !name) {
    return -1;
  }
  for (int32_t i = 0; i < ar->file_count; i++) {
    if (ar->entries[i].name && str_equal_nocase(ar->entries[i].name, name)) {
      return i;
    }
  }
  return -1;
}

bool archive_file_exists(const Archive *ar, const char *name) {
  return archive_get_file_index(ar, name) >= 0;
}

// File I/O

bool archive_open_file(Archive *ar, const char *name) {
  int32_t index = archive_get_file_index(ar, name);
  if (index < 0) {
    return false;
  }
  return archive_open_file_index(ar, index);
}

bool archive_open_file_index(Archive *ar, int32_t index) {
  if (!ar || index < 0 || index >= ar->file_count) {
    return false;
  }

  archive_close_file(ar);

  ar->fp = fopen(ar->path, "rb");
  if (!ar->fp) {
    return false;
  }

  ar->cur_index = index;
  ar->cur_base = ar->entries[index].offset;
  ar->cur_length = ar->entries[index].length;
  ar->cur_pos = 0;

  if (fseek(ar->fp, (long)ar->cur_base, SEEK_SET) != 0) {
    fclose(ar->fp);
    ar->fp = NULL;
    ar->cur_index = -1;
    return false;
  }

  return true;
}

void archive_close_file(Archive *ar) {
  if (!ar) {
    return;
  }
  if (ar->fp) {
    fclose(ar->fp);
    ar->fp = NULL;
  }
  ar->cur_index = -1;
  ar->cur_base = 0;
  ar->cur_length = 0;
  ar->cur_pos = 0;
}

int32_t archive_read_file(Archive *ar, void *data, int32_t size) {
  if (!ar || !ar->fp || !data || size <= 0) {
    return 0;
  }

  uint32_t remaining = ar->cur_length - ar->cur_pos;
  uint32_t to_read = (uint32_t)size < remaining ? (uint32_t)size : remaining;
  if (to_read == 0) {
    return 0;
  }

  size_t actual = fread(data, 1, to_read, ar->fp);
  ar->cur_pos += (uint32_t)actual;
  return (int32_t)actual;
}

bool archive_seek_file(Archive *ar, int32_t offset, int origin) {
  if (!ar || !ar->fp) {
    return false;
  }

  int32_t new_pos;
  switch (origin) {
  case SEEK_SET:
    new_pos = offset;
    break;
  case SEEK_CUR:
    new_pos = (int32_t)ar->cur_pos + offset;
    break;
  case SEEK_END:
    new_pos = (int32_t)ar->cur_length + offset;
    break;
  default:
    return false;
  }

  if (new_pos < 0 || (uint32_t)new_pos > ar->cur_length) {
    return false;
  }

  ar->cur_pos = (uint32_t)new_pos;
  long abs_pos = (long)ar->cur_base + (long)ar->cur_pos;
  return fseek(ar->fp, abs_pos, SEEK_SET) == 0;
}

int32_t archive_get_loc_in_file(const Archive *ar) {
  if (!ar || ar->cur_index < 0) {
    return -1;
  }
  return (int32_t)ar->cur_pos;
}
