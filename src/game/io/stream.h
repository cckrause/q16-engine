// ===========================================================================
// StreamReader — Abstract byte stream with read/seek/tell
// ===========================================================================
#ifndef Q16_STREAM_H
#define Q16_STREAM_H

#include <stdbool.h>
#include <stdint.h>

// Function-pointer vtable decoupling format parsers from data sources.
// Three concrete backends: archive, stdio FILE*, and memory buffer.
//
// The StreamReader does NOT own the underlying resource in the archive case
// (the caller must have called archive_open_file first). The file and memory
// backends own their resources and release them on close.

struct Archive; // avoid pulling in archive.h

typedef struct StreamReader {
  int32_t (*read)(void *ctx, void *buf, int32_t size);
  bool (*seek)(void *ctx, int32_t offset, int origin);
  int32_t (*tell)(void *ctx);
  void (*close)(void *ctx);
  void *ctx;
} StreamReader;

// --- Concrete constructors -------------------------------------------------

// Wrap an open archive file handle. Requires archive_open_file() first.
// Does NOT close the archive file on stream_close — caller manages archive lifetime.
StreamReader stream_from_archive(struct Archive *ar);

// Open a file from disk (stdio "rb"). stream_close frees the FILE*.
StreamReader stream_from_file(const char *path);

// Wrap a read-only memory buffer. No allocation, no close needed.
StreamReader stream_from_memory(const void *data, int32_t size);

// Close the stream (calls the backend close, then zeroes the struct).
void stream_close(StreamReader *s);

#endif /* Q16_STREAM_H */
