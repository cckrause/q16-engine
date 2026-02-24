#include "test_harness.h"
#include "memory/game_memory.h"

#include <string.h>

void test_game_memory(void) {
  TEST_SUITE_BEGIN("game memory wrappers");

  game_memory_init();

  // regions exist
  {
    MemoryRegion *gr = game_get_game_region();
    MemoryRegion *lr = game_get_level_region();
    TEST_CHECK("game region exists", gr != NULL);
    TEST_CHECK("level region exists", lr != NULL);
    TEST_CHECK("regions differ", gr != lr);
  }

  // level_alloc / level_realloc
  {
    void *ptr = level_alloc(128);
    TEST_CHECK("level_alloc returns non-NULL", ptr != NULL);

    void *ptr2 = level_realloc(ptr, 256);
    TEST_CHECK("level_realloc returns non-NULL", ptr2 != NULL);
  }

  // game_alloc / game_free (heap)
  {
    void *ptr = game_alloc(64);
    TEST_CHECK("game_alloc returns non-NULL", ptr != NULL);

    // game_alloc zeroes memory.
    uint8_t sum = 0;
    for (int i = 0; i < 64; i++) {
      sum |= ((uint8_t *)ptr)[i];
    }
    TEST_CHECK("game_alloc zeroed", sum == 0);

    game_free(ptr);
  }

  // level_clear resets level region
  {
    void *before = level_alloc(64);
    TEST_CHECK("pre-clear alloc", before != NULL);

    game_level_clear();

    // After clear, level allocations are invalid — but we can allocate again.
    void *after = level_alloc(64);
    TEST_CHECK("post-clear alloc", after != NULL);
  }

  game_memory_shutdown();

  // shutdown nulls regions
  TEST_CHECK("shutdown game NULL", game_get_game_region() == NULL);
  TEST_CHECK("shutdown level NULL", game_get_level_region() == NULL);

  TEST_SUITE_END();
}
