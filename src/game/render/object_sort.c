// ===========================================================================
// Object Sorting
// ===========================================================================
// Transforms sector objects to view space and sorts back-to-front for the
// painter's algorithm. Bridge 3DO models sort before all other objects.

#include "render/object_sort.h"
#include "world/flags.h"
#include "world/model.h"
#include "world/object.h"
#include <stdlib.h>

int32_t object_sort_transform(const CameraState *cam,
                              SecObject **objects, int32_t object_capacity,
                              ObjectSortEntry *out) {
  int32_t count = 0;

  for (int32_t i = 0; i < object_capacity; i++) {
    SecObject *obj = objects[i];
    if (!obj) continue;

    float wx = fixed16_to_float(obj->pos_ws.x);
    float wy = fixed16_to_float(obj->pos_ws.y);
    float wz = fixed16_to_float(obj->pos_ws.z);

    ObjectSortEntry *e = &out[count];
    e->obj = obj;
    camera_transform_vertex(cam, wx, wy, wz, &e->vx, &e->vy, &e->vz);

    if (obj->type == OBJ_TYPE_SPRITE) {
      e->sprite_view_angle = object_sort_sprite_angle(cam->yaw, obj->yaw);
    } else {
      e->sprite_view_angle = 0;
    }

    count++;
  }

  return count;
}

// qsort comparison: bridge models first, then back-to-front by Z.
static int object_sort_compare(const void *a, const void *b) {
  const ObjectSortEntry *ea = (const ObjectSortEntry *)a;
  const ObjectSortEntry *eb = (const ObjectSortEntry *)b;

  // Bridge 3DO models are drawn first (they act as floors).
  // A 3DO is a bridge if its model has the flag; since JediModel doesn't
  // have an explicit bridge field yet, we use OBJ_FLAG_BOSS as a proxy
  // for bridge priority. TODO: add is_bridge to JediModel when 3DO
  // loading is implemented.
  bool a_bridge = (ea->obj->type == OBJ_TYPE_3D) &&
                  (ea->obj->flags & OBJ_FLAG_BOSS);
  bool b_bridge = (eb->obj->type == OBJ_TYPE_3D) &&
                  (eb->obj->flags & OBJ_FLAG_BOSS);

  if (a_bridge && !b_bridge) return -1;
  if (!a_bridge && b_bridge) return  1;

  // Back-to-front: larger Z (farther) drawn first.
  if (eb->vz > ea->vz) return  1;
  if (eb->vz < ea->vz) return -1;
  return 0;
}

void object_sort(ObjectSortEntry *entries, int32_t count) {
  if (count <= 1) return;
  qsort(entries, (size_t)count, sizeof(ObjectSortEntry), object_sort_compare);
}

int32_t object_sort_sprite_angle(Angle14 camera_yaw, Angle14 obj_yaw) {
  // The view angle is derived from the angular difference between the camera
  // and the object, quantized to 32 bins (one per WAX view direction).
  int32_t diff = (camera_yaw - obj_yaw) >> 9;
  return (31 - diff) & 31;
}
