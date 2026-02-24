// ===========================================================================
// Texture (BM bitmap)
// ===========================================================================
#ifndef Q16_WORLD_TEXTURE_H
#define Q16_WORLD_TEXTURE_H

#include "types/types.h"
#include <stdint.h>

// Runtime representation of a BM texture after loading.
// Pixel data is column-major: image[x * height + y], palette indices 0-255.
// 8:1 texel-to-world-unit ratio: 64px wide texture spans 8 world units.
struct Texture {
  char name[16]; // BM filename (e.g. "IFWALT.BM") for deferred loading
  int32_t width;
  int32_t height;
  uint32_t flags;   // TEX_OPACITY_TRANS (0x08) or TEX_OPACITY_OPAQUE
  JBool compressed; // RLE-compressed column data
  uint8_t *image;   // column-major pixel data (palette indices)

  struct Texture *next_frame; // next frame in animation cycle (wraps around)
  int32_t anim_frame_count;
};

// ===========================================================================
// WAX sprite hierarchy
// WAX -> Anims -> Views -> Frames -> Cells
// ===========================================================================

struct WaxCell {
  int32_t size_x;      // width in pixels (may be negative for flipped)
  int32_t size_y;      // height in pixels
  uint32_t compressed; // 0 = uncompressed, 1 = RLE
  uint8_t *image;      // column-major pixel data (palette indices)
};

struct WaxFrame {
  int32_t offset_x;    // horizontal rendering offset
  int32_t offset_y;    // vertical rendering offset
  JBool flip;          // horizontal flip flag
  struct WaxCell cell; // pixel data (owned, not a pointer — loaded contiguously)
};

struct WaxView {
  int32_t frame_count;
  struct WaxFrame *frames; // array, one per frame in this viewing angle
};

struct WaxAnim {
  int32_t frame_rate; // index into s_frameTicks[]
  int32_t frame_count;
  int32_t view_count;    // typically 8 or 32
  struct WaxView *views; // array, one per viewing angle
};

struct JediWax {
  Fixed16 x_scale;
  Fixed16 y_scale;
  int32_t anim_count;
  struct WaxAnim *anims; // array of animation states
};

#endif /* Q16_WORLD_TEXTURE_H */
