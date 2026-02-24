/*
 * Unit tests for the MemoryRegion block-based allocator.
 */

#include "test_harness.h"

#include "memory/memory_region.h"

void test_memory_region(void) {
  TEST_SUITE_BEGIN("MemoryRegion");

  // Create / destroy
  MemoryRegion *r = region_create("test", 64 * 1024, 256 * 1024);
  TEST_CHECK("region_create returns non-NULL", r != NULL);

  // Basic allocation
  void *a = region_alloc(r, 100);
  TEST_CHECK("region_alloc returns non-NULL", a != NULL);

  void *b = region_alloc(r, 200);
  TEST_CHECK("second alloc returns non-NULL", b != NULL);
  TEST_CHECK("different allocations have different addresses", a != b);

  // Free and reuse
  region_free(r, a);
  void *c = region_alloc(r, 80);
  TEST_CHECK("alloc after free succeeds", c != NULL);

  // Zeroed memory
  void *d = region_alloc(r, 32);
  TEST_CHECK("alloc for zero-check succeeds", d != NULL);

  // Realloc in-place (if adjacent free block exists)
  region_free(r, b);
  region_free(r, c);
  void *e = region_alloc(r, 64);
  void *f = region_realloc(r, e, 128);
  TEST_CHECK("region_realloc returns non-NULL", f != NULL);

  // Clear
  region_clear(r);
  void *g = region_alloc(r, 100);
  TEST_CHECK("alloc after clear works", g != NULL);

  // Large allocation up to block size
  region_clear(r);
  void *big = region_alloc(r, 60000);
  TEST_CHECK("large alloc (60 KB in 64 KB block) succeeds", big != NULL);

  // Allocation exceeding block size fails
  void *too_big = region_alloc(r, 65 * 1024);
  TEST_CHECK("alloc exceeding block size returns NULL", too_big == NULL);

  // Multiple small allocations
  region_clear(r);
  int alloc_ok = 1;
  for (int i = 0; i < 100; i++) {
    if (!region_alloc(r, 32)) {
      alloc_ok = 0;
      break;
    }
  }
  TEST_CHECK("100 x 32-byte allocs succeed", alloc_ok);

  region_destroy(r);
  TEST_SUITE_END();
}
