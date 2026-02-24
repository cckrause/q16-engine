// ===========================================================================
// OpenGL 3.3 Wireframe Backend
// ===========================================================================
// Expands display list entries into colored line segments and draws them.

#include "gl_backend.h"

#include <glad/glad.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include "render/render_limits.h"
#include "types/fixed16.h"
#include "world/flags.h"
#include "world/sector.h"
#include "world/wall.h"

typedef struct {
  float x, y, z;
  float r, g, b;
} WireVertex;

// 8 vertices per wireframe quad (4 lines via GL_LINES).
#define VERTS_PER_QUAD 8

// 6 vertices per filled quad (2 triangles via GL_TRIANGLES).
#define VERTS_PER_FILL 6

// Total vertices per wall entry: filled + wireframe.
#define VERTS_PER_WALL_TOTAL (VERTS_PER_FILL + VERTS_PER_QUAD)

static const char *s_vert_src = "#version 330 core\n"
                                "uniform mat4 u_viewproj;\n"
                                "layout(location = 0) in vec3 a_pos;\n"
                                "layout(location = 1) in vec3 a_color;\n"
                                "out vec3 v_color;\n"
                                "void main() {\n"
                                "    gl_Position = u_viewproj * vec4(a_pos, 1.0);\n"
                                "    v_color = a_color;\n"
                                "}\n";

static const char *s_frag_src = "#version 330 core\n"
                                "uniform float u_alpha;\n"
                                "in vec3 v_color;\n"
                                "out vec4 frag_color;\n"
                                "void main() {\n"
                                "    frag_color = vec4(v_color, u_alpha);\n"
                                "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);

  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof(log), NULL, log);
    fprintf(stderr, "shader compile error: %s\n", log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

static GLuint link_program(GLuint vs, GLuint fs) {
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);

  GLint ok;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetProgramInfoLog(p, sizeof(log), NULL, log);
    fprintf(stderr, "program link error: %s\n", log);
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

bool gl_backend_init(GlBackend *gl, int32_t max_display_entries) {
  memset(gl, 0, sizeof(*gl));

  GLuint vs = compile_shader(GL_VERTEX_SHADER, s_vert_src);
  if (!vs)
    return false;

  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, s_frag_src);
  if (!fs) {
    glDeleteShader(vs);
    return false;
  }

  gl->program = link_program(vs, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!gl->program)
    return false;

  gl->u_viewproj_loc = glGetUniformLocation(gl->program, "u_viewproj");
  gl->u_alpha_loc = glGetUniformLocation(gl->program, "u_alpha");

  gl->max_vertices = max_display_entries * VERTS_PER_WALL_TOTAL;

  glGenVertexArrays(1, &gl->vao);
  glGenBuffers(1, &gl->vbo);

  glBindVertexArray(gl->vao);
  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  glBufferData(GL_ARRAY_BUFFER,
               (GLsizeiptr)gl->max_vertices * (GLsizeiptr)sizeof(WireVertex), NULL,
               GL_DYNAMIC_DRAW);

  // a_pos at location 0
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WireVertex),
                        (void *)offsetof(WireVertex, x));
  // a_color at location 1
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(WireVertex),
                        (void *)offsetof(WireVertex, r));

  glBindVertexArray(0);
  return true;
}

void gl_backend_destroy(GlBackend *gl) {
  if (gl->vbo)
    glDeleteBuffers(1, &gl->vbo);
  if (gl->vao)
    glDeleteVertexArrays(1, &gl->vao);
  if (gl->program)
    glDeleteProgram(gl->program);
  memset(gl, 0, sizeof(*gl));
}

// HSV to RGB (h in [0,360), s/v in [0,1]).
static void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
  int32_t hi = (int32_t)(h / 60.0f) % 6;
  float f = h / 60.0f - (float)hi;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  switch (hi) {
  case 0:
    *r = v;
    *g = t;
    *b = p;
    break;
  case 1:
    *r = q;
    *g = v;
    *b = p;
    break;
  case 2:
    *r = p;
    *g = v;
    *b = t;
    break;
  case 3:
    *r = p;
    *g = q;
    *b = v;
    break;
  case 4:
    *r = t;
    *g = p;
    *b = v;
    break;
  default:
    *r = v;
    *g = p;
    *b = q;
    break;
  }
}

// Golden-angle hash: maps sector ID to a unique, well-distributed hue.
static void sector_color(int32_t sector_id, float *r, float *g, float *b) {
  float hue = fmodf((float)sector_id * 137.508f, 360.0f);
  hsv_to_rgb(hue, 0.7f, 0.9f, r, g, b);
}

// Part ID colors.
static void part_color(int32_t part_id, float *r, float *g, float *b) {
  switch (part_id) {
  case PART_MID_WALL:
    *r = 1.0f;
    *g = 1.0f;
    *b = 1.0f;
    break; // white
  case PART_TOP_WALL:
    *r = 1.0f;
    *g = 1.0f;
    *b = 0.3f;
    break; // yellow
  case PART_BOT_WALL:
    *r = 0.3f;
    *g = 1.0f;
    *b = 1.0f;
    break; // cyan
  case PART_FLOOR:
    *r = 0.2f;
    *g = 0.8f;
    *b = 0.2f;
    break; // green
  case PART_CEILING:
    *r = 0.8f;
    *g = 0.2f;
    *b = 0.2f;
    break; // red
  case PART_FLOOR_CAP:
    *r = 0.1f;
    *g = 0.4f;
    *b = 0.1f;
    break; // dark green
  case PART_CEILING_CAP:
    *r = 0.4f;
    *g = 0.1f;
    *b = 0.1f;
    break; // dark red
  case PART_MID_SIGN:
  case PART_TOP_SIGN:
  case PART_BOT_SIGN:
    *r = 1.0f;
    *g = 0.5f;
    *b = 0.0f;
    break; // orange
  default:
    *r = 0.5f;
    *g = 0.5f;
    *b = 0.5f;
    break; // grey
  }
}

// Build a perspective projection matrix for our view-space convention:
//   +X right, +Y down, +Z forward
// Maps to OpenGL NDC: X [-1,1], Y [-1,1] (up), Z [-1,1].
static void build_projection(const CameraState *cam, float near, float far,
                             float out[16]) {
  memset(out, 0, 16 * sizeof(float));

  float sx = cam->focal_length / cam->half_width;
  float sy = cam->focal_len_aspect / cam->half_height;
  float pitch_shift = cam->pitch_offset / cam->half_height;

  float d = far - near;

  // Column-major storage for glUniformMatrix4fv.
  out[0] = sx;                      // col 0, row 0
  out[5] = -sy;                     // col 1, row 1  (Y flip)
  out[9] = -pitch_shift;            // col 2, row 1  (Y-shear from pitch)
  out[10] = (far + near) / d;       // col 2, row 2
  out[11] = 1.0f;                   // col 2, row 3  (clip_w = vz)
  out[14] = -2.0f * far * near / d; // col 3, row 2
}

// Emit one wireframe quad (4 lines = 8 vertices).
static int32_t emit_quad(WireVertex *out, float x0, float z0, float x1, float z1,
                         float y_bot, float y_top, float r, float g, float b) {
  // bottom-left → bottom-right
  out[0] = (WireVertex){x0, y_bot, z0, r, g, b};
  out[1] = (WireVertex){x1, y_bot, z1, r, g, b};
  // bottom-right → top-right
  out[2] = (WireVertex){x1, y_bot, z1, r, g, b};
  out[3] = (WireVertex){x1, y_top, z1, r, g, b};
  // top-right → top-left
  out[4] = (WireVertex){x1, y_top, z1, r, g, b};
  out[5] = (WireVertex){x0, y_top, z0, r, g, b};
  // top-left → bottom-left
  out[6] = (WireVertex){x0, y_top, z0, r, g, b};
  out[7] = (WireVertex){x0, y_bot, z0, r, g, b};
  return VERTS_PER_QUAD;
}

// Emit one horizontal line (2 vertices) — used for floor/ceiling edges.
static int32_t emit_hline(WireVertex *out, float x0, float z0, float x1, float z1,
                          float y, float r, float g, float b) {
  out[0] = (WireVertex){x0, y, z0, r, g, b};
  out[1] = (WireVertex){x1, y, z1, r, g, b};
  return 2;
}

// Emit one filled quad as 2 triangles (6 vertices via GL_TRIANGLES).
static int32_t emit_filled_quad(WireVertex *out, float x0, float z0, float x1, float z1,
                                float y_bot, float y_top, float r, float g, float b) {
  float dim = 0.25f;
  float dr = r * dim, dg = g * dim, db = b * dim;
  // Triangle 1: bottom-left, bottom-right, top-right
  out[0] = (WireVertex){x0, y_bot, z0, dr, dg, db};
  out[1] = (WireVertex){x1, y_bot, z1, dr, dg, db};
  out[2] = (WireVertex){x1, y_top, z1, dr, dg, db};
  // Triangle 2: bottom-left, top-right, top-left
  out[3] = (WireVertex){x0, y_bot, z0, dr, dg, db};
  out[4] = (WireVertex){x1, y_top, z1, dr, dg, db};
  out[5] = (WireVertex){x0, y_top, z0, dr, dg, db};
  return VERTS_PER_FILL;
}

// Emit filled triangles for wall parts only (MID, TOP, BOT, SIGN).
static void expand_entries_filled(const DisplayListEntry *entries, int32_t count,
                                  const CameraState *cam, const Sector *level_sectors,
                                  int32_t sector_count, int32_t color_mode,
                                  WireVertex *verts, int32_t *out_count,
                                  int32_t max_verts) {
  int32_t n = 0;

  for (int32_t i = 0; i < count; i++) {
    const DisplayListEntry *e = &entries[i];
    int32_t part_id = (int32_t)(e->data.flags_part & 0x0F);
    int32_t sec_id = (int32_t)e->data.sector_id;

    if (sec_id < 0 || sec_id >= sector_count)
      continue;

    float x0 = e->pos.v0x;
    float z0 = e->pos.v0z;
    float x1 = e->pos.v1x;
    float z1 = e->pos.v1z;
    float y_bot = e->pos.y_bot - cam->pos_y;
    float y_top = e->pos.y_top - cam->pos_y;

    float r, g, b;
    if (color_mode == GL_COLOR_SECTOR) {
      sector_color(sec_id, &r, &g, &b);
    } else {
      part_color(part_id, &r, &g, &b);
    }

    int32_t added = 0;

    switch (part_id) {
    case PART_MID_WALL:
    case PART_TOP_WALL:
    case PART_BOT_WALL:
    case PART_MID_SIGN:
    case PART_TOP_SIGN:
    case PART_BOT_SIGN:
      if (n + VERTS_PER_FILL <= max_verts)
        added = emit_filled_quad(verts + n, x0, z0, x1, z1, y_bot, y_top, r, g, b);
      break;
    default:
      break;
    }

    n += added;
  }

  *out_count = n;
}

static void expand_entries(const DisplayListEntry *entries, int32_t count,
                           const CameraState *cam, const Sector *level_sectors,
                           int32_t sector_count, int32_t color_mode, WireVertex *verts,
                           int32_t *out_count, int32_t max_verts) {
  int32_t n = 0;

  for (int32_t i = 0; i < count; i++) {
    const DisplayListEntry *e = &entries[i];
    int32_t part_id = (int32_t)(e->data.flags_part & 0x0F);
    int32_t sec_id = (int32_t)e->data.sector_id;

    if (sec_id < 0 || sec_id >= sector_count)
      continue;

    float x0 = e->pos.v0x;
    float z0 = e->pos.v0z;
    float x1 = e->pos.v1x;
    float z1 = e->pos.v1z;
    float y_bot = e->pos.y_bot - cam->pos_y;
    float y_top = e->pos.y_top - cam->pos_y;

    float r, g, b;
    if (color_mode == GL_COLOR_SECTOR) {
      sector_color(sec_id, &r, &g, &b);
    } else {
      part_color(part_id, &r, &g, &b);
    }

    int32_t added = 0;

    switch (part_id) {
    case PART_MID_WALL:
    case PART_TOP_WALL:
    case PART_BOT_WALL:
    case PART_MID_SIGN:
    case PART_TOP_SIGN:
    case PART_BOT_SIGN:
      if (n + VERTS_PER_QUAD <= max_verts)
        added = emit_quad(verts + n, x0, z0, x1, z1, y_bot, y_top, r, g, b);
      break;

    case PART_FLOOR:
    case PART_FLOOR_CAP:
      if (n + 2 <= max_verts)
        added = emit_hline(verts + n, x0, z0, x1, z1, y_bot, r, g, b);
      break;

    case PART_CEILING:
    case PART_CEILING_CAP:
      if (n + 2 <= max_verts)
        added = emit_hline(verts + n, x0, z0, x1, z1, y_bot, r, g, b);
      break;

    default:
      break;
    }

    n += added;
  }

  *out_count = n;
}

void gl_backend_draw(GlBackend *gl, const RenderState *rs, const Sector *level_sectors,
                     int32_t sector_count) {
  const DisplayList *dl = &rs->display_list;
  const CameraState *cam = &rs->camera;

  int32_t total_entries = dl->opaque_count + dl->transparent_count;
  if (total_entries == 0)
    return;

  int32_t max_verts = gl->max_vertices;

  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  WireVertex *mapped = (WireVertex *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
  if (!mapped)
    return;

  // Region 1: filled triangles for wall faces.
  int32_t tri_count = 0;
  int32_t n = 0;

  expand_entries_filled(dl->opaque, dl->opaque_count, cam, level_sectors, sector_count,
                        gl->color_mode, mapped, &n, max_verts);
  tri_count += n;

  expand_entries_filled(dl->transparent, dl->transparent_count, cam, level_sectors,
                        sector_count, gl->color_mode, mapped + tri_count, &n,
                        max_verts - tri_count);
  tri_count += n;

  // Region 2: wireframe lines (after triangles in the same buffer).
  int32_t line_count = 0;
  int32_t line_start = tri_count;

  expand_entries(dl->opaque, dl->opaque_count, cam, level_sectors, sector_count,
                 gl->color_mode, mapped + line_start, &n, max_verts - line_start);
  line_count += n;

  expand_entries(dl->transparent, dl->transparent_count, cam, level_sectors, sector_count,
                 gl->color_mode, mapped + line_start + line_count, &n,
                 max_verts - line_start - line_count);
  line_count += n;

  glUnmapBuffer(GL_ARRAY_BUFFER);

  if (tri_count == 0 && line_count == 0)
    return;

  float proj[16];
  build_projection(cam, 0.1f, 4000.0f, proj);

  glUseProgram(gl->program);
  glUniformMatrix4fv(gl->u_viewproj_loc, 1, GL_FALSE, proj);
  glBindVertexArray(gl->vao);

  // Pass 1: filled wall faces (color baked at 25% brightness in vertices).
  if (tri_count > 0) {
    glUniform1f(gl->u_alpha_loc, 1.0f);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, tri_count);
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  // Pass 2: wireframe edges on top.
  if (line_count > 0) {
    glUniform1f(gl->u_alpha_loc, 1.0f);
    glDrawArrays(GL_LINES, line_start, line_count);
  }

  glBindVertexArray(0);
}

// Minimap overlay

#define MINIMAP_PAD       0.05f
#define MINIMAP_MAX_VERTS (65536 * 2)
#define MINIMAP_BG_VERTS  6

// 7-segment digit rendering for sector labels.
// Segments: 0=top, 1=top-right, 2=bot-right, 3=bottom, 4=bot-left, 5=top-left, 6=middle
static const uint8_t s_digit_segs[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static int32_t emit_digit(WireVertex *out, int32_t digit, float cx, float cy, float w,
                          float h, float r, float g, float b) {
  if (digit < 0 || digit > 9)
    return 0;
  uint8_t mask = s_digit_segs[digit];
  int32_t n = 0;
  float hw = w * 0.5f, hh = h * 0.5f;
  float x0 = cx - hw, x1 = cx + hw;
  float y0 = cy - hh, ym = cy, y1 = cy + hh;

  if (mask & 0x01) {
    out[n++] = (WireVertex){x0, y1, 0, r, g, b};
    out[n++] = (WireVertex){x1, y1, 0, r, g, b};
  }
  if (mask & 0x02) {
    out[n++] = (WireVertex){x1, ym, 0, r, g, b};
    out[n++] = (WireVertex){x1, y1, 0, r, g, b};
  }
  if (mask & 0x04) {
    out[n++] = (WireVertex){x1, y0, 0, r, g, b};
    out[n++] = (WireVertex){x1, ym, 0, r, g, b};
  }
  if (mask & 0x08) {
    out[n++] = (WireVertex){x0, y0, 0, r, g, b};
    out[n++] = (WireVertex){x1, y0, 0, r, g, b};
  }
  if (mask & 0x10) {
    out[n++] = (WireVertex){x0, y0, 0, r, g, b};
    out[n++] = (WireVertex){x0, ym, 0, r, g, b};
  }
  if (mask & 0x20) {
    out[n++] = (WireVertex){x0, ym, 0, r, g, b};
    out[n++] = (WireVertex){x0, y1, 0, r, g, b};
  }
  if (mask & 0x40) {
    out[n++] = (WireVertex){x0, ym, 0, r, g, b};
    out[n++] = (WireVertex){x1, ym, 0, r, g, b};
  }
  return n;
}

static int32_t emit_number(WireVertex *out, int32_t value, float cx, float cy,
                           float digit_w, float digit_h, float r, float g, float b,
                           int32_t max_verts) {
  char buf[12];
  int32_t len = snprintf(buf, sizeof(buf), "%d", value);
  if (len <= 0)
    return 0;
  float total_w = (float)len * digit_w * 1.4f;
  float start_x = cx - total_w * 0.5f + digit_w * 0.7f;
  int32_t n = 0;
  for (int32_t i = 0; i < len && n + 14 <= max_verts; i++) {
    int32_t d = buf[i] - '0';
    n += emit_digit(out + n, d, start_x + (float)i * digit_w * 1.4f, cy, digit_w, digit_h,
                    r, g, b);
  }
  return n;
}

void gl_backend_draw_minimap(GlBackend *gl, const Sector *sectors, int32_t sector_count,
                             const bool *visited, int32_t cam_sector_id, float cam_x,
                             float cam_z, float cam_yaw_rad, int32_t viewport_w,
                             int32_t viewport_h) {
  if (sector_count <= 0)
    return;

  // Compute world bounds across all sectors.
  float min_x = 1e18f, max_x = -1e18f;
  float min_z = 1e18f, max_z = -1e18f;
  for (int32_t i = 0; i < sector_count; i++) {
    float bx0 = fixed16_to_float(sectors[i].bounds_min.x);
    float bz0 = fixed16_to_float(sectors[i].bounds_min.z);
    float bx1 = fixed16_to_float(sectors[i].bounds_max.x);
    float bz1 = fixed16_to_float(sectors[i].bounds_max.z);
    if (bx0 < min_x)
      min_x = bx0;
    if (bz0 < min_z)
      min_z = bz0;
    if (bx1 > max_x)
      max_x = bx1;
    if (bz1 > max_z)
      max_z = bz1;
  }

  float world_w = max_x - min_x;
  float world_h = max_z - min_z;
  if (world_w < 1.0f)
    world_w = 1.0f;
  if (world_h < 1.0f)
    world_h = 1.0f;

  // Fullscreen ortho: fit the entire world centered in the viewport,
  // preserving aspect ratio so the map doesn't distort.
  float avail = 2.0f - 2.0f * MINIMAP_PAD;
  float aspect = (float)viewport_w / (float)viewport_h;

  // uniform_scale_x: NDC units per world unit.
  // Constraint: world fits in avail NDC in both axes, corrected for aspect.
  float usx_w = avail / world_w;
  float usx_h = avail / (world_h * aspect);
  float usx = (usx_w < usx_h) ? usx_w : usx_h;

  float sx = usx;
  float sz = usx * aspect;
  float center_x = (min_x + max_x) * 0.5f;
  float center_z = (min_z + max_z) * 0.5f;
  float tx = -center_x * sx;
  float tz = -center_z * sz;

  float ortho[16];
  memset(ortho, 0, sizeof(ortho));
  ortho[0] = sx;
  ortho[5] = sz;
  ortho[10] = 0.0f;
  ortho[12] = tx;
  ortho[13] = tz;
  ortho[15] = 1.0f;

  // Build vertex data: background quad + sector outlines as lines.
  glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
  WireVertex *mapped = (WireVertex *)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
  if (!mapped)
    return;

  int32_t max_v = gl->max_vertices;
  if (max_v > MINIMAP_MAX_VERTS)
    max_v = MINIMAP_MAX_VERTS;

  // First 6 vertices: fullscreen background quad (NDC coords, drawn with
  // identity matrix). Two triangles covering -1..1 in X and Y.
  float bg = 0.02f;
  mapped[0] = (WireVertex){-1, -1, 0, bg, bg, bg};
  mapped[1] = (WireVertex){1, -1, 0, bg, bg, bg};
  mapped[2] = (WireVertex){1, 1, 0, bg, bg, bg};
  mapped[3] = (WireVertex){-1, -1, 0, bg, bg, bg};
  mapped[4] = (WireVertex){1, 1, 0, bg, bg, bg};
  mapped[5] = (WireVertex){-1, 1, 0, bg, bg, bg};
  int32_t n = MINIMAP_BG_VERTS;

  for (int32_t i = 0; i < sector_count && n + 2 <= max_v; i++) {
    const Sector *s = &sectors[i];

    float r, g, b;
    if (i == cam_sector_id) {
      r = 1.0f;
      g = 1.0f;
      b = 0.0f;
    } else if (visited && visited[i]) {
      sector_color(i, &r, &g, &b);
    } else {
      r = 0.15f;
      g = 0.15f;
      b = 0.15f;
    }

    for (int32_t w = 0; w < s->wall_count && n + 2 <= max_v; w++) {
      float wx0 = fixed16_to_float(s->walls[w].w0->x);
      float wz0 = fixed16_to_float(s->walls[w].w0->z);
      float wx1 = fixed16_to_float(s->walls[w].w1->x);
      float wz1 = fixed16_to_float(s->walls[w].w1->z);
      mapped[n++] = (WireVertex){wx0, wz0, 0.0f, r, g, b};
      mapped[n++] = (WireVertex){wx1, wz1, 0.0f, r, g, b};
    }
  }

  // Sector ID labels (only for visited sectors + camera sector).
  float label_size = world_w * 0.025f;
  for (int32_t i = 0; i < sector_count && n + 14 <= max_v; i++) {
    if (i != cam_sector_id && !(visited && visited[i]))
      continue;
    const Sector *s = &sectors[i];
    float scx = fixed16_to_float(s->bounds_min.x + s->bounds_max.x) * 0.5f;
    float scz = fixed16_to_float(s->bounds_min.z + s->bounds_max.z) * 0.5f;
    float lr = 1.0f, lg = 1.0f, lb = 1.0f;
    if (i == cam_sector_id) {
      lr = 1.0f;
      lg = 1.0f;
      lb = 0.0f;
    }
    n += emit_number(mapped + n, i, scx, scz, label_size, label_size * 1.6f, lr, lg, lb,
                     max_v - n);
  }

  // Camera indicator: position dot + direction line.
  if (n + 4 <= max_v) {
    float arrow_len = world_w * 0.015f;
    float dx = sinf(cam_yaw_rad) * arrow_len;
    float dz = cosf(cam_yaw_rad) * arrow_len;
    float pr = 1.0f, pg = 1.0f, pb = 1.0f;
    mapped[n++] = (WireVertex){cam_x - dx * 0.3f, cam_z - dz * 0.3f, 0.0f, pr, pg, pb};
    mapped[n++] = (WireVertex){cam_x + dx, cam_z + dz, 0.0f, pr, pg, pb};
    float cx = cosf(cam_yaw_rad) * arrow_len * 0.3f;
    float cz = -sinf(cam_yaw_rad) * arrow_len * 0.3f;
    mapped[n++] = (WireVertex){cam_x - cx, cam_z - cz, 0.0f, pr, pg, pb};
    mapped[n++] = (WireVertex){cam_x + cx, cam_z + cz, 0.0f, pr, pg, pb};
  }

  glUnmapBuffer(GL_ARRAY_BUFFER);

  int32_t line_verts = n - MINIMAP_BG_VERTS;
  if (line_verts <= 0)
    return;

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(gl->program);
  glBindVertexArray(gl->vao);

  // Pass 1: fullscreen dark background quad (identity matrix, 50% alpha).
  float identity[16];
  memset(identity, 0, sizeof(identity));
  identity[0] = 1.0f;
  identity[5] = 1.0f;
  identity[10] = 1.0f;
  identity[15] = 1.0f;

  glUniformMatrix4fv(gl->u_viewproj_loc, 1, GL_FALSE, identity);
  glUniform1f(gl->u_alpha_loc, 0.5f);
  glDrawArrays(GL_TRIANGLES, 0, MINIMAP_BG_VERTS);

  // Pass 2: minimap lines (ortho matrix, fully opaque).
  glUniformMatrix4fv(gl->u_viewproj_loc, 1, GL_FALSE, ortho);
  glUniform1f(gl->u_alpha_loc, 1.0f);
  glDrawArrays(GL_LINES, MINIMAP_BG_VERTS, line_verts);

  glBindVertexArray(0);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
}
