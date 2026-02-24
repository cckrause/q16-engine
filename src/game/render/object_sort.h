// ===========================================================================
// Object Sorting
// ===========================================================================
#ifndef Q16_RENDER_OBJECT_SORT_H
#define Q16_RENDER_OBJECT_SORT_H

#include "render/camera.h"
#include "types/forward.h"
#include <stdint.h>

// Transforms sector objects to view space and sorts them back-to-front for
// correct draw ordering. Bridge models (which serve as walkable floors for
// other objects) are drawn first regardless of depth.

// View-space object data computed per frame.
typedef struct {
  SecObject *obj;
  float vx;  // view-space X
  float vy;  // view-space Y
  float vz;  // view-space Z (depth — larger = farther)

  // WAX sprite view angle selection.
  int32_t sprite_view_angle; // 0-31, derived from camera-object angle delta
} ObjectSortEntry;

// Transform all non-NULL objects in a sector to view space.
// cam: current camera.
// objects: the sector's object_list (sparse array, may contain NULLs).
// object_capacity: length of the objects array.
// out: caller-provided array (must hold at least object_capacity entries).
// Returns the number of entries written to out.
int32_t object_sort_transform(const CameraState *cam,
                              SecObject **objects, int32_t object_capacity,
                              ObjectSortEntry *out);

// Sort entries back-to-front by view-space Z. Bridge models sort first.
void object_sort(ObjectSortEntry *entries, int32_t count);

// Compute the WAX view angle index (0-31) from the camera yaw and object yaw.
int32_t object_sort_sprite_angle(Angle14 camera_yaw, Angle14 obj_yaw);

#endif /* Q16_RENDER_OBJECT_SORT_H */
