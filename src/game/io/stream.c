// ===========================================================================
// StreamReader — Abstract Byte Stream
// ===========================================================================
// Backends for file I/O and archive-based reading.

#include "io/stream.h"
#include "archive/archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Archive backend

static int32_t archive_read(void *ctx, void *buf, int32_t size) {
  return archive_read_file((Archive *)ctx, buf, size);
}

static bool archive_seek(void *ctx, int32_t offset, int origin) {
  return archive_seek_file((Archive *)ctx, offset, origin);
}

static int32_t archive_tell(void *ctx) {
  return archive_get_loc_in_file((const Archive *)ctx);
}

static void archive_close_noop(void *ctx) {
  (void)ctx;
}

StreamReader stream_from_archive(struct Archive *ar) {
  StreamReader s;
  s.read = archive_read;
  s.seek = archive_seek;
  s.tell = archive_tell;
  s.close = archive_close_noop;
  s.ctx = ar;
  return s;
}

// File backend

static int32_t file_read(void *ctx, void *buf, int32_t size) {
  return (int32_t)fread(buf, 1, (size_t)size, (FILE *)ctx);
}

static bool file_seek(void *ctx, int32_t offset, int origin) {
  return fseek((FILE *)ctx, (long)offset, origin) == 0;
}

static int32_t file_tell(void *ctx) {
  return (int32_t)ftell((FILE *)ctx);
}

static void file_close(void *ctx) {
  if (ctx) {
    fclose((FILE *)ctx);
  }
}

StreamReader stream_from_file(const char *path) {
  StreamReader s = {0};
  if (!path) {
    return s;
  }
  FILE *fp = fopen(path, "rb");
  if (!fp) {
    return s;
  }
  s.read = file_read;
  s.seek = file_seek;
  s.tell = file_tell;
  s.close = file_close;
  s.ctx = fp;
  return s;
}

// Memory backend

typedef struct MemStream {
  const uint8_t *data;
  int32_t size;
  int32_t pos;
} MemStream;

static int32_t mem_read(void *ctx, void *buf, int32_t size) {
  MemStream *ms = (MemStream *)ctx;
  int32_t remaining = ms->size - ms->pos;
  int32_t to_read = size < remaining ? size : remaining;
  if (to_read <= 0) {
    return 0;
  }
  memcpy(buf, ms->data + ms->pos, (size_t)to_read);
  ms->pos += to_read;
  return to_read;
}

static bool mem_seek(void *ctx, int32_t offset, int origin) {
  MemStream *ms = (MemStream *)ctx;
  int32_t new_pos;
  switch (origin) {
  case 0:
    new_pos = offset;
    break; // SEEK_SET
  case 1:
    new_pos = ms->pos + offset;
    break; // SEEK_CUR
  case 2:
    new_pos = ms->size + offset;
    break; // SEEK_END
  default:
    return false;
  }
  if (new_pos < 0 || new_pos > ms->size) {
    return false;
  }
  ms->pos = new_pos;
  return true;
}

static int32_t mem_tell(void *ctx) {
  return ((MemStream *)ctx)->pos;
}

static void mem_close(void *ctx) {
  free(ctx);
}

StreamReader stream_from_memory(const void *data, int32_t size) {
  StreamReader s = {0};
  if (!data || size < 0) {
    return s;
  }
  MemStream *ms = (MemStream *)malloc(sizeof(MemStream));
  if (!ms) {
    return s;
  }
  ms->data = (const uint8_t *)data;
  ms->size = size;
  ms->pos = 0;

  s.read = mem_read;
  s.seek = mem_seek;
  s.tell = mem_tell;
  s.close = mem_close;
  s.ctx = ms;
  return s;
}

// Common

void stream_close(StreamReader *s) {
  if (s && s->close) {
    s->close(s->ctx);
  }
  if (s) {
    memset(s, 0, sizeof(*s));
  }
}
