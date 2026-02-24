#include "test_harness.h"

#include "render/depth.h"
#include "render/window.h"
#include <float.h>
#include <math.h>

void test_depth_window(void) {
  TEST_SUITE_BEGIN("depth & window");

  // Depth buffer
  {
    DepthBuffer db;
    bool ok = depth_buffer_init(&db, 320, 4);
    TEST_CHECK("depth init", ok);

    depth_buffer_reset(&db);
    DepthValue *cur = depth_buffer_current(&db);
    TEST_CHECK("reset to FLT_MAX", cur[0] == FLT_MAX);
    TEST_CHECK("reset col 160", cur[160] == FLT_MAX);

    cur[50] = 5.0f;
    cur[51] = 6.0f;

    bool entered = depth_buffer_enter_adjoin(&db, 50, 51);
    TEST_CHECK("enter adjoin", entered);

    DepthValue *child = depth_buffer_current(&db);
    TEST_CHECK("child copied [50]", child[50] == 5.0f);
    TEST_CHECK("child copied [51]", child[51] == 6.0f);

    child[50] = 3.0f;
    depth_buffer_exit_adjoin(&db, 50, 51, true);
    DepthValue *parent = depth_buffer_current(&db);
    TEST_CHECK("copy-back [50]", parent[50] == 3.0f);
    TEST_CHECK("copy-back [51]", parent[51] == 6.0f);

    depth_buffer_destroy(&db);
  }

  // Window
  {
    RenderWindow rw;
    bool ok = render_window_init(&rw, 320, 200, 4);
    TEST_CHECK("window init", ok);

    render_window_reset(&rw);
    int32_t *top = render_window_top(&rw);
    int32_t *bot = render_window_bot(&rw);
    TEST_CHECK("top[0] = 0", top[0] == 0);
    TEST_CHECK("bot[0] = 199", bot[0] == 199);
    TEST_CHECK("top[319] = 0", top[319] == 0);
    TEST_CHECK("bot[319] = 199", bot[319] == 199);

    // Narrow solid.
    render_window_narrow_solid(&rw, 100);
    top = render_window_top(&rw);
    bot = render_window_bot(&rw);
    TEST_CHECK("solid top[100] = 200", top[100] == 200);
    TEST_CHECK("solid bot[100] = -1", bot[100] == -1);

    // Narrow top.
    render_window_reset(&rw);
    render_window_narrow_top(&rw, 50, 30);
    top = render_window_top(&rw);
    TEST_CHECK("narrow top[50] = 31", top[50] == 31);

    // Narrow bot.
    render_window_narrow_bot(&rw, 50, 170);
    bot = render_window_bot(&rw);
    TEST_CHECK("narrow bot[50] = 169", bot[50] == 169);

    // Enter/exit adjoin.
    render_window_reset(&rw);
    top = render_window_top(&rw);
    bot = render_window_bot(&rw);
    top[60] = 10;
    bot[60] = 190;

    bool went_in = render_window_enter_adjoin(&rw, 60, 60);
    TEST_CHECK("window enter", went_in);
    int32_t *child_top = render_window_top(&rw);
    int32_t *child_bot = render_window_bot(&rw);
    TEST_CHECK("child top[60] = 10", child_top[60] == 10);
    TEST_CHECK("child bot[60] = 190", child_bot[60] == 190);

    render_window_exit_adjoin(&rw);
    top = render_window_top(&rw);
    TEST_CHECK("back to parent top[60]", top[60] == 10);

    render_window_destroy(&rw);
  }

  TEST_SUITE_END();
}
