// ===========================================================================
// MemoryRegion — Block-based region allocator with free-list bins
// ===========================================================================
#ifndef Q16_MEMORY_REGION_H
#define Q16_MEMORY_REGION_H

#include <stdint.h>

// The backbone memory system. All Allocators and ChunkedArrays allocate from
// MemoryRegions. Two global instances: s_gameRegion (session lifetime) and
// s_levelRegion (cleared on level transitions).

typedef struct MemoryRegion MemoryRegion;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Create a region. block_size = bytes per block (max 16 MB).
// max_size = total capacity (0 = unlimited). First block allocated immediately.
MemoryRegion *region_create(const char *name, int32_t block_size, int32_t max_size);

// Reset all blocks to a single free allocation. Does NOT free blocks.
// Used on level transitions for s_levelRegion.
void region_clear(MemoryRegion *region);

// Free all blocks and the region struct itself.
void region_destroy(MemoryRegion *region);

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

// Allocate size bytes (8-byte aligned, minimum 24 bytes).
// Returns NULL if out of memory.
void *region_alloc(MemoryRegion *region, int32_t size);

// Free a previous allocation. Coalesces with adjacent free blocks.
void region_free(MemoryRegion *region, void *ptr);

// Resize an allocation. Tries in-place expansion first (merge with adjacent
// free block), falls back to alloc+copy+free.
void *region_realloc(MemoryRegion *region, void *ptr, int32_t new_size);

#endif /* Q16_MEMORY_REGION_H */
