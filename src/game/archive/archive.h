// ===========================================================================
// Archive — Unified read-only archive for GOB and LAB formats
// ===========================================================================
#ifndef Q16_ARCHIVE_H
#define Q16_ARCHIVE_H

#include <stdbool.h>
#include <stdint.h>

// Supports two on-disk formats, auto-detected from magic bytes:
//
//   GOB (Dark Forces):  "GOB\n" magic, directory at end of file,
//                       13-byte inline filenames.
//   LAB (Outlaws):      "LABN" magic, directory after header,
//                       variable-length filenames in a string table.
//
// After open(), the format is transparent — all queries and I/O use
// the same normalized entry array. Only one file can be open for
// reading at a time.

typedef enum ArchiveFormat {
  ARCHIVE_FMT_GOB, // Dark Forces .GOB
  ARCHIVE_FMT_LAB, // Outlaws .LAB
} ArchiveFormat;

// Opaque archive handle.
typedef struct Archive Archive;

// --- Lifecycle -------------------------------------------------------------

// Open an archive from disk. Auto-detects GOB or LAB from magic bytes.
// Returns NULL on I/O error, unrecognized format, or invalid header.
Archive *archive_open(const char *path);

// Close the archive and free all resources.
void archive_close(Archive *ar);

// Which on-disk format was detected.
ArchiveFormat archive_get_format(const Archive *ar);

// --- Directory queries (read-only, no file I/O) ----------------------------

int32_t archive_get_file_count(const Archive *ar);
const char *archive_get_file_name(const Archive *ar, int32_t index);
uint32_t archive_get_file_length(const Archive *ar, int32_t index);

// Case-insensitive linear search. Returns -1 if not found.
int32_t archive_get_file_index(const Archive *ar, const char *name);

bool archive_file_exists(const Archive *ar, const char *name);

// --- File I/O (single file at a time) --------------------------------------

bool archive_open_file(Archive *ar, const char *name);
bool archive_open_file_index(Archive *ar, int32_t index);
void archive_close_file(Archive *ar);

// Read up to `size` bytes into `data`. Returns bytes actually read.
int32_t archive_read_file(Archive *ar, void *data, int32_t size);

// Seek within the currently open file. `origin` is SEEK_SET, SEEK_CUR,
// or SEEK_END, all relative to the file's own offset range [0, length].
bool archive_seek_file(Archive *ar, int32_t offset, int origin);

// Current read position within the open file (0-based).
int32_t archive_get_loc_in_file(const Archive *ar);

#endif /* Q16_ARCHIVE_H */
