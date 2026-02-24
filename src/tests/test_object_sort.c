#include "test_harness.h"

#include "render/object_sort.h"
#include "world/flags.h"
#include "world/object.h"
#include <string.h>

void test_object_sort(void) {
  TEST_SUITE_BEGIN("object sort");

  // Sprite angle: camera and object both facing +Z (yaw=0).
  {
    int32_t angle = object_sort_sprite_angle(0, 0);
    TEST_CHECK("same yaw angle = 31", angle == 31);
  }

  // Camera behind object: 180-degree difference.
  {
    int32_t angle = object_sort_sprite_angle(ANGLE14_HALF_CIRCLE, 0);
    int32_t expected = (31 - ((ANGLE14_HALF_CIRCLE) >> 9)) & 31;
    TEST_CHECK("180 deg angle", angle == expected);
  }

  // Sorting: back-to-front by Z.
  {
    SecObject obj_a, obj_b, obj_c;
    memset(&obj_a, 0, sizeof(SecObject));
    memset(&obj_b, 0, sizeof(SecObject));
    memset(&obj_c, 0, sizeof(SecObject));
    obj_a.type = OBJ_TYPE_SPRITE;
    obj_b.type = OBJ_TYPE_SPRITE;
    obj_c.type = OBJ_TYPE_SPRITE;

    ObjectSortEntry entries[3];
    entries[0].obj = &obj_a;
    entries[0].vz = 5.0f;
    entries[1].obj = &obj_b;
    entries[1].vz = 20.0f;
    entries[2].obj = &obj_c;
    entries[2].vz = 10.0f;

    object_sort(entries, 3);

    TEST_CHECK("farthest first", entries[0].vz >= entries[1].vz);
    TEST_CHECK("middle second", entries[1].vz >= entries[2].vz);
  }

  // Bridge priority: bridge sorts before non-bridge regardless of Z.
  {
    SecObject bridge_obj, normal_obj;
    memset(&bridge_obj, 0, sizeof(SecObject));
    memset(&normal_obj, 0, sizeof(SecObject));
    bridge_obj.type = OBJ_TYPE_3D;
    bridge_obj.flags = OBJ_FLAG_BOSS;
    normal_obj.type = OBJ_TYPE_SPRITE;

    ObjectSortEntry entries[2];
    entries[0].obj = &normal_obj;
    entries[0].vz = 100.0f;
    entries[1].obj = &bridge_obj;
    entries[1].vz = 5.0f;

    object_sort(entries, 2);

    TEST_CHECK("bridge first", entries[0].obj == &bridge_obj);
  }

  // Transform: null entries in sparse array are skipped.
  {
    CameraState cam;
    memset(&cam, 0, sizeof(cam));
    camera_set_projection(&cam, 320, 200, 90.0f, 1.333f);
    camera_compute_transform(&cam, 0.0f, 0.0f, 0.0f, 0, 0);

    SecObject obj;
    memset(&obj, 0, sizeof(SecObject));
    obj.type = OBJ_TYPE_SPRITE;
    obj.pos_ws.x = float_to_fixed16(5.0f);
    obj.pos_ws.y = float_to_fixed16(-3.0f);
    obj.pos_ws.z = float_to_fixed16(10.0f);

    SecObject *list[4] = {NULL, &obj, NULL, NULL};
    ObjectSortEntry out[4];
    int32_t count = object_sort_transform(&cam, list, 4, out);
    TEST_CHECK("sparse: count = 1", count == 1);
    TEST_CHECK("sparse: correct obj", out[0].obj == &obj);
    TEST_CHECK("sparse: vz > 0", out[0].vz > 0.0f);
  }

  TEST_SUITE_END();
}
