// ===========================================================================
// OpenGL 3.3 Wireframe Backend
// ===========================================================================
#ifndef Q16_DEV_GL_BACKEND_H
#define Q16_DEV_GL_BACKEND_H

#include "render/render_sector.h"
#include <stdbool.h>
#include <stdint.h>

// Consumes display list output from the CPU render pipeline and draws
// colored wireframe quads via a minimal shader pair.

// Color modes for wireframe visualization.
#define GL_COLOR_PART       0 // color by part type (wall/floor/ceiling)
#define GL_COLOR_SECTOR     1 // unique color per sector ID
#define GL_COLOR_DEPTH      2 // color by adjoin recursion depth
#define GL_COLOR_MODE_COUNT 3

typedef struct {
  uint32_t program;
  uint32_t vao;
  uint32_t vbo;
  int32_t u_viewproj_loc;
  int32_t u_alpha_loc;
  int32_t max_vertices;
  int32_t color_mode;
} GlBackend;

// Compile shaders, create VAO/VBO. Returns false on failure.
bool gl_backend_init(GlBackend *gl, int32_t max_display_entries);

void gl_backend_destroy(GlBackend *gl);

// Draw the display list as colored wireframe.
void gl_backend_draw(GlBackend *gl, const RenderState *rs, const Sector *level_sectors,
                     int32_t sector_count);

// Draw a fullscreen 2D top-down minimap overlay with semi-transparent background.
void gl_backend_draw_minimap(GlBackend *gl, const Sector *sectors, int32_t sector_count,
                             const bool *visited, int32_t cam_sector_id, float cam_x,
                             float cam_z, float cam_yaw_rad, int32_t viewport_w,
                             int32_t viewport_h);

#endif /* Q16_DEV_GL_BACKEND_H */
