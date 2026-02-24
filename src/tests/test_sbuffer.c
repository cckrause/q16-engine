#include "test_harness.h"

#include "render/sbuffer.h"
#include <math.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_sbuffer(void) {
  TEST_SUITE_BEGIN("sbuffer");

  // Unit-square projection: cardinal directions.
  {
    float p_fwd = sbuffer_project(0.0f, 1.0f);
    TEST_CHECK("proj +Z ~1.5", NEAR(p_fwd, 1.5f, 0.01f));

    float p_right = sbuffer_project(1.0f, 0.0f);
    TEST_CHECK("proj +X ~2.5", NEAR(p_right, 2.5f, 0.01f));

    float p_back = sbuffer_project(0.0f, -1.0f);
    TEST_CHECK("proj -Z ~3.5", NEAR(p_back, 3.5f, 0.01f));

    float p_left = sbuffer_project(-1.0f, 0.0f);
    TEST_CHECK("proj -X ~0.5", NEAR(p_left, 0.5f, 0.01f));
  }

  // Init and basic insert.
  SBuffer sb;
  bool ok = sbuffer_init(&sb, 64);
  TEST_CHECK("sbuffer init", ok);

  sbuffer_reset(&sb);
  TEST_CHECK("empty after reset", sbuffer_first(&sb) == NULL);

  // Insert a single segment.
  SBufferSeg *seg1 = sbuffer_insert(&sb, 1.0f, 2.0f, 0.0f, 1.0f, -5.0f,
                                    0, false, NULL);
  TEST_CHECK("insert returns non-NULL", seg1 != NULL);
  TEST_CHECK("seg1 start", NEAR(seg1->start, 1.0f, 1e-5f));
  TEST_CHECK("seg1 end", NEAR(seg1->end, 2.0f, 1e-5f));

  const SBufferSeg *first = sbuffer_first(&sb);
  TEST_CHECK("first == seg1", first == seg1);
  TEST_CHECK("no second seg", sbuffer_next(&sb, first) == NULL);

  // Insert a non-overlapping segment.
  SBufferSeg *seg2 = sbuffer_insert(&sb, 2.5f, 3.5f, 0.0f, 1.0f, -8.0f,
                                    1, false, NULL);
  TEST_CHECK("seg2 non-NULL", seg2 != NULL);

  // Should now have two segments in order.
  first = sbuffer_first(&sb);
  const SBufferSeg *second = sbuffer_next(&sb, first);
  TEST_CHECK("two segs: first exists", first != NULL);
  TEST_CHECK("two segs: second exists", second != NULL);
  TEST_CHECK("two segs: ordered", first->start < second->start);

  // Insert an overlapping segment that is in front (more negative normal_d).
  sbuffer_reset(&sb);
  sbuffer_insert(&sb, 1.0f, 3.0f, 0.0f, 1.0f, -5.0f, 10, false, NULL);
  SBufferSeg *front = sbuffer_insert(&sb, 1.5f, 2.5f, 0.0f, 1.0f, -10.0f,
                                     20, false, NULL);
  TEST_CHECK("front seg non-NULL", front != NULL);

  // The front segment should have occluded the middle of the back segment.
  // Iterate and count segments.
  int32_t count = 0;
  for (const SBufferSeg *s = sbuffer_first(&sb); s; s = sbuffer_next(&sb, s))
    count++;
  TEST_CHECK("overlap: 3 segments", count == 3);

  // Sprite clip query.
  sbuffer_reset(&sb);
  sbuffer_insert(&sb, 1.0f, 2.0f, 0.0f, 1.0f, -5.0f, 0, false, NULL);
  sbuffer_insert(&sb, 2.5f, 3.5f, 0.0f, 1.0f, -8.0f, 1, false, NULL);

  SBufferSpriteSpan spans[8];
  int32_t nspans = sbuffer_clip_sprite(&sb, 0.5f, 4.0f, spans, 8);
  TEST_CHECK("sprite clip: 2 spans", nspans == 2);
  if (nspans >= 2) {
    TEST_CHECK("span0 start ~1", NEAR(spans[0].start, 1.0f, 1e-5f));
    TEST_CHECK("span0 end ~2", NEAR(spans[0].end, 2.0f, 1e-5f));
    TEST_CHECK("span1 start ~2.5", NEAR(spans[1].start, 2.5f, 1e-5f));
  }

  // Degenerate segment is rejected.
  sbuffer_reset(&sb);
  SBufferSeg *degen = sbuffer_insert(&sb, 1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
                                     0, false, NULL);
  TEST_CHECK("degenerate rejected", degen == NULL);

  sbuffer_destroy(&sb);
  TEST_SUITE_END();
}
