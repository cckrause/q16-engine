// ===========================================================================
// Sector — Spatial queries
// ===========================================================================

#include "world/sector.h"
#include "world/level.h"
#include "world/wall.h"
#include <stddef.h>

bool sector_contains_point(const Sector *s, Fixed16 px, Fixed16 pz) {
  int32_t crossings = 0;
  for (int32_t i = 0; i < s->wall_count; i++) {
    Fixed16 z0 = s->walls[i].w0->z;
    Fixed16 z1 = s->walls[i].w1->z;
    if ((z0 <= pz && z1 > pz) || (z1 <= pz && z0 > pz)) {
      Fixed16 x0 = s->walls[i].w0->x;
      Fixed16 x1 = s->walls[i].w1->x;
      Fixed16 cross_x = x0 + div16(mul16(pz - z0, x1 - x0), z1 - z0);
      if (px < cross_x)
        crossings++;
    }
  }
  return (crossings & 1) != 0;
}

Sector *sector_find_at(LevelState *state, Fixed16 x, Fixed16 y, Fixed16 z) {
  Sector *best = NULL;
  bool best_y_match = false;
  Sector *fallback = NULL;
  Fixed16 best_dist_sq = 0;

  for (int32_t i = 0; i < state->sector_count; i++) {
    Sector *s = &state->sectors[i];
    if (x < s->bounds_min.x || x > s->bounds_max.x ||
        z < s->bounds_min.z || z > s->bounds_max.z)
      continue;

    if (!sector_contains_point(s, x, z)) {
      Fixed16 cx = (s->bounds_min.x >> 1) + (s->bounds_max.x >> 1);
      Fixed16 cz = (s->bounds_min.z >> 1) + (s->bounds_max.z >> 1);
      Fixed16 dx = x - cx;
      Fixed16 dz = z - cz;
      Fixed16 d = mul16(dx, dx) + mul16(dz, dz);
      if (!fallback || d < best_dist_sq) {
        best_dist_sq = d;
        fallback = s;
      }
      continue;
    }

    bool y_ok = (y >= s->ceiling_height && y <= s->floor_height);

    if (y_ok) {
      if (!best || !best_y_match) {
        best = s;
        best_y_match = true;
      } else {
        Fixed16 cur_h = best->floor_height - best->ceiling_height;
        Fixed16 new_h = s->floor_height - s->ceiling_height;
        if (new_h < cur_h)
          best = s;
      }
    } else if (!best) {
      best = s;
    }
  }

  if (best)
    return best;
  return fallback ? fallback : &state->sectors[0];
}
