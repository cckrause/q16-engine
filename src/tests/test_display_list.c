#include "test_harness.h"

#include "render/display_list.h"
#include <string.h>

void test_display_list(void) {
  TEST_SUITE_BEGIN("display list");

  // Bit-packing helpers.
  {
    uint32_t flags =
        display_list_pack_flags(3, false, false, true, true, false, false, 0x3FFFFF);
    TEST_CHECK("part_id = 3", (flags & 0x0F) == 3);
    TEST_CHECK("fullbright set", (flags & (1u << 6)) != 0);
    TEST_CHECK("opaque set", (flags & (1u << 7)) != 0);
    TEST_CHECK("no stretch_top", (flags & (1u << 4)) == 0);
    TEST_CHECK("next_sector = none", ((flags >> 10) & 0x3FFFFF) == 0x3FFFFF);
  }

  {
    uint32_t light = display_list_pack_light(5, true, 0, 0);
    TEST_CHECK("wall_light = 37", (light & 0x3F) == 37);
    TEST_CHECK("flip set", (light & (1u << 6)) != 0);
  }

  {
    uint32_t wt = display_list_pack_wall_tex(42, 100);
    TEST_CHECK("wall_id = 42", (wt & 0xFFFF) == 42);
    TEST_CHECK("tex_id = 100", ((wt >> 16) & 0xFFFF) == 100);
  }

  // Init, add, reset.
  DisplayList dl;
  bool ok = display_list_init(&dl, 128, 32);
  TEST_CHECK("init succeeds", ok);
  TEST_CHECK("opaque empty", dl.opaque_count == 0);
  TEST_CHECK("transparent empty", dl.transparent_count == 0);

  DisplayListEntry entry;
  memset(&entry, 0, sizeof(entry));
  entry.pos.v0x = 1.0f;
  entry.data.flags_part =
      display_list_pack_flags(0, false, false, false, true, false, false, 0);
  bool added_op = display_list_add_opaque(&dl, &entry);
  TEST_CHECK("add opaque", added_op);
  TEST_CHECK("opaque count = 1", dl.opaque_count == 1);

  bool added_tr = display_list_add_transparent(&dl, &entry);
  TEST_CHECK("add transparent", added_tr);
  TEST_CHECK("transparent count = 1", dl.transparent_count == 1);

  int32_t plane_idx = display_list_add_plane(&dl, 1.0f, 0.0f, 0.0f, 5.0f);
  TEST_CHECK("plane idx = 0", plane_idx == 0);
  TEST_CHECK("plane count = 1", dl.plane_count == 1);

  display_list_reset(&dl);
  TEST_CHECK("reset opaque = 0", dl.opaque_count == 0);
  TEST_CHECK("reset transparent = 0", dl.transparent_count == 0);
  TEST_CHECK("reset planes = 0", dl.plane_count == 0);

  display_list_destroy(&dl);
  TEST_SUITE_END();
}
