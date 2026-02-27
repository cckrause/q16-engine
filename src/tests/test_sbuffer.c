#include "test_harness.h"

#include "render/sbuffer.h"
#include <math.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

// Compute wall normal from cam-space endpoints: outward normal = (dz, -dx).
static void compute_wall_normal(float vx0, float vz0, float vx1, float vz1,
                                float *nx, float *nz, float *nd) {
  float dx = vx1 - vx0;
  float dz = vz1 - vz0;
  float len = sqrtf(dx * dx + dz * dz);
  if (len < 1e-8f) {
    *nx = 0.0f; *nz = 1.0f; *nd = vz0;
    return;
  }
  *nx = dz / len;
  *nz = -dx / len;
  *nd = (*nx) * vx0 + (*nz) * vz0;
}

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
  bool ok = sbuffer_init(&sb, 128);
  TEST_CHECK("sbuffer init", ok);

  sbuffer_reset(&sb);
  TEST_CHECK("empty after reset", sbuffer_first(&sb) == NULL);

  // Wall A: a wall at z=5, spanning x=[-1,1] (facing camera).
  {
    float vx0 = -1.0f, vz0 = 5.0f, vx1 = 1.0f, vz1 = 5.0f;
    float nx, nz, nd;
    compute_wall_normal(vx0, vz0, vx1, vz1, &nx, &nz, &nd);

    SBufferSeg *seg1 = sbuffer_insert(&sb, 1.0f, 2.0f,
                                      vx0, vz0, vx1, vz1, nx, nz, nd,
                                      0, false, NULL);
    TEST_CHECK("insert returns non-NULL", seg1 != NULL);
    TEST_CHECK("seg1 start", NEAR(seg1->start, 1.0f, 1e-5f));
    TEST_CHECK("seg1 end", NEAR(seg1->end, 2.0f, 1e-5f));

    const SBufferSeg *first = sbuffer_first(&sb);
    TEST_CHECK("first == seg1", first == seg1);
    TEST_CHECK("no second seg", sbuffer_next(&sb, first) == NULL);
  }

  // Insert a non-overlapping segment.
  {
    float vx0 = -1.0f, vz0 = 8.0f, vx1 = 1.0f, vz1 = 8.0f;
    float nx, nz, nd;
    compute_wall_normal(vx0, vz0, vx1, vz1, &nx, &nz, &nd);

    SBufferSeg *seg2 = sbuffer_insert(&sb, 2.5f, 3.5f,
                                      vx0, vz0, vx1, vz1, nx, nz, nd,
                                      1, false, NULL);
    TEST_CHECK("seg2 non-NULL", seg2 != NULL);

    const SBufferSeg *first = sbuffer_first(&sb);
    const SBufferSeg *second = sbuffer_next(&sb, first);
    TEST_CHECK("two segs: first exists", first != NULL);
    TEST_CHECK("two segs: second exists", second != NULL);
    TEST_CHECK("two segs: ordered", first->start < second->start);
  }

  // Overlapping segment — closer wall replaces middle of farther wall.
  {
    sbuffer_reset(&sb);

    float vx_back0 = -2.0f, vz_back0 = 10.0f, vx_back1 = 2.0f, vz_back1 = 10.0f;
    float nx_b, nz_b, nd_b;
    compute_wall_normal(vx_back0, vz_back0, vx_back1, vz_back1, &nx_b, &nz_b, &nd_b);

    sbuffer_insert(&sb, 1.0f, 3.0f,
                   vx_back0, vz_back0, vx_back1, vz_back1, nx_b, nz_b, nd_b,
                   10, false, NULL);

    float vx_front0 = -0.5f, vz_front0 = 3.0f, vx_front1 = 0.5f, vz_front1 = 3.0f;
    float nx_f, nz_f, nd_f;
    compute_wall_normal(vx_front0, vz_front0, vx_front1, vz_front1, &nx_f, &nz_f, &nd_f);

    SBufferSeg *front = sbuffer_insert(&sb, 1.5f, 2.5f,
                                       vx_front0, vz_front0, vx_front1, vz_front1,
                                       nx_f, nz_f, nd_f,
                                       20, false, NULL);
    TEST_CHECK("front seg non-NULL", front != NULL);

    int32_t count = 0;
    for (const SBufferSeg *s = sbuffer_first(&sb); s; s = sbuffer_next(&sb, s))
      count++;
    TEST_CHECK("overlap: 3 segments", count == 3);
  }

  // Sprite clip query.
  {
    sbuffer_reset(&sb);

    float vx0a = -1.0f, vz0a = 5.0f, vx1a = 1.0f, vz1a = 5.0f;
    float nxa, nza, nda;
    compute_wall_normal(vx0a, vz0a, vx1a, vz1a, &nxa, &nza, &nda);
    sbuffer_insert(&sb, 1.0f, 2.0f, vx0a, vz0a, vx1a, vz1a, nxa, nza, nda,
                   0, false, NULL);

    float vx0b = -1.0f, vz0b = 8.0f, vx1b = 1.0f, vz1b = 8.0f;
    float nxb, nzb, ndb;
    compute_wall_normal(vx0b, vz0b, vx1b, vz1b, &nxb, &nzb, &ndb);
    sbuffer_insert(&sb, 2.5f, 3.5f, vx0b, vz0b, vx1b, vz1b, nxb, nzb, ndb,
                   1, false, NULL);

    SBufferSpriteSpan spans[8];
    int32_t nspans = sbuffer_clip_sprite(&sb, 0.5f, 4.0f, spans, 8);
    TEST_CHECK("sprite clip: 2 spans", nspans == 2);
    if (nspans >= 2) {
      TEST_CHECK("span0 start ~1", NEAR(spans[0].start, 1.0f, 1e-5f));
      TEST_CHECK("span0 end ~2", NEAR(spans[0].end, 2.0f, 1e-5f));
      TEST_CHECK("span0 depth ~5", NEAR(spans[0].depth, 5.0f, 0.5f));
      TEST_CHECK("span1 start ~2.5", NEAR(spans[1].start, 2.5f, 1e-5f));
    }
  }

  // Degenerate segment is rejected.
  {
    sbuffer_reset(&sb);
    SBufferSeg *degen = sbuffer_insert(&sb, 1.0f, 1.0f,
                                       0.0f, 5.0f, 0.0f, 5.0f,
                                       0.0f, 1.0f, 5.0f,
                                       0, false, NULL);
    TEST_CHECK("degenerate rejected", degen == NULL);
  }

  // Crossing walls: two angled walls that intersect in XZ.
  // Wall A goes from (-3, 2) to (3, 8) — crosses from left-close to right-far.
  // Wall B goes from (3, 2) to (-3, 8) — crosses from right-close to left-far.
  // They cross around x=0, z=5.
  {
    sbuffer_reset(&sb);

    float ax0 = -3.0f, az0 = 2.0f, ax1 = 3.0f, az1 = 8.0f;
    float anx, anz, and_;
    compute_wall_normal(ax0, az0, ax1, az1, &anx, &anz, &and_);

    float bx0 = 3.0f, bz0 = 2.0f, bx1 = -3.0f, bz1 = 8.0f;
    float bnx, bnz, bnd;
    compute_wall_normal(bx0, bz0, bx1, bz1, &bnx, &bnz, &bnd);

    float pa0 = sbuffer_project(ax0, az0);
    float pa1 = sbuffer_project(ax1, az1);
    float pb0 = sbuffer_project(bx0, bz0);
    float pb1 = sbuffer_project(bx1, bz1);

    sbuffer_insert(&sb, pa0, pa1, ax0, az0, ax1, az1, anx, anz, and_,
                   100, false, NULL);
    sbuffer_insert(&sb, pb0, pb1, bx0, bz0, bx1, bz1, bnx, bnz, bnd,
                   200, false, NULL);

    // After insertion, both walls should be partially visible (split at crossing).
    int32_t count = 0;
    bool found_100 = false, found_200 = false;
    for (const SBufferSeg *s = sbuffer_first(&sb); s; s = sbuffer_next(&sb, s)) {
      count++;
      if (s->wall_id == 100) found_100 = true;
      if (s->wall_id == 200) found_200 = true;
    }
    TEST_CHECK("crossing: at least 2 segments", count >= 2);
    TEST_CHECK("crossing: wall A visible", found_100);
    TEST_CHECK("crossing: wall B visible", found_200);
  }

  sbuffer_destroy(&sb);
  TEST_SUITE_END();
}
