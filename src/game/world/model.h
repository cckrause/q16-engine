// ===========================================================================
// JediModel (3DO)
// ===========================================================================
#ifndef Q16_WORLD_MODEL_H
#define Q16_WORLD_MODEL_H

#include "types/forward.h"
#include <stdint.h>


struct JediPolygon {
  int32_t *vertex_indices; // indices into parent model vertex array
  int32_t vertex_count;
  Texture *texture; // per-polygon texture (NULL = solid color)
  uint8_t color;    // flat-color palette index (if no texture)
  uint32_t shading; // shading mode (FLAT, GOURAUD, etc.)
};

struct JediSubObject {
  char name[32];
  Fixed16 transform[9]; // local 3x3 rotation matrix
  Vec3Fixed offset;     // local translation offset
  int32_t polygon_start;
  int32_t polygon_count;
  struct JediSubObject *children;
  int32_t child_count;
};

struct JediModel {
  char name[32];
  int32_t vertex_count;
  Vec3Fixed *vertices; // model-space vertex positions
  int32_t polygon_count;
  struct JediPolygon *polygons;
  int32_t object_count;
  struct JediSubObject *objects; // hierarchical sub-objects
};

#endif /* Q16_WORLD_MODEL_H */
