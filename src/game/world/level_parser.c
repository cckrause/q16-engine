// ===========================================================================
// Level Geometry Parser
// ===========================================================================
// Streaming tokenizer for LEV (Dark Forces) and LVT (Outlaws) level files.
// Reads sectors, walls, vertices, and textures from an archive stream.

#include "world/level_parser.h"
#include "io/text_parser.h"
#include "math/core_math.h"
#include "memory/game_memory.h"
#include "types/types.h"
#include "world/flags.h"
#include "world/level.h"
#include "world/sector.h"
#include "world/texture.h"
#include "world/wall.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Parse limits — reject files with absurd counts before allocation

#define PARSE_MAX_SECTORS      65536
#define PARSE_MAX_WALLS        262144
#define PARSE_MAX_VERTICES     262144
#define PARSE_MAX_TEXTURES     4096
#define PARSE_MAX_HEADER_ITEMS 256

// Like calloc for the level region: rejects count*elem_size that would wrap int32_t.
static void *level_calloc(int32_t count, int32_t elem_size) {
  if (count <= 0 || elem_size <= 0) {
    return NULL;
  }
  if (count >
      INT32_MAX /
          elem_size) { // if element size is too large, the multiplication will overflow
    return NULL;
  }
  return level_alloc(count * elem_size);
}

// Adjoin index encoding
// During parsing, adjoin pointers temporarily hold encoded integer indices
// instead of real pointers. Index 0 is offset to 1 so NULL means no-adjoin.

#define ENCODE_SECTOR_IDX(id) ((Sector *)(intptr_t)((id) + 1))
#define DECODE_SECTOR_IDX(p)  ((int32_t)(intptr_t)(p) - 1)
#define ENCODE_WALL_IDX(id)   ((Wall *)(intptr_t)((id) + 1))
#define DECODE_WALL_IDX(p)    ((int32_t)(intptr_t)(p) - 1)

// Wall direction vector (hybrid float/fixed)

static Fixed16 wall_compute_direction(Wall *wall) {
  Fixed16 dx = wall->w1->x - wall->w0->x;
  Fixed16 dz = wall->w1->z - wall->w0->z;

  float fdx = fixed16_to_float(dx);
  float fdz = fixed16_to_float(dz);
  float len = sqrtf(fdx * fdx + fdz * fdz);
  Fixed16 len_fixed = float_to_fixed16(len);

  if (len_fixed != 0) {
    wall->wall_dir.x = div16(dx, len_fixed);
    wall->wall_dir.z = div16(dz, len_fixed);
  } else {
    wall->wall_dir.x = 0;
    wall->wall_dir.z = 0;
  }
  return len_fixed;
}

// Texture helpers

static Texture *resolve_tex(LevelState *state, int32_t idx) {
  if (idx < 0 || idx >= state->texture_count) {
    return NULL;
  }
  return &state->textures[idx];
}

static void wall_finish(Wall *wall) {
  wall->length = wall_compute_direction(wall);
  wall->texel_length = wall->length * 8;

  Fixed16 dx = wall->w1->x - wall->w0->x;
  Fixed16 dz = wall->w1->z - wall->w0->z;
  wall->angle = vec2_to_angle(dx, dz);

  wall->seen = JFALSE;
  wall->collision_frame = 0;
  wall->emit_frame = 0;
  wall->draw_flags = 0;
}

// ID parser — auto-detect decimal vs hex
// DF uses small decimal IDs (0, 1, 191). OL uses hex (9CB2055E, 1A2B).
// If the token contains any hex letter (A-F), parse as hex; else decimal.

static int32_t parse_id(TextParser *p) {
  char token[32];
  if (!parser_read_token(p, token, sizeof(token))) {
    return 0;
  }
  for (int32_t i = 0; token[i] != '\0'; i++) {
    char c = token[i];
    if ((c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
      return (int32_t)strtol(token, NULL, 16);
    }
  }
  return (int32_t)strtol(token, NULL, 10);
}

// LEV 2.1 header parser (Dark Forces)

static bool parse_lev_header(TextParser *p, LevelState *state) {
  char version[16];
  parser_read_token(p, version, sizeof(version));
  if (strcmp(version, "2.1") != 0) {
    return false;
  }

  parser_match(p, "LEVELNAME");
  parser_read_token(p, state->level_name, sizeof(state->level_name));

  parser_match(p, "PALETTE");
  parser_read_token(p, state->palette_name, sizeof(state->palette_name));
  state->palette_count = 1;
  memcpy(state->palette_names[0], state->palette_name, sizeof(state->palette_names[0]));

  parser_match(p, "MUSIC");
  parser_read_token(p, state->music_name, sizeof(state->music_name));

  parser_match(p, "PARALLAX");
  state->parallax0 = float_to_fixed16(parser_read_float(p));
  state->parallax1 = float_to_fixed16(parser_read_float(p));

  return true;
}

// LVT 1.1 header parser (Outlaws)

static bool parse_lvt_header(TextParser *p, LevelState *state) {
  char version[16];
  parser_read_token(p, version, sizeof(version));
  if (strcmp(version, "1.1") != 0) {
    return false;
  }

  parser_match(p, "LEVELNAME");
  parser_read_token(p, state->level_name, sizeof(state->level_name));

  // VERSION <float> (build version, skip)
  parser_match(p, "VERSION");
  parser_read_float(p);

  // PALETTES <n> + PALETTE: <name> ...
  parser_match(p, "PALETTES");
  state->palette_count = parser_read_int(p);
  if (state->palette_count < 0 || state->palette_count > PARSE_MAX_HEADER_ITEMS) {
    return false;
  }
  for (int32_t i = 0; i < state->palette_count; i++) {
    parser_match(p, "PALETTE:");
    char name[16];
    parser_read_token(p, name, sizeof(name));
    if (i < MAX_LEVEL_PALETTES) {
      memcpy(state->palette_names[i], name, sizeof(state->palette_names[i]));
      state->palette_names[i][sizeof(state->palette_names[i]) - 1] = '\0';
    }
  }
  if (state->palette_count > 0) {
    memcpy(state->palette_name, state->palette_names[0], sizeof(state->palette_name));
  }

  // CMAPS <n> + CMAP: <name> ...
  parser_match(p, "CMAPS");
  int32_t cmap_count = parser_read_int(p);
  if (cmap_count < 0 || cmap_count > PARSE_MAX_HEADER_ITEMS) {
    return false;
  }
  for (int32_t i = 0; i < cmap_count; i++) {
    parser_match(p, "CMAP:");
    parser_skip_line(p);
  }

  parser_match(p, "MUSIC");
  parser_read_token(p, state->music_name, sizeof(state->music_name));

  parser_match(p, "PARALLAX");
  state->parallax0 = float_to_fixed16(parser_read_float(p));
  state->parallax1 = float_to_fixed16(parser_read_float(p));

  // LIGHT SOURCE <x> <y> <z> <w> (skip)
  parser_match(p, "LIGHT");
  parser_match(p, "SOURCE");
  parser_read_float(p);
  parser_read_float(p);
  parser_read_float(p);
  parser_read_float(p);

  // SHADES <n> + SHADE: lines (skip all)
  parser_match(p, "SHADES");
  int32_t shade_count = parser_read_int(p);
  if (shade_count < 0 || shade_count > PARSE_MAX_HEADER_ITEMS) {
    return false;
  }
  for (int32_t i = 0; i < shade_count; i++) {
    parser_match(p, "SHADE:");
    parser_skip_line(p);
  }

  return true;
}

// Texture section (shared between LEV and LVT)

static bool parse_textures(TextParser *p, LevelState *state) {
  if (!parser_match(p, "TEXTURES")) {
    return false;
  }
  int32_t count = parser_read_int(p);
  if (count < 0 || count > PARSE_MAX_TEXTURES) {
    return false;
  }
  state->texture_count = count;

  if (count > 0) {
    int32_t total = count * 2;
    state->textures = (Texture *)level_calloc(total, (int32_t)sizeof(Texture));
    if (!state->textures) {
      return false;
    }
    memset(state->textures, 0, sizeof(Texture) * (size_t)total);

    for (int32_t i = 0; i < count; i++) {
      parser_match(p, "TEXTURE:");
      char name[16];
      parser_read_token(p, name, sizeof(name));

      if (strcmp(name, "<NoTexture>") == 0) {
        state->textures[i].name[0] = '\0';
      } else {
        memcpy(state->textures[i].name, name, sizeof(state->textures[i].name));
        state->textures[i].name[sizeof(state->textures[i].name) - 1] = '\0';
      }
      state->textures[count + i] = state->textures[i];
    }
  }

  return true;
}

// Wall parser — Dark Forces format
// WALL LEFT: <v0> RIGHT: <v1> MID: <tex> <offX> <offY> <flag>
// TOP: ... BOT: ... SIGN: ... ADJOIN: <id> MIRROR: <id> WALK: <id>
// FLAGS: <f1> <f2> <f3> LIGHT: <int>

static bool parse_wall_df(TextParser *p, Wall *wall, Sector *sec, LevelState *state) {
  parser_match(p, "LEFT:");
  int32_t left_idx = parser_read_int(p);
  parser_match(p, "RIGHT:");
  int32_t right_idx = parser_read_int(p);

  if (left_idx < 0 || left_idx >= sec->vertex_count || right_idx < 0 ||
      right_idx >= sec->vertex_count) {
    return false;
  }

  wall->w0 = &sec->vertices_ws[left_idx];
  wall->w1 = &sec->vertices_ws[right_idx];
  wall->world_pos0.x = wall->w0->x;
  wall->world_pos0.z = wall->w0->z;

  // MID: <tex> <offX> <offY> <unused>
  parser_match(p, "MID:");
  int32_t mid_idx = parser_read_int(p);
  wall->mid_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->mid_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  parser_read_float(p);
  wall->mid_tex = resolve_tex(state, mid_idx);

  // TOP: <tex> <offX> <offY> <unused>
  parser_match(p, "TOP:");
  int32_t top_idx = parser_read_int(p);
  wall->top_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->top_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  parser_read_float(p);
  wall->top_tex = resolve_tex(state, top_idx);

  // BOT: <tex> <offX> <offY> <unused>
  parser_match(p, "BOT:");
  int32_t bot_idx = parser_read_int(p);
  wall->bot_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->bot_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  parser_read_float(p);
  wall->bot_tex = resolve_tex(state, bot_idx);

  // SIGN: <tex> <offX> <offY>
  parser_match(p, "SIGN:");
  int32_t sign_idx = parser_read_int(p);
  wall->sign_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->sign_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  wall->sign_tex = resolve_tex(state, sign_idx);

  // ADJOIN: <id> MIRROR: <id> WALK: <id>
  parser_match(p, "ADJOIN:");
  int32_t adjoin_id = parser_read_int(p);
  parser_match(p, "MIRROR:");
  int32_t mirror_id = parser_read_int(p);
  parser_match(p, "WALK:");
  parser_read_int(p);

  if (adjoin_id >= 0) {
    wall->next_sector = ENCODE_SECTOR_IDX(adjoin_id);
    wall->mirror_wall = ENCODE_WALL_IDX(mirror_id);
  } else {
    wall->next_sector = NULL;
    wall->mirror_wall = NULL;
  }
  wall->dadjoin_sector = NULL;
  wall->dmirror_wall = NULL;

  // FLAGS: <f1> <f2> <f3>
  parser_match(p, "FLAGS:");
  wall->flags1 = (uint32_t)parser_read_int(p);
  parser_read_int(p); // f2 (unused by Wall struct)
  wall->flags3 = (uint32_t)parser_read_int(p);

  parser_match(p, "LIGHT:");
  wall->wall_light = int_to_fixed16(parser_read_int(p));

  wall_finish(wall);
  return true;
}

// Wall parser — Outlaws format
// WALL: <id> V1: <v0> V2: <v1> MID: <tex> <offX> <offY>
// TOP: ... BOT: ... SIGN:/OVERLAY: ... ADJOIN: <id> MIRROR: <id>
// DADJOIN: <id> DMIRROR: <id> FLAGS: <f1> <f2> LIGHT: <int>

static bool parse_wall_ol(TextParser *p, Wall *wall, Sector *sec, LevelState *state) {
  parser_read_int(p); // wall ID (discarded, indexed sequentially)
  parser_match(p, "V1:");
  int32_t v1_idx = parser_read_int(p);
  parser_match(p, "V2:");
  int32_t v2_idx = parser_read_int(p);

  if (v1_idx < 0 || v1_idx >= sec->vertex_count || v2_idx < 0 ||
      v2_idx >= sec->vertex_count) {
    return false;
  }

  wall->w0 = &sec->vertices_ws[v1_idx];
  wall->w1 = &sec->vertices_ws[v2_idx];
  wall->world_pos0.x = wall->w0->x;
  wall->world_pos0.z = wall->w0->z;

  // MID: <tex> <offX> <offY>
  parser_match(p, "MID:");
  int32_t mid_idx = parser_read_int(p);
  wall->mid_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->mid_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  wall->mid_tex = resolve_tex(state, mid_idx);

  // TOP: <tex> <offX> <offY>
  parser_match(p, "TOP:");
  int32_t top_idx = parser_read_int(p);
  wall->top_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->top_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  wall->top_tex = resolve_tex(state, top_idx);

  // BOT: <tex> <offX> <offY>
  parser_match(p, "BOT:");
  int32_t bot_idx = parser_read_int(p);
  wall->bot_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->bot_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  wall->bot_tex = resolve_tex(state, bot_idx);

  // SIGN: or OVERLAY: <tex> <offX> <offY>
  if (!parser_match(p, "SIGN:")) {
    parser_match(p, "OVERLAY:");
  }
  int32_t sign_idx = parser_read_int(p);
  wall->sign_offset.x = float_to_fixed16(parser_read_float(p)) * 8;
  wall->sign_offset.z = float_to_fixed16(parser_read_float(p)) * 8;
  wall->sign_tex = resolve_tex(state, sign_idx);

  // ADJOIN: <id> MIRROR: <id> DADJOIN: <id> DMIRROR: <id>
  parser_match(p, "ADJOIN:");
  int32_t adjoin_id = parser_read_int(p);
  parser_match(p, "MIRROR:");
  int32_t mirror_id = parser_read_int(p);
  parser_match(p, "DADJOIN:");
  int32_t dadjoin_id = parser_read_int(p);
  parser_match(p, "DMIRROR:");
  int32_t dmirror_id = parser_read_int(p);

  if (adjoin_id >= 0) {
    wall->next_sector = ENCODE_SECTOR_IDX(adjoin_id);
    wall->mirror_wall = ENCODE_WALL_IDX(mirror_id);
  } else {
    wall->next_sector = NULL;
    wall->mirror_wall = NULL;
  }
  if (dadjoin_id >= 0) {
    wall->dadjoin_sector = ENCODE_SECTOR_IDX(dadjoin_id);
    wall->dmirror_wall = ENCODE_WALL_IDX(dmirror_id);
  } else {
    wall->dadjoin_sector = NULL;
    wall->dmirror_wall = NULL;
  }

  // FLAGS: <f1> <f2>
  parser_match(p, "FLAGS:");
  wall->flags1 = (uint32_t)parser_read_int(p);
  wall->flags3 = (uint32_t)parser_read_int(p);

  parser_match(p, "LIGHT:");
  wall->wall_light = int_to_fixed16(parser_read_int(p));

  wall_finish(wall);
  return true;
}

// Unified sector parser (keyword-driven)
// Handles both Dark Forces and Outlaws sector keywords in any order.
// Unknown keywords are skipped gracefully.

static bool parse_sector(TextParser *p, LevelState *state, int32_t sec_idx) {
  Sector *sec = &state->sectors[sec_idx];
  memset(sec, 0, sizeof(Sector));

  parser_match(p, "SECTOR");
  parse_id(p); // consume file ID (DF: decimal, OL: hex) — not used as array index
  sec->id = sec_idx;
  sec->self = sec;

  // Default slope values
  sec->slope_floor.sector_idx = -1;
  sec->slope_floor.wall_idx = -1;
  sec->slope_ceiling.sector_idx = -1;
  sec->slope_ceiling.wall_idx = -1;

  // Keyword-driven property loop — exits when VERTICES is found
  for (;;) {
    if (parser_at_end(p)) {
      return false;
    }

    if (parser_match(p, "VERTICES")) {
      break;
    } else if (parser_match(p, "NAME")) {
      if (!parser_read_line_token(p, sec->name, sizeof(sec->name))) {
        sec->name[0] = '\0';
      }
      if (strcmp(sec->name, "complete") == 0) {
        state->complete_sector = sec;
      } else if (strcmp(sec->name, "boss") == 0) {
        state->boss_sector = sec;
      } else if (strcmp(sec->name, "mohc") == 0) {
        state->mohc_sector = sec;
      }
    } else if (parser_match(p, "AMBIENT")) {
      sec->ambient = int_to_fixed16(parser_read_int(p));
    } else if (parser_match(p, "FLOOR")) {
      if (parser_match(p, "Y")) {
        // OL: FLOOR Y <height> <texIdx> <offX> <offZ> <unused>
        // Negate to convert OL Y-up heights to DF Y-down convention.
        sec->floor_height = -float_to_fixed16(parser_read_float(p));
        int32_t idx = parser_read_int(p);
        sec->floor_offset.x = float_to_fixed16(parser_read_float(p));
        sec->floor_offset.z = float_to_fixed16(parser_read_float(p));
        parser_read_int(p);
        sec->floor_tex = resolve_tex(state, idx);
      } else if (parser_match(p, "TEXTURE")) {
        // DF: FLOOR TEXTURE <idx> <offX> <offZ> <unused>
        int32_t idx = parser_read_int(p);
        sec->floor_offset.x = float_to_fixed16(parser_read_float(p));
        sec->floor_offset.z = float_to_fixed16(parser_read_float(p));
        parser_read_float(p);
        sec->floor_tex = resolve_tex(state, idx);
      } else if (parser_match(p, "ALTITUDE")) {
        // DF: FLOOR ALTITUDE <height>
        sec->floor_height = float_to_fixed16(parser_read_float(p));
      } else if (parser_match(p, "OFFSETS")) {
        // OL: FLOOR OFFSETS <value> → stored as sec_height
        sec->sec_height = float_to_fixed16(parser_read_float(p));
      } else if (parser_match(p, "SOUND")) {
        parser_skip_line(p);
      } else {
        parser_skip_line(p);
      }
    } else if (parser_match(p, "CEILING")) {
      if (parser_match(p, "Y")) {
        // OL: CEILING Y <height> <texIdx> <offX> <offZ> <unused>
        // Negate to convert OL Y-up heights to DF Y-down convention.
        sec->ceiling_height = -float_to_fixed16(parser_read_float(p));
        int32_t idx = parser_read_int(p);
        sec->ceil_offset.x = float_to_fixed16(parser_read_float(p));
        sec->ceil_offset.z = float_to_fixed16(parser_read_float(p));
        parser_read_int(p);
        sec->ceil_tex = resolve_tex(state, idx);
      } else if (parser_match(p, "TEXTURE")) {
        // DF: CEILING TEXTURE <idx> <offX> <offZ> <unused>
        int32_t idx = parser_read_int(p);
        sec->ceil_offset.x = float_to_fixed16(parser_read_float(p));
        sec->ceil_offset.z = float_to_fixed16(parser_read_float(p));
        parser_read_float(p);
        sec->ceil_tex = resolve_tex(state, idx);
      } else if (parser_match(p, "ALTITUDE")) {
        // DF: CEILING ALTITUDE <height>
        sec->ceiling_height = float_to_fixed16(parser_read_float(p));
      } else {
        parser_skip_line(p);
      }
    } else if (parser_match(p, "SECOND")) {
      // DF: SECOND ALTITUDE <height>
      parser_match(p, "ALTITUDE");
      sec->sec_height = float_to_fixed16(parser_read_float(p));
    } else if (parser_match(p, "FLAGS")) {
      // 1-3 integer values depending on format
      char tok[32];
      sec->flags1 = (uint32_t)parser_read_int(p);
      if (parser_read_line_token(p, tok, sizeof(tok))) {
        sec->flags2 = (uint32_t)strtol(tok, NULL, 10);
        if (parser_read_line_token(p, tok, sizeof(tok))) {
          sec->flags3 = (uint32_t)strtol(tok, NULL, 10);
        }
      }
      if (sec->flags1 & SEC_FLAG1_SECRET) {
        state->secret_count++;
      }
    } else if (parser_match(p, "SLOPEDFLOOR")) {
      sec->slope_floor.sector_idx = parser_read_int(p);
      sec->slope_floor.wall_idx = parser_read_int(p);
      sec->slope_floor.angle = parser_read_int(p);
    } else if (parser_match(p, "SLOPEDCEILING")) {
      sec->slope_ceiling.sector_idx = parser_read_int(p);
      sec->slope_ceiling.wall_idx = parser_read_int(p);
      sec->slope_ceiling.angle = parser_read_int(p);
    } else if (parser_match(p, "LAYER")) {
      sec->layer = parser_read_int(p);
      if (sec->layer < state->min_layer) {
        state->min_layer = sec->layer;
      }
      if (sec->layer > state->max_layer) {
        state->max_layer = sec->layer;
      }
    } else if (parser_match(p, "PALETTE") || parser_match(p, "CMAP") ||
               parser_match(p, "VADJOIN") || parser_match(p, "FRICTION") ||
               parser_match(p, "GRAVITY") || parser_match(p, "ELASTICITY") ||
               parser_match(p, "VELOCITY") || parser_match(p, "F_OVERLAY") ||
               parser_match(p, "C_OVERLAY")) {
      parser_skip_line(p);
    } else {
      parser_skip_line(p);
    }
  }

  // Vertex section
  sec->vertex_count = parser_read_int(p);
  if (sec->vertex_count < 0 || sec->vertex_count > PARSE_MAX_VERTICES) {
    return false;
  }
  if (sec->vertex_count > 0) {
    sec->vertices_ws =
        (Vec2Fixed *)level_calloc(sec->vertex_count, (int32_t)sizeof(Vec2Fixed));
    sec->vertices_vs =
        (Vec2Fixed *)level_calloc(sec->vertex_count, (int32_t)sizeof(Vec2Fixed));
    if (!sec->vertices_ws || !sec->vertices_vs) {
      return false;
    }
  }

  for (int32_t v = 0; v < sec->vertex_count; v++) {
    parser_match(p, "X:");
    sec->vertices_ws[v].x = float_to_fixed16(parser_read_float(p));
    parser_match(p, "Z:");
    sec->vertices_ws[v].z = float_to_fixed16(parser_read_float(p));
  }

  // Wall section
  parser_match(p, "WALLS");
  sec->wall_count = parser_read_int(p);
  if (sec->wall_count < 0 || sec->wall_count > PARSE_MAX_WALLS) {
    return false;
  }
  if (sec->wall_count > 0) {
    sec->walls = (Wall *)level_calloc(sec->wall_count, (int32_t)sizeof(Wall));
    if (!sec->walls) {
      return false;
    }
    memset(sec->walls, 0, sizeof(Wall) * (size_t)sec->wall_count);
  }

  for (int32_t w = 0; w < sec->wall_count; w++) {
    Wall *wall = &sec->walls[w];
    wall->sector = sec;

    // Per-wall format detection: OL uses "WALL:" + "V1:", DF uses "WALL" + "LEFT:"
    if (parser_match(p, "WALL:")) {
      if (!parse_wall_ol(p, wall, sec, state)) {
        return false;
      }
    } else {
      parser_match(p, "WALL");
      if (!parse_wall_df(p, wall, sec, state)) {
        return false;
      }
    }
  }

  // Init remaining sector fields
  sec->dirty_flags = SDF_ALL;
  sec->object_list = NULL;
  sec->object_count = 0;
  sec->object_capacity = 0;
  sec->inf_link = NULL;
  sec->collision_frame = 0;
  sec->search_key = 0;
  sec->prev_draw_frame = 0;
  sec->draw_wall_cnt = 0;
  sec->col_min_sector = NULL;

  return true;
}

// Post-parse adjoin resolution
// Encoded adjoin/mirror indices are validated against actual sector/wall
// counts and then resolved to real pointers.

static void resolve_adjoins(LevelState *state) {
  for (int32_t si = 0; si < state->sector_count; si++) {
    Sector *sec = &state->sectors[si];
    for (int32_t wi = 0; wi < sec->wall_count; wi++) {
      Wall *wall = &sec->walls[wi];

      if (wall->next_sector != NULL) {
        int32_t adj_idx = DECODE_SECTOR_IDX(wall->next_sector);
        if (adj_idx < 0 || adj_idx >= state->sector_count) {
          wall->next_sector = NULL;
          wall->mirror_wall = NULL;
        } else {
          Sector *adj_sec = &state->sectors[adj_idx];
          int32_t mir_idx = DECODE_WALL_IDX(wall->mirror_wall);
          if (mir_idx < 0 || mir_idx >= adj_sec->wall_count) {
            wall->next_sector = NULL;
            wall->mirror_wall = NULL;
          } else {
            wall->next_sector = adj_sec;
            wall->mirror_wall = &adj_sec->walls[mir_idx];
          }
        }
      }

      if (wall->dadjoin_sector != NULL) {
        int32_t dadj_idx = DECODE_SECTOR_IDX(wall->dadjoin_sector);
        if (dadj_idx < 0 || dadj_idx >= state->sector_count) {
          wall->dadjoin_sector = NULL;
          wall->dmirror_wall = NULL;
        } else {
          Sector *dadj_sec = &state->sectors[dadj_idx];
          int32_t dmir_idx = DECODE_WALL_IDX(wall->dmirror_wall);
          if (dmir_idx < 0 || dmir_idx >= dadj_sec->wall_count) {
            wall->dadjoin_sector = NULL;
            wall->dmirror_wall = NULL;
          } else {
            wall->dadjoin_sector = dadj_sec;
            wall->dmirror_wall = &dadj_sec->walls[dmir_idx];
          }
        }
      }
    }
  }
}

// Entry point — auto-detect format and parse

bool level_load_geometry(StreamReader *reader, LevelState *state) {
  TextParser p;
  parser_init(&p, reader);

  memset(state, 0, sizeof(LevelState));
  state->min_layer = INT32_MAX;
  state->max_layer = INT32_MIN;

  // Auto-detect format from magic token
  bool header_ok = false;
  if (parser_match(&p, "LEV")) {
    header_ok = parse_lev_header(&p, state);
  } else if (parser_match(&p, "LVT")) {
    header_ok = parse_lvt_header(&p, state);
  }
  if (!header_ok) {
    return false;
  }

  if (!parse_textures(&p, state)) {
    return false;
  }

  if (!parser_match(&p, "NUMSECTORS")) {
    return false;
  }
  state->sector_count = parser_read_int(&p);
  if (state->sector_count < 0 || state->sector_count > PARSE_MAX_SECTORS) {
    return false;
  }
  if (state->sector_count > 0) {
    state->sectors = (Sector *)level_calloc(state->sector_count, (int32_t)sizeof(Sector));
    if (!state->sectors) {
      return false;
    }
  }

  int32_t total_walls = 0;
  int32_t total_verts = 0;

  for (int32_t i = 0; i < state->sector_count; i++) {
    if (!parse_sector(&p, state, i)) {
      return false;
    }
    total_walls += state->sectors[i].wall_count;
    total_verts += state->sectors[i].vertex_count;
  }

  state->wall_count = total_walls;
  state->vertex_count = total_verts;

  resolve_adjoins(state);

  // Compute effective collision heights from PIT/EXTERIOR flags.
  for (int32_t i = 0; i < state->sector_count; i++) {
    Sector *sec = &state->sectors[i];
    sec->col_floor_height = sec->floor_height;
    sec->col_ceil_height = sec->ceiling_height;
    if (sec->flags1 & SEC_FLAG1_PIT) {
      sec->col_floor_height += SEC_SKY_HEIGHT;
    }
    if (sec->flags1 & SEC_FLAG1_EXTERIOR) {
      sec->col_ceil_height -= SEC_SKY_HEIGHT;
    }
  }

  // Compute AABB bounds for each sector from its vertex positions.
  for (int32_t i = 0; i < state->sector_count; i++) {
    Sector *sec = &state->sectors[i];
    if (sec->vertex_count <= 0)
      continue;

    Fixed16 min_x = sec->vertices_ws[0].x;
    Fixed16 max_x = min_x;
    Fixed16 min_z = sec->vertices_ws[0].z;
    Fixed16 max_z = min_z;

    for (int32_t v = 1; v < sec->vertex_count; v++) {
      Fixed16 vx = sec->vertices_ws[v].x;
      Fixed16 vz = sec->vertices_ws[v].z;
      if (vx < min_x)
        min_x = vx;
      if (vx > max_x)
        max_x = vx;
      if (vz < min_z)
        min_z = vz;
      if (vz > max_z)
        max_z = vz;
    }

    sec->bounds_min.x = min_x;
    sec->bounds_min.z = min_z;
    sec->bounds_max.x = max_x;
    sec->bounds_max.z = max_z;
  }

  return true;
}
