// ===========================================================================
// Global Memory Management
// ===========================================================================
// Two MemoryRegions: game (session lifetime) and level (per-level lifetime).

#include "memory/game_memory.h"

#include <stdlib.h>
#include <string.h>

// Module state

// Block size: 1 MB. Max: 16 blocks = 16 MB per region.
#define GAME_REGION_BLOCK_SIZE  (1 * 1024 * 1024)
#define GAME_REGION_MAX_SIZE    (16 * 1024 * 1024)
#define LEVEL_REGION_BLOCK_SIZE (1 * 1024 * 1024)
#define LEVEL_REGION_MAX_SIZE   (16 * 1024 * 1024)

static MemoryRegion *s_game_region = NULL;
static MemoryRegion *s_level_region = NULL;

// Lifecycle

void game_memory_init(void) {
  if (!s_game_region) {
    s_game_region = region_create("game", GAME_REGION_BLOCK_SIZE, GAME_REGION_MAX_SIZE);
  }
  if (!s_level_region) {
    s_level_region =
        region_create("level", LEVEL_REGION_BLOCK_SIZE, LEVEL_REGION_MAX_SIZE);
  }
}

void game_memory_shutdown(void) {
  if (s_level_region) {
    region_destroy(s_level_region);
    s_level_region = NULL;
  }
  if (s_game_region) {
    region_destroy(s_game_region);
    s_game_region = NULL;
  }
}

MemoryRegion *game_get_game_region(void) {
  return s_game_region;
}

MemoryRegion *game_get_level_region(void) {
  return s_level_region;
}

void game_level_clear(void) {
  if (s_level_region) {
    region_clear(s_level_region);
  }
}

// Convenience wrappers

void *level_alloc(int32_t size) {
  return s_level_region ? region_alloc(s_level_region, size) : NULL;
}

void *level_realloc(void *ptr, int32_t size) {
  return s_level_region ? region_realloc(s_level_region, ptr, size) : NULL;
}

void *game_alloc(int32_t size) {
  if (size <= 0)
    return NULL;
  void *ptr = malloc((size_t)size);
  if (ptr) {
    memset(ptr, 0, (size_t)size);
  }
  return ptr;
}

void game_free(void *ptr) {
  free(ptr);
}
