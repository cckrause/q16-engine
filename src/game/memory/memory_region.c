// ===========================================================================
// MemoryRegion — Block-based Region Allocator
// ===========================================================================
// Free-list binned allocator backed by growable OS blocks.
// All Allocators and ChunkedArrays allocate from MemoryRegions.

#include "memory/memory_region.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define REGION_ALIGNMENT      8
#define REGION_MIN_SPLIT_SIZE 32
#define REGION_BIN_COUNT      6
#define REGION_BLOCK_ARR_STEP 16
#define REGION_MAX_BLOCK_SIZE (16 * 1024 * 1024)

// Internal structures

// Header prepended to every allocation (both active and free).
// size = total bytes including this header, aligned to REGION_ALIGNMENT.
typedef struct RegionAllocHeader {
  int32_t size;
  int32_t free; // 0 = allocated, 1 = free
} RegionAllocHeader;

// Extended header for free blocks — adds doubly-linked list pointers
// for the free-list bins.
typedef struct AllocHeaderFree {
  int32_t size;
  int32_t free;
  struct AllocHeaderFree *next;
  struct AllocHeaderFree *prev;
} AllocHeaderFree;

// A contiguous memory block within the region.
typedef struct MemoryBlock {
  uint8_t *data;
  int32_t data_size; // total block capacity
  int32_t size_free; // total free bytes
  int32_t count;     // number of active allocations
  AllocHeaderFree *free_bins[REGION_BIN_COUNT];
} MemoryBlock;

// The region itself.
struct MemoryRegion {
  char name[32];
  MemoryBlock **blocks;
  int32_t block_arr_capacity;
  int32_t block_count;
  int32_t block_size;
  int32_t max_blocks; // 0 = unlimited
};

// Helpers

static int32_t align_size(int32_t size) {
  return (size + REGION_ALIGNMENT - 1) & ~(REGION_ALIGNMENT - 1);
}

// Minimum allocation = size of AllocHeaderFree so a freed block can hold
// the free-list pointers.
static int32_t min_alloc_size(void) {
  int32_t s = (int32_t)sizeof(AllocHeaderFree);
  return align_size(s);
}

// Determine which free-list bin a given size falls into.
// Bin 0: 0–32, Bin 1: 33–64, Bin 2: 65–128, Bin 3: 129–256,
// Bin 4: 257–512, Bin 5: 513+.
static int32_t get_bin_index(int32_t size) {
  if (size <= 32)
    return 0;
  if (size <= 64)
    return 1;
  if (size <= 128)
    return 2;
  if (size <= 256)
    return 3;
  if (size <= 512)
    return 4;
  return 5;
}

// Free-list operations

static void bin_insert(MemoryBlock *block, AllocHeaderFree *hdr) {
  int32_t bin = get_bin_index(hdr->size);
  hdr->prev = NULL;
  hdr->next = block->free_bins[bin];
  if (block->free_bins[bin]) {
    block->free_bins[bin]->prev = hdr;
  }
  block->free_bins[bin] = hdr;
}

static void bin_remove(MemoryBlock *block, AllocHeaderFree *hdr) {
  int32_t bin = get_bin_index(hdr->size);
  if (hdr->prev) {
    hdr->prev->next = hdr->next;
  } else {
    block->free_bins[bin] = hdr->next;
  }
  if (hdr->next) {
    hdr->next->prev = hdr->prev;
  }
  hdr->next = NULL;
  hdr->prev = NULL;
}

// Block management

static MemoryBlock *block_create(int32_t block_size) {
  MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
  if (!block)
    return NULL;

  block->data = (uint8_t *)malloc((size_t)block_size);
  if (!block->data) {
    free(block);
    return NULL;
  }

  memset(block->data, 0, (size_t)block_size);
  block->data_size = block_size;
  block->size_free = block_size;
  block->count = 0;
  memset(block->free_bins, 0, sizeof(block->free_bins));

  // Initialize the entire block as one free entry.
  AllocHeaderFree *hdr = (AllocHeaderFree *)block->data;
  hdr->size = block_size;
  hdr->free = 1;
  hdr->next = NULL;
  hdr->prev = NULL;
  bin_insert(block, hdr);

  return block;
}

static void block_clear(MemoryBlock *block) {
  memset(block->data, 0, (size_t)block->data_size);
  block->size_free = block->data_size;
  block->count = 0;
  memset(block->free_bins, 0, sizeof(block->free_bins));

  AllocHeaderFree *hdr = (AllocHeaderFree *)block->data;
  hdr->size = block->data_size;
  hdr->free = 1;
  hdr->next = NULL;
  hdr->prev = NULL;
  bin_insert(block, hdr);
}

static void block_destroy(MemoryBlock *block) {
  if (block) {
    free(block->data);
    free(block);
  }
}

// Check if a pointer falls within a block's data range.
static int block_contains(MemoryBlock *block, void *ptr) {
  uint8_t *p = (uint8_t *)ptr;
  return p >= block->data && p < block->data + block->data_size;
}

// Find the block containing a given user pointer.
static MemoryBlock *find_block(MemoryRegion *region, void *ptr) {
  for (int32_t i = 0; i < region->block_count; i++) {
    if (block_contains(region->blocks[i], ptr)) {
      return region->blocks[i];
    }
  }
  return NULL;
}

// Ensure the blocks array has room for one more block.
static int region_grow_block_array(MemoryRegion *region) {
  if (region->block_count < region->block_arr_capacity)
    return 1;

  int32_t new_cap = region->block_arr_capacity + REGION_BLOCK_ARR_STEP;
  MemoryBlock **new_arr =
      (MemoryBlock **)realloc(region->blocks, (size_t)new_cap * sizeof(MemoryBlock *));
  if (!new_arr)
    return 0;

  region->blocks = new_arr;
  region->block_arr_capacity = new_cap;
  return 1;
}

static int region_add_block(MemoryRegion *region) {
  if (region->max_blocks > 0 && region->block_count >= region->max_blocks) {
    return 0;
  }
  if (!region_grow_block_array(region))
    return 0;

  MemoryBlock *block = block_create(region->block_size);
  if (!block)
    return 0;

  region->blocks[region->block_count] = block;
  region->block_count++;
  return 1;
}

// Public API

MemoryRegion *region_create(const char *name, int32_t block_size, int32_t max_size) {
  if (block_size > REGION_MAX_BLOCK_SIZE) {
    block_size = REGION_MAX_BLOCK_SIZE;
  }

  MemoryRegion *region = (MemoryRegion *)malloc(sizeof(MemoryRegion));
  if (!region)
    return NULL;

  memset(region, 0, sizeof(MemoryRegion));
  if (name) {
    strncpy(region->name, name, 31);
    region->name[31] = '\0';
  }

  region->block_size = block_size;
  region->max_blocks = (max_size > 0) ? (max_size / block_size) : 0;

  // Allocate initial block array and first block.
  region->block_arr_capacity = REGION_BLOCK_ARR_STEP;
  region->blocks =
      (MemoryBlock **)calloc((size_t)region->block_arr_capacity, sizeof(MemoryBlock *));
  if (!region->blocks) {
    free(region);
    return NULL;
  }

  if (!region_add_block(region)) {
    free(region->blocks);
    free(region);
    return NULL;
  }

  return region;
}

void region_clear(MemoryRegion *region) {
  if (!region)
    return;

  // Keep the first block, free the rest.
  for (int32_t i = 1; i < region->block_count; i++) {
    block_destroy(region->blocks[i]);
    region->blocks[i] = NULL;
  }
  region->block_count = 1;

  // Reset the first block to one big free entry.
  block_clear(region->blocks[0]);
}

void region_destroy(MemoryRegion *region) {
  if (!region)
    return;

  for (int32_t i = 0; i < region->block_count; i++) {
    block_destroy(region->blocks[i]);
  }
  free(region->blocks);
  free(region);
}

void *region_alloc(MemoryRegion *region, int32_t size) {
  if (!region || size <= 0)
    return NULL;

  // Add header size and align. Minimum = sizeof(AllocHeaderFree).
  int32_t total = align_size(size + (int32_t)sizeof(RegionAllocHeader));
  if (total < min_alloc_size()) {
    total = min_alloc_size();
  }
  if (total > region->block_size)
    return NULL;

  // Search blocks for a fit.
  for (int attempt = 0; attempt < 2; attempt++) {
    for (int32_t bi = 0; bi < region->block_count; bi++) {
      MemoryBlock *block = region->blocks[bi];
      if (block->size_free < total)
        continue;

      // Search bins starting from the appropriate bin, ascending.
      int32_t start_bin = get_bin_index(total);
      for (int32_t bin = start_bin; bin < REGION_BIN_COUNT; bin++) {
        AllocHeaderFree *hdr = block->free_bins[bin];
        while (hdr) {
          if (hdr->size >= total) {
            // Found a fit. Remove from free list.
            bin_remove(block, hdr);

            // Split if remainder is large enough.
            if (hdr->size - total >= REGION_MIN_SPLIT_SIZE) {
              AllocHeaderFree *remainder = (AllocHeaderFree *)((uint8_t *)hdr + total);
              remainder->size = hdr->size - total;
              remainder->free = 1;
              remainder->next = NULL;
              remainder->prev = NULL;
              bin_insert(block, remainder);
              hdr->size = total;
            }

            hdr->free = 0;
            block->size_free -= hdr->size;
            block->count++;

            // Return pointer past the header.
            return (uint8_t *)hdr + sizeof(RegionAllocHeader);
          }
          hdr = hdr->next;
        }
      }
    }

    // No fit found — try adding a new block (only on first attempt).
    if (attempt == 0) {
      if (!region_add_block(region))
        return NULL;
    }
  }

  return NULL;
}

void region_free(MemoryRegion *region, void *ptr) {
  if (!region || !ptr)
    return;

  RegionAllocHeader *hdr =
      (RegionAllocHeader *)((uint8_t *)ptr - sizeof(RegionAllocHeader));

  MemoryBlock *block = find_block(region, hdr);
  if (!block)
    return;

  // Save original size — only this portion is transitioning from
  // "allocated" to "free".  The coalesced neighbour was already free.
  int32_t freed_size = hdr->size;

  // Coalesce with the next block if it is free and within the block.
  RegionAllocHeader *next_hdr = (RegionAllocHeader *)((uint8_t *)hdr + hdr->size);
  uint8_t *block_end = block->data + block->data_size;

  if ((uint8_t *)next_hdr < block_end && next_hdr->free) {
    bin_remove(block, (AllocHeaderFree *)next_hdr);
    hdr->size += next_hdr->size;
  }

  // Mark as free and insert into the appropriate bin.
  hdr->free = 1;
  block->size_free += freed_size;
  block->count--;

  AllocHeaderFree *free_hdr = (AllocHeaderFree *)hdr;
  free_hdr->next = NULL;
  free_hdr->prev = NULL;
  bin_insert(block, free_hdr);
}

void *region_realloc(MemoryRegion *region, void *ptr, int32_t new_size) {
  if (!region)
    return NULL;
  if (!ptr)
    return region_alloc(region, new_size);

  RegionAllocHeader *hdr =
      (RegionAllocHeader *)((uint8_t *)ptr - sizeof(RegionAllocHeader));

  int32_t aligned_new = align_size(new_size + (int32_t)sizeof(RegionAllocHeader));
  if (aligned_new < min_alloc_size()) {
    aligned_new = min_alloc_size();
  }

  // Already large enough?
  if (hdr->size >= aligned_new)
    return ptr;

  // Try in-place expansion: check if the next header is free and combined
  // size is large enough.
  MemoryBlock *block = find_block(region, hdr);
  if (block) {
    RegionAllocHeader *next_hdr = (RegionAllocHeader *)((uint8_t *)hdr + hdr->size);
    uint8_t *block_end = block->data + block->data_size;

    if ((uint8_t *)next_hdr < block_end && next_hdr->free &&
        hdr->size + next_hdr->size >= aligned_new) {
      bin_remove(block, (AllocHeaderFree *)next_hdr);

      int32_t combined = hdr->size + next_hdr->size;

      // Split remainder back to free list if large enough.
      if (combined - aligned_new >= REGION_MIN_SPLIT_SIZE) {
        AllocHeaderFree *remainder = (AllocHeaderFree *)((uint8_t *)hdr + aligned_new);
        remainder->size = combined - aligned_new;
        remainder->free = 1;
        remainder->next = NULL;
        remainder->prev = NULL;
        bin_insert(block, remainder);
        hdr->size = aligned_new;
        block->size_free -= (aligned_new - (combined - next_hdr->size));
      } else {
        hdr->size = combined;
        block->size_free -= next_hdr->size;
      }

      return ptr;
    }
  }

  // Fall back to allocate + copy + free.
  int32_t old_data_size = hdr->size - (int32_t)sizeof(RegionAllocHeader);
  void *new_ptr = region_alloc(region, new_size);
  if (!new_ptr)
    return NULL;

  int32_t copy_size = old_data_size < new_size ? old_data_size : new_size;
  memcpy(new_ptr, ptr, (size_t)copy_size);
  region_free(region, ptr);
  return new_ptr;
}
