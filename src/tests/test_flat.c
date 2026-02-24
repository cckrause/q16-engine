#include "test_harness.h"

#include "render/flat.h"
#include <string.h>

#define TEST_WIDTH  32
#define TEST_HEIGHT 200

void test_flat(void) {
  TEST_SUITE_BEGIN("flat");

  // 1. Init / destroy lifecycle
  {
    FlatState fs;
    memset(&fs, 0, sizeof(fs));

    bool ok = flat_init(&fs, TEST_WIDTH);
    TEST_CHECK("init succeeds", ok);
    TEST_CHECK("width set", fs.width == TEST_WIDTH);
    TEST_CHECK("column_top non-NULL", fs.column_top != NULL);
    TEST_CHECK("column_bot non-NULL", fs.column_bot != NULL);

    flat_destroy(&fs);
    TEST_CHECK("destroy: column_top NULL", fs.column_top == NULL);
    TEST_CHECK("destroy: column_bot NULL", fs.column_bot == NULL);
  }

  // 2. Reset sets full-screen defaults
  {
    FlatState fs;
    memset(&fs, 0, sizeof(fs));
    flat_init(&fs, TEST_WIDTH);
    flat_reset(&fs, TEST_HEIGHT);

    bool top_ok = true;
    bool bot_ok = true;
    for (int32_t x = 0; x < TEST_WIDTH; x++) {
      if (fs.column_top[x] != 0) {
        top_ok = false;
      }
      if (fs.column_bot[x] != TEST_HEIGHT - 1) {
        bot_ok = false;
      }
    }
    TEST_CHECK("reset: all column_top = 0", top_ok);
    TEST_CHECK("reset: all column_bot = height-1", bot_ok);

    flat_destroy(&fs);
  }

  // 3. flat_update_solid writes both top and bot
  {
    FlatState fs;
    memset(&fs, 0, sizeof(fs));
    flat_init(&fs, TEST_WIDTH);
    flat_reset(&fs, TEST_HEIGHT);

    int32_t ceil_y[5] = {20, 21, 22, 23, 24};
    int32_t floor_y[5] = {180, 179, 178, 177, 176};

    flat_update_solid(&fs, 10, 14, ceil_y, floor_y);

    TEST_CHECK("solid: top[10] = 20", fs.column_top[10] == 20);
    TEST_CHECK("solid: top[12] = 22", fs.column_top[12] == 22);
    TEST_CHECK("solid: top[14] = 24", fs.column_top[14] == 24);
    TEST_CHECK("solid: bot[10] = 180", fs.column_bot[10] == 180);
    TEST_CHECK("solid: bot[14] = 176", fs.column_bot[14] == 176);

    TEST_CHECK("solid: top[9] unchanged", fs.column_top[9] == 0);
    TEST_CHECK("solid: bot[9] unchanged", fs.column_bot[9] == TEST_HEIGHT - 1);
    TEST_CHECK("solid: top[15] unchanged", fs.column_top[15] == 0);
    TEST_CHECK("solid: bot[15] unchanged", fs.column_bot[15] == TEST_HEIGHT - 1);

    flat_destroy(&fs);
  }

  // 4. flat_update_top only narrows top downward
  {
    FlatState fs;
    memset(&fs, 0, sizeof(fs));
    flat_init(&fs, TEST_WIDTH);
    flat_reset(&fs, TEST_HEIGHT);

    int32_t ceil_y[3] = {30, 25, 40};
    flat_update_top(&fs, 5, 7, ceil_y);

    TEST_CHECK("top: col[5] narrowed to 30", fs.column_top[5] == 30);
    TEST_CHECK("top: col[6] narrowed to 25", fs.column_top[6] == 25);
    TEST_CHECK("top: col[7] narrowed to 40", fs.column_top[7] == 40);
    TEST_CHECK("top: bot[5] unchanged", fs.column_bot[5] == TEST_HEIGHT - 1);

    // Second call with smaller values — must NOT widen back.
    int32_t ceil_y2[3] = {10, 10, 10};
    flat_update_top(&fs, 5, 7, ceil_y2);

    TEST_CHECK("top: col[5] stays 30 (not widened)", fs.column_top[5] == 30);
    TEST_CHECK("top: col[6] stays 25 (not widened)", fs.column_top[6] == 25);
    TEST_CHECK("top: col[7] stays 40 (not widened)", fs.column_top[7] == 40);

    // Third call with larger values — narrows further.
    int32_t ceil_y3[3] = {50, 50, 50};
    flat_update_top(&fs, 5, 7, ceil_y3);

    TEST_CHECK("top: col[5] narrowed to 50", fs.column_top[5] == 50);
    TEST_CHECK("top: col[7] narrowed to 50", fs.column_top[7] == 50);

    flat_destroy(&fs);
  }

  // 5. flat_update_bot only narrows bot upward
  {
    FlatState fs;
    memset(&fs, 0, sizeof(fs));
    flat_init(&fs, TEST_WIDTH);
    flat_reset(&fs, TEST_HEIGHT);

    int32_t floor_y[3] = {170, 175, 160};
    flat_update_bot(&fs, 5, 7, floor_y);

    TEST_CHECK("bot: col[5] narrowed to 170", fs.column_bot[5] == 170);
    TEST_CHECK("bot: col[6] narrowed to 175", fs.column_bot[6] == 175);
    TEST_CHECK("bot: col[7] narrowed to 160", fs.column_bot[7] == 160);
    TEST_CHECK("bot: top[5] unchanged", fs.column_top[5] == 0);

    // Second call with larger values — must NOT widen back.
    int32_t floor_y2[3] = {190, 190, 190};
    flat_update_bot(&fs, 5, 7, floor_y2);

    TEST_CHECK("bot: col[5] stays 170 (not widened)", fs.column_bot[5] == 170);
    TEST_CHECK("bot: col[6] stays 175 (not widened)", fs.column_bot[6] == 175);
    TEST_CHECK("bot: col[7] stays 160 (not widened)", fs.column_bot[7] == 160);

    // Third call with smaller values — narrows further.
    int32_t floor_y3[3] = {150, 150, 150};
    flat_update_bot(&fs, 5, 7, floor_y3);

    TEST_CHECK("bot: col[5] narrowed to 150", fs.column_bot[5] == 150);
    TEST_CHECK("bot: col[7] narrowed to 150", fs.column_bot[7] == 150);

    flat_destroy(&fs);
  }

  TEST_SUITE_END();
}
