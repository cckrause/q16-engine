#include "test_harness.h"

#include "render/adjoin.h"
#include <math.h>
#include <string.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_adjoin(void) {
  TEST_SUITE_BEGIN("adjoin");

  // AdjoinList basic operations.
  AdjoinList al;
  adjoin_list_reset(&al);
  TEST_CHECK("empty after reset", al.count == 0);

  EdgePair edge;
  memset(&edge, 0, sizeof(edge));
  edge.x0 = 100;
  edge.x1 = 200;

  bool added = adjoin_list_add(&al, NULL, &edge, NULL);
  TEST_CHECK("add succeeds", added);
  TEST_CHECK("count = 1", al.count == 1);
  TEST_CHECK("entry x0", al.entries[0].edge_pair.x0 == 100);
  TEST_CHECK("entry x1", al.entries[0].edge_pair.x1 == 200);

  // State save/restore.
  AdjoinSaveState save;
  adjoin_save_state(&save, 10, 310, 5, 195, 15.0f, 13.125f);
  TEST_CHECK("saved min_x", save.window_min_x == 10);
  TEST_CHECK("saved max_x", save.window_max_x == 310);
  TEST_CHECK("saved ambient", NEAR(save.sector_ambient, 15.0f, 1e-5f));

  int32_t rx, ry, rmx, rmy;
  float ra, rsa;
  adjoin_restore_state(&save, &rx, &rmx, &ry, &rmy, &ra, &rsa);
  TEST_CHECK("restored min_x", rx == 10);
  TEST_CHECK("restored max_x", rmx == 310);
  TEST_CHECK("restored ambient", NEAR(ra, 15.0f, 1e-5f));
  TEST_CHECK("restored scaled", NEAR(rsa, 13.125f, 1e-3f));

  // EdgePair computation.
  {
    WallSegment seg;
    memset(&seg, 0, sizeof(seg));
    seg.wall_x0 = 100;
    seg.wall_x1 = 200;
    seg.z0 = 10.0f;
    seg.z1 = 10.0f;

    EdgePair ep;
    adjoin_compute_edge_pair(100.0f, 100.0f, 0.0f,
                             0.0f, -10.0f, -2.0f, -8.0f,
                             &seg, &ep);
    TEST_CHECK("ep x0", ep.x0 == 100);
    TEST_CHECK("ep x1", ep.x1 == 200);
    // Ceiling and floor should produce valid Y values.
    TEST_CHECK("ceil y finite", isfinite(ep.y_ceil0));
    TEST_CHECK("floor y finite", isfinite(ep.y_floor0));
    TEST_CHECK("ceil above floor", ep.y_ceil0 < ep.y_floor0);
  }

  TEST_SUITE_END();
}
