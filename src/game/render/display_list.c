// ===========================================================================
// Display List Construction
// ===========================================================================
// Packs CPU-side geometry into position+data buffers for the HAL to consume.
// Separate opaque and transparent lists with portal clip plane storage.

#include "render/display_list.h"
#include <stdlib.h>
#include <string.h>

bool display_list_init(DisplayList *dl, int32_t max_entries, int32_t max_planes) {
  dl->opaque = (DisplayListEntry *)malloc((size_t)max_entries * sizeof(DisplayListEntry));
  dl->transparent = (DisplayListEntry *)malloc((size_t)max_entries * sizeof(DisplayListEntry));
  dl->planes = (DisplayListPlane *)malloc((size_t)max_planes * sizeof(DisplayListPlane));

  if (!dl->opaque || !dl->transparent || !dl->planes) {
    free(dl->opaque);
    free(dl->transparent);
    free(dl->planes);
    dl->opaque = NULL;
    dl->transparent = NULL;
    dl->planes = NULL;
    return false;
  }

  dl->max_entries = max_entries;
  dl->max_planes  = max_planes;
  dl->opaque_count      = 0;
  dl->transparent_count = 0;
  dl->plane_count       = 0;
  return true;
}

void display_list_destroy(DisplayList *dl) {
  free(dl->opaque);
  free(dl->transparent);
  free(dl->planes);
  dl->opaque = NULL;
  dl->transparent = NULL;
  dl->planes = NULL;
}

void display_list_reset(DisplayList *dl) {
  dl->opaque_count      = 0;
  dl->transparent_count = 0;
  dl->plane_count       = 0;
}

bool display_list_add_opaque(DisplayList *dl, const DisplayListEntry *entry) {
  if (dl->opaque_count >= dl->max_entries) return false;
  dl->opaque[dl->opaque_count++] = *entry;
  return true;
}

bool display_list_add_transparent(DisplayList *dl, const DisplayListEntry *entry) {
  if (dl->transparent_count >= dl->max_entries) return false;
  dl->transparent[dl->transparent_count++] = *entry;
  return true;
}

int32_t display_list_add_plane(DisplayList *dl, float x, float y, float z, float w) {
  if (dl->plane_count >= dl->max_planes) return -1;
  int32_t idx = dl->plane_count;
  dl->planes[idx].x = x;
  dl->planes[idx].y = y;
  dl->planes[idx].z = z;
  dl->planes[idx].w = w;
  dl->plane_count++;
  return idx;
}
