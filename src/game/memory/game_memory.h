// ===========================================================================
// Global Memory Management
// ===========================================================================
#ifndef Q16_GAME_MEMORY_H
#define Q16_GAME_MEMORY_H

#include "memory/memory_region.h"
#include <stdint.h>

// Two primary MemoryRegions serve all allocations:
// - s_gameRegion:  persists for the entire game session (sounds, tasks, globals).
// - s_levelRegion: cleared via region_clear on level transitions (sectors, walls,
//                  objects, INF state).
//
// Convenience wrappers provide the standard allocation vocabulary used
// throughout the Jedi Engine.

// Initialize both global regions. Call once at startup.
void game_memory_init(void);

// Shut down and free both regions. Call at program exit.
void game_memory_shutdown(void);

// Get handles to the two global regions.
MemoryRegion *game_get_game_region(void);
MemoryRegion *game_get_level_region(void);

// Reset level region for a level transition. All level allocations become
// invalid after this call.
void game_level_clear(void);

// ---------------------------------------------------------------------------
// Convenience wrappers
// ---------------------------------------------------------------------------

// Allocate from s_levelRegion. Freed automatically on level transition.
void *level_alloc(int32_t size);

// Realloc within s_levelRegion. Used by sector object list growth.
void *level_realloc(void *ptr, int32_t size);

// Heap allocation (malloc). NOT tied to any MemoryRegion.
// Callers must explicitly free with game_free.
void *game_alloc(int32_t size);

// Free memory allocated via game_alloc.
void game_free(void *ptr);

#endif /* Q16_GAME_MEMORY_H */
