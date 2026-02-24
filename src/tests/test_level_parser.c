#include "io/stream.h"
#include "memory/game_memory.h"
#include "test_harness.h"
#include "types/types.h"
#include "world/flags.h"
#include "world/level.h"
#include "world/level_parser.h"
#include "world/sector.h"
#include "world/texture.h"
#include "world/wall.h"

#include <math.h>
#include <string.h>

// Minimal valid LEV: 1 sector, 4 walls, a 10x10 square room.
static const char *MINIMAL_LEV =
    "LEV 2.1\n"
    "LEVELNAME test_level\n"
    "PALETTE SECBASE.PAL\n"
    "MUSIC SECBASE.GMD\n"
    "PARALLAX 1024.0 1024.0\n"
    "TEXTURES 2\n"
    "  TEXTURE: FLOOR.BM\n"
    "  TEXTURE: <NoTexture>\n"
    "NUMSECTORS 1\n"
    "  SECTOR 0\n"
    "    NAME complete\n"
    "    AMBIENT 20\n"
    "    FLOOR TEXTURE 0 0.5 0.25 0\n"
    "    FLOOR ALTITUDE -2.0\n"
    "    CEILING TEXTURE 0 0.0 0.0 0\n"
    "    CEILING ALTITUDE -12.0\n"
    "    SECOND ALTITUDE 0.0\n"
    "    FLAGS 32768 0 0\n"
    "    LAYER 1\n"
    "    VERTICES 4\n"
    "      X: 0.0 Z: 0.0\n"
    "      X: 10.0 Z: 0.0\n"
    "      X: 10.0 Z: 10.0\n"
    "      X: 0.0 Z: 10.0\n"
    "    WALLS 4\n"
    "      WALL LEFT: 0 RIGHT: 1 MID: 0 1.5 2.0 0 TOP: -1 0.0 0.0 0 "
    "BOT: -1 0.0 0.0 0 SIGN: -1 0.0 0.0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
    "FLAGS: 0 0 0 LIGHT: 5\n"
    "      WALL LEFT: 1 RIGHT: 2 MID: 0 0.0 0.0 0 TOP: -1 0.0 0.0 0 "
    "BOT: -1 0.0 0.0 0 SIGN: -1 0.0 0.0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
    "FLAGS: 0 0 0 LIGHT: 0\n"
    "      WALL LEFT: 2 RIGHT: 3 MID: 0 0.0 0.0 0 TOP: -1 0.0 0.0 0 "
    "BOT: -1 0.0 0.0 0 SIGN: -1 0.0 0.0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
    "FLAGS: 0 0 0 LIGHT: 0\n"
    "      WALL LEFT: 3 RIGHT: 0 MID: 0 0.0 0.0 0 TOP: -1 0.0 0.0 0 "
    "BOT: -1 0.0 0.0 0 SIGN: -1 0.0 0.0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
    "FLAGS: 0 0 0 LIGHT: 0\n";

// Minimal valid LVT: 2 sectors — one with empty NAME + active slopes,
// one with normal name. Tests keyword-driven parsing, SLOPEDFLOOR/CEILING,
// empty NAME, OL wall format with SIGN:.
static const char *MINIMAL_LVT =
    "LVT 1.1\n"
    "LEVELNAME HIDEOUT\n"
    "VERSION 1234.042497\n"
    "PALETTES 2\n"
    "  PALETTE: OLPAL.PCX\n"
    "  PALETTE: UWATER.PCX\n"
    "CMAPS 1\n"
    "  CMAP: OLPAL.CMP\n"
    "MUSIC TOWN1.GMD\n"
    "PARALLAX 1024.00 0.00\n"
    "LIGHT SOURCE 0.0 0.0 0.0 0.0\n"
    "SHADES 2\n"
    "SHADE:  1 200 200 200 10 L\n"
    "SHADE:  2 200 200 200 25 L\n"
    "TEXTURES 2\n"
    "  TEXTURE: WOOD01.PCX\n"
    "  TEXTURE: STONE02.PCX\n"
    "NUMSECTORS 2\n"
    // Sector 0: empty NAME, active SLOPEDFLOOR
    "SECTOR 0\n"
    "  NAME\n"
    "  AMBIENT 20\n"
    "  PALETTE 0\n"
    "  CMAP 0\n"
    "  VADJOIN -1\n"
    "  FRICTION 1\n"
    "  GRAVITY -60\n"
    "  ELASTICITY 0.3\n"
    "  VELOCITY 0 0 0\n"
    "  FLOOR SOUND NULL\n"
    "  FLOOR Y 0.00 0 0.00 0.00 0\n"
    "  CEILING Y 24.00 1 0.00 0.00 0\n"
    "  F_OVERLAY -1 0.00 0.00 0\n"
    "  C_OVERLAY -1 0.00 0.00 0\n"
    "  FLOOR OFFSETS 0\n"
    "  FLAGS 1 0\n"
    "  SLOPEDFLOOR 0 1 4096\n"
    "  SLOPEDCEILING -1 -1 0\n"
    "  LAYER 1\n"
    "  VERTICES 4\n"
    "    X: 0.00 Z: 0.00\n"
    "    X: 10.00 Z: 0.00\n"
    "    X: 10.00 Z: 10.00\n"
    "    X: 0.00 Z: 10.00\n"
    "  WALLS 4\n"
    "    WALL: 0 V1: 0 V2: 1 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 SIGN: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    "    WALL: 1 V1: 1 V2: 2 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 SIGN: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    "    WALL: 2 V1: 2 V2: 3 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 SIGN: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    "    WALL: 3 V1: 3 V2: 0 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 SIGN: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    // Sector 1: named sector, OVERLAY: keyword (real-file variant)
    "SECTOR 1A2B\n"
    "  NAME entrance\n"
    "  AMBIENT 15\n"
    "  PALETTE 0\n"
    "  CMAP 0\n"
    "  VADJOIN -1\n"
    "  FRICTION 1\n"
    "  GRAVITY -60\n"
    "  ELASTICITY 0.3\n"
    "  VELOCITY 0 0 0\n"
    "  FLOOR SOUND WOOD\n"
    "  FLOOR Y -5.00 0 0.50 0.25 0\n"
    "  CEILING Y 10.00 1 0.00 0.00 0\n"
    "  F_OVERLAY -1 0.00 0.00 0\n"
    "  C_OVERLAY -1 0.00 0.00 0\n"
    "  FLOOR OFFSETS 0\n"
    "  FLAGS 32768 0\n"
    "  SLOPEDFLOOR -1 -1 0\n"
    "  SLOPEDCEILING -1 -1 0\n"
    "  LAYER 2\n"
    "  VERTICES 4\n"
    "    X: 0.00 Z: 0.00\n"
    "    X: 10.00 Z: 0.00\n"
    "    X: 10.00 Z: 10.00\n"
    "    X: 0.00 Z: 10.00\n"
    "  WALLS 4\n"
    "    WALL: 100 V1: 0 V2: 1 MID: 0 1.00 2.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 OVERLAY: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 3\n"
    "    WALL: 101 V1: 1 V2: 2 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 OVERLAY: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    "    WALL: 102 V1: 2 V2: 3 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 OVERLAY: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n"
    "    WALL: 103 V1: 3 V2: 0 MID: 0 0.00 0.00 TOP: -1 0.00 0.00 "
    "BOT: -1 0.00 0.00 OVERLAY: -1 0.00 0.00 "
    "ADJOIN: -1 MIRROR: -1 DADJOIN: -1 DMIRROR: -1 FLAGS: 0 0 LIGHT: 0\n";

void test_level_parser(void) {
  TEST_SUITE_BEGIN("level_parser");

  game_memory_init();

  // LEV 2.1 (Dark Forces) parsing tests

  StreamReader sr = stream_from_memory(MINIMAL_LEV, (int32_t)strlen(MINIMAL_LEV));
  LevelState state;
  bool ok = level_load_geometry(&sr, &state);
  stream_close(&sr);

  TEST_CHECK("parse succeeds", ok);
  if (!ok) {
    game_level_clear();
    game_memory_shutdown();
    TEST_SUITE_END();
    return;
  }

  // Header metadata
  TEST_CHECK("level_name", strcmp(state.level_name, "test_level") == 0);
  TEST_CHECK("palette_name", strcmp(state.palette_name, "SECBASE.PAL") == 0);
  TEST_CHECK("music_name", strcmp(state.music_name, "SECBASE.GMD") == 0);
  TEST_CHECK("parallax0", state.parallax0 == FIXED(1024));
  TEST_CHECK("parallax1", state.parallax1 == FIXED(1024));

  // Textures
  TEST_CHECK("texture_count", state.texture_count == 2);
  TEST_CHECK("tex 0 name", strcmp(state.textures[0].name, "FLOOR.BM") == 0);
  TEST_CHECK("tex 1 is NoTexture", state.textures[1].name[0] == '\0');
  TEST_CHECK("tex immutable copy name", strcmp(state.textures[2].name, "FLOOR.BM") == 0);

  // Sector
  TEST_CHECK("sector_count", state.sector_count == 1);
  Sector *sec = &state.sectors[0];
  TEST_CHECK("sector id", sec->id == 0);
  TEST_CHECK("sector name", strcmp(sec->name, "complete") == 0);
  TEST_CHECK("complete_sector", state.complete_sector == sec);
  TEST_CHECK("ambient", sec->ambient == FIXED(20));
  TEST_CHECK("floor_height", sec->floor_height == FIXED(-2));
  TEST_CHECK("ceiling_height", sec->ceiling_height == FIXED(-12));
  TEST_CHECK("sec_height", sec->sec_height == 0);

  TEST_CHECK("flags1", sec->flags1 == 0x8000);
  TEST_CHECK("flags2", sec->flags2 == 0);
  TEST_CHECK("flags3", sec->flags3 == 0);
  // 0x8000 has no PIT/EXTERIOR bits set in OL mode
  TEST_CHECK("col_floor == floor (no PIT)", sec->col_floor_height == sec->floor_height);
  TEST_CHECK("col_ceil == ceil (no EXTERIOR)",
             sec->col_ceil_height == sec->ceiling_height);

  TEST_CHECK("layer", sec->layer == 1);
  TEST_CHECK("min_layer", state.min_layer == 1);
  TEST_CHECK("max_layer", state.max_layer == 1);

  TEST_CHECK("floor_offset.x", sec->floor_offset.x == float_to_fixed16(0.5f));
  TEST_CHECK("floor_offset.z", sec->floor_offset.z == float_to_fixed16(0.25f));
  TEST_CHECK("floor_tex points to tex 0", sec->floor_tex == &state.textures[0]);

  // Slopes default to inactive for DF
  TEST_CHECK("slope_floor inactive", sec->slope_floor.sector_idx == -1);
  TEST_CHECK("slope_ceiling inactive", sec->slope_ceiling.sector_idx == -1);

  // Vertices
  TEST_CHECK("vertex_count", sec->vertex_count == 4);
  TEST_CHECK("v0.x", sec->vertices_ws[0].x == FIXED(0));
  TEST_CHECK("v0.z", sec->vertices_ws[0].z == FIXED(0));
  TEST_CHECK("v1.x", sec->vertices_ws[1].x == FIXED(10));
  TEST_CHECK("v2.z", sec->vertices_ws[2].z == FIXED(10));
  TEST_CHECK("v3.x", sec->vertices_ws[3].x == FIXED(0));

  // Walls
  TEST_CHECK("wall_count", sec->wall_count == 4);
  TEST_CHECK("total wall_count", state.wall_count == 4);
  TEST_CHECK("total vertex_count", state.vertex_count == 4);

  Wall *w0 = &sec->walls[0];
  TEST_CHECK("w0 sector backptr", w0->sector == sec);
  TEST_CHECK("w0->w0 points to v0", w0->w0 == &sec->vertices_ws[0]);
  TEST_CHECK("w0->w1 points to v1", w0->w1 == &sec->vertices_ws[1]);
  TEST_CHECK("w0 world_pos0.x", w0->world_pos0.x == FIXED(0));
  TEST_CHECK("w0 world_pos0.z", w0->world_pos0.z == FIXED(0));

  float actual_len = fixed16_to_float(w0->length);
  TEST_CHECK("w0 length ~10", fabsf(actual_len - 10.0f) < 0.01f);
  TEST_CHECK("w0 texel_length = length*8", w0->texel_length == w0->length * 8);

  float dir_x = fixed16_to_float(w0->wall_dir.x);
  float dir_z = fixed16_to_float(w0->wall_dir.z);
  TEST_CHECK("w0 dir_x ~1", fabsf(dir_x - 1.0f) < 0.01f);
  TEST_CHECK("w0 dir_z ~0", fabsf(dir_z) < 0.01f);

  Fixed16 expected_mid_x = float_to_fixed16(1.5f) * 8;
  Fixed16 expected_mid_z = float_to_fixed16(2.0f) * 8;
  TEST_CHECK("w0 mid_offset.x (x8)", w0->mid_offset.x == expected_mid_x);
  TEST_CHECK("w0 mid_offset.z (x8)", w0->mid_offset.z == expected_mid_z);
  TEST_CHECK("w0 mid_tex points to tex 0", w0->mid_tex == &state.textures[0]);
  TEST_CHECK("w0 top_tex is NULL", w0->top_tex == NULL);
  TEST_CHECK("w0 bot_tex is NULL", w0->bot_tex == NULL);
  TEST_CHECK("w0 sign_tex is NULL", w0->sign_tex == NULL);
  TEST_CHECK("w0 wall_light", w0->wall_light == FIXED(5));
  TEST_CHECK("w0 next_sector NULL", w0->next_sector == NULL);
  TEST_CHECK("w0 mirror_wall NULL", w0->mirror_wall == NULL);
  TEST_CHECK("w0 dadjoin NULL", w0->dadjoin_sector == NULL);
  TEST_CHECK("w0 dmirror NULL", w0->dmirror_wall == NULL);
  TEST_CHECK("w0 seen", w0->seen == JFALSE);

  Wall *w3 = &sec->walls[3];
  TEST_CHECK("w3->w0 is v3", w3->w0 == &sec->vertices_ws[3]);
  TEST_CHECK("w3->w1 is v0", w3->w1 == &sec->vertices_ws[0]);

  // Invalid format
  {
    StreamReader sr2 = stream_from_memory("NOT A LEV FILE", 14);
    LevelState bad;
    bool bad_ok = level_load_geometry(&sr2, &bad);
    TEST_CHECK("bad format fails", !bad_ok);
    stream_close(&sr2);
  }

  // LVT 1.1 (Outlaws) parsing tests

  game_level_clear();

  {
    StreamReader lvt_sr = stream_from_memory(MINIMAL_LVT, (int32_t)strlen(MINIMAL_LVT));
    LevelState ls;
    bool lvt_ok = level_load_geometry(&lvt_sr, &ls);
    stream_close(&lvt_sr);

    TEST_CHECK("LVT parse succeeds", lvt_ok);
    if (lvt_ok) {
      // Header
      TEST_CHECK("lvt level_name", strcmp(ls.level_name, "HIDEOUT") == 0);
      TEST_CHECK("lvt palette_name", strcmp(ls.palette_name, "OLPAL.PCX") == 0);
      TEST_CHECK("lvt palette_count", ls.palette_count == 2);
      TEST_CHECK("lvt palette_names[0]", strcmp(ls.palette_names[0], "OLPAL.PCX") == 0);
      TEST_CHECK("lvt palette_names[1]", strcmp(ls.palette_names[1], "UWATER.PCX") == 0);
      TEST_CHECK("lvt music_name", strcmp(ls.music_name, "TOWN1.GMD") == 0);
      TEST_CHECK("lvt parallax0", ls.parallax0 == FIXED(1024));
      TEST_CHECK("lvt parallax1", ls.parallax1 == FIXED(0));

      // Textures
      TEST_CHECK("lvt texture_count", ls.texture_count == 2);
      TEST_CHECK("lvt tex 0", strcmp(ls.textures[0].name, "WOOD01.PCX") == 0);
      TEST_CHECK("lvt tex 1", strcmp(ls.textures[1].name, "STONE02.PCX") == 0);

      // Sector counts
      TEST_CHECK("lvt sector_count", ls.sector_count == 2);
      TEST_CHECK("lvt total walls", ls.wall_count == 8);
      TEST_CHECK("lvt total verts", ls.vertex_count == 8);

      // Sector 0: empty NAME, active SLOPEDFLOOR
      Sector *s0 = &ls.sectors[0];
      TEST_CHECK("lvt s0 id", s0->id == 0);
      TEST_CHECK("lvt s0 empty name", s0->name[0] == '\0');
      TEST_CHECK("lvt s0 ambient", s0->ambient == FIXED(20));
      TEST_CHECK("lvt s0 floor_height", s0->floor_height == FIXED(0));
      TEST_CHECK("lvt s0 ceiling_height (negated Y-up→Y-down)",
                 s0->ceiling_height == float_to_fixed16(-24.0f));
      TEST_CHECK("lvt s0 floor_tex", s0->floor_tex == &ls.textures[0]);
      TEST_CHECK("lvt s0 ceil_tex", s0->ceil_tex == &ls.textures[1]);
      TEST_CHECK("lvt s0 flags1", s0->flags1 == 1);
      TEST_CHECK("lvt s0 flags2", s0->flags2 == 0);
      // s0 has EXTERIOR (bit 0): col_ceil_height lowered by SEC_SKY_HEIGHT
      TEST_CHECK("lvt s0 col_floor_height == floor_height",
                 s0->col_floor_height == s0->floor_height);
      TEST_CHECK("lvt s0 col_ceil_height adjusted for EXTERIOR",
                 s0->col_ceil_height == s0->ceiling_height - SEC_SKY_HEIGHT);
      TEST_CHECK("lvt s0 layer", s0->layer == 1);
      TEST_CHECK("lvt s0 vertex_count", s0->vertex_count == 4);
      TEST_CHECK("lvt s0 wall_count", s0->wall_count == 4);

      // SLOPEDFLOOR 0 1 4096
      TEST_CHECK("lvt s0 slope_floor.sector_idx", s0->slope_floor.sector_idx == 0);
      TEST_CHECK("lvt s0 slope_floor.wall_idx", s0->slope_floor.wall_idx == 1);
      TEST_CHECK("lvt s0 slope_floor.angle", s0->slope_floor.angle == 4096);
      // SLOPEDCEILING -1 -1 0
      TEST_CHECK("lvt s0 slope_ceiling inactive", s0->slope_ceiling.sector_idx == -1);

      // OL wall format (SIGN: keyword)
      Wall *lw0 = &s0->walls[0];
      TEST_CHECK("lvt s0 w0 sector", lw0->sector == s0);
      TEST_CHECK("lvt s0 w0->w0 is v0", lw0->w0 == &s0->vertices_ws[0]);
      TEST_CHECK("lvt s0 w0 next_sector NULL", lw0->next_sector == NULL);
      TEST_CHECK("lvt s0 w0 dadjoin NULL", lw0->dadjoin_sector == NULL);

      // Sector 1: named, hex ID, OVERLAY: keyword
      Sector *s1 = &ls.sectors[1];
      TEST_CHECK("lvt s1 id == array index", s1->id == 1);
      TEST_CHECK("lvt s1 name", strcmp(s1->name, "entrance") == 0);
      TEST_CHECK("lvt s1 ambient", s1->ambient == FIXED(15));
      TEST_CHECK("lvt s1 floor_height (negated Y-up→Y-down)",
                 s1->floor_height == float_to_fixed16(5.0f));
      TEST_CHECK("lvt s1 ceiling_height (negated Y-up→Y-down)",
                 s1->ceiling_height == float_to_fixed16(-10.0f));
      TEST_CHECK("lvt s1 floor_tex", s1->floor_tex == &ls.textures[0]);
      TEST_CHECK("lvt s1 floor_offset.x", s1->floor_offset.x == float_to_fixed16(0.5f));
      TEST_CHECK("lvt s1 floor_offset.z", s1->floor_offset.z == float_to_fixed16(0.25f));
      TEST_CHECK("lvt s1 ceil_tex", s1->ceil_tex == &ls.textures[1]);
      TEST_CHECK("lvt s1 flags1", s1->flags1 == 0x8000);
      // s1 has no PIT/EXTERIOR: col heights equal raw heights
      TEST_CHECK("lvt s1 col_floor_height == floor_height",
                 s1->col_floor_height == s1->floor_height);
      TEST_CHECK("lvt s1 col_ceil_height == ceiling_height",
                 s1->col_ceil_height == s1->ceiling_height);
      TEST_CHECK("lvt s1 layer", s1->layer == 2);

      // OVERLAY: wall keyword maps to sign_tex
      Wall *lw1_0 = &s1->walls[0];
      TEST_CHECK("lvt s1 w0 mid_offset.x",
                 lw1_0->mid_offset.x == float_to_fixed16(1.0f) * 8);
      TEST_CHECK("lvt s1 w0 mid_offset.z",
                 lw1_0->mid_offset.z == float_to_fixed16(2.0f) * 8);
      TEST_CHECK("lvt s1 w0 mid_tex", lw1_0->mid_tex == &ls.textures[0]);
      TEST_CHECK("lvt s1 w0 wall_light", lw1_0->wall_light == FIXED(3));

      float lvt_len = fixed16_to_float(lw1_0->length);
      TEST_CHECK("lvt s1 w0 length ~10", fabsf(lvt_len - 10.0f) < 0.01f);

      // Min/max layer across both sectors
      TEST_CHECK("lvt min_layer", ls.min_layer == 1);
      TEST_CHECK("lvt max_layer", ls.max_layer == 2);
    }
  }

  // Invalid LVT
  {
    StreamReader sr_bad = stream_from_memory("NOT LVT DATA", 13);
    LevelState bad_lvt;
    bool bad_ok = level_load_geometry(&sr_bad, &bad_lvt);
    TEST_CHECK("bad LVT fails", !bad_ok);
    stream_close(&sr_bad);
  }

  // Hardening tests — malformed input must fail gracefully, never crash

  // Negative sector count
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\nNUMSECTORS -5\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("negative sector count fails", !r);
    stream_close(&sr2);
  }

  // Huge sector count
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\nNUMSECTORS 999999\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("huge sector count fails", !r);
    stream_close(&sr2);
  }

  // Negative texture count
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES -3\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("negative texture count fails", !r);
    stream_close(&sr2);
  }

  // Negative vertex count
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\nNUMSECTORS 1\n"
                       "  SECTOR 0\n  NAME x\n  AMBIENT 0\n"
                       "  FLOOR TEXTURE 0 0 0 0\n  FLOOR ALTITUDE 0\n"
                       "  CEILING TEXTURE 0 0 0 0\n  CEILING ALTITUDE -10\n"
                       "  SECOND ALTITUDE 0\n  FLAGS 0 0 0\n  LAYER 0\n"
                       "  VERTICES -1\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("negative vertex count fails", !r);
    stream_close(&sr2);
  }

  // Out-of-bounds vertex index in wall
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\nNUMSECTORS 1\n"
                       "  SECTOR 0\n  NAME x\n  AMBIENT 0\n"
                       "  FLOOR TEXTURE 0 0 0 0\n  FLOOR ALTITUDE 0\n"
                       "  CEILING TEXTURE 0 0 0 0\n  CEILING ALTITUDE -10\n"
                       "  SECOND ALTITUDE 0\n  FLAGS 0 0 0\n  LAYER 0\n"
                       "  VERTICES 2\n    X: 0 Z: 0\n    X: 10 Z: 0\n"
                       "  WALLS 1\n"
                       "    WALL LEFT: 0 RIGHT: 99 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("OOB vertex index fails", !r);
    stream_close(&sr2);
  }

  // Out-of-range adjoin is nullified (not crash)
  game_level_clear();
  {
    const char *data = "LEV 2.1\nLEVELNAME t\nPALETTE P\nMUSIC M\nPARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\nNUMSECTORS 1\n"
                       "  SECTOR 0\n  NAME x\n  AMBIENT 0\n"
                       "  FLOOR TEXTURE 0 0 0 0\n  FLOOR ALTITUDE 0\n"
                       "  CEILING TEXTURE 0 0 0 0\n  CEILING ALTITUDE -10\n"
                       "  SECOND ALTITUDE 0\n  FLAGS 0 0 0\n  LAYER 0\n"
                       "  VERTICES 4\n"
                       "    X: 0 Z: 0\n    X: 10 Z: 0\n"
                       "    X: 10 Z: 10\n    X: 0 Z: 10\n"
                       "  WALLS 4\n"
                       "    WALL LEFT: 0 RIGHT: 1 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: 99 MIRROR: 0 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 1 RIGHT: 2 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 2 RIGHT: 3 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 3 RIGHT: 0 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("OOB adjoin parses OK", r);
    if (r) {
      TEST_CHECK("OOB adjoin nullified", ls2.sectors[0].walls[0].next_sector == NULL);
      TEST_CHECK("OOB mirror nullified", ls2.sectors[0].walls[0].mirror_wall == NULL);
    }
    stream_close(&sr2);
  }

  // C-style comments parsed correctly
  game_level_clear();
  {
    const char *data = "LEV 2.1\n"
                       "// line comment\n"
                       "LEVELNAME commented\n"
                       "/* block\n   comment */ PALETTE P.PAL\n"
                       "MUSIC M.GMD\n"
                       "PARALLAX 0 0\n"
                       "TEXTURES 1\n  TEXTURE: A.BM\n"
                       "NUMSECTORS 1\n"
                       "  SECTOR 0\n  NAME x\n  AMBIENT 0\n"
                       "  FLOOR TEXTURE 0 0 0 0\n  FLOOR ALTITUDE 0\n"
                       "  CEILING TEXTURE 0 0 0 0\n  CEILING ALTITUDE -10\n"
                       "  SECOND ALTITUDE 0\n  FLAGS 0 0 0\n  LAYER 0\n"
                       "  VERTICES 4\n"
                       "    X: 0 Z: 0\n    X: 10 Z: 0\n"
                       "    X: 10 Z: 10\n    X: 0 Z: 10\n"
                       "  WALLS 4\n"
                       "    WALL LEFT: 0 RIGHT: 1 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 1 RIGHT: 2 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 2 RIGHT: 3 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n"
                       "    WALL LEFT: 3 RIGHT: 0 MID: 0 0 0 0 TOP: -1 0 0 0 "
                       "BOT: -1 0 0 0 SIGN: -1 0 0 ADJOIN: -1 MIRROR: -1 WALK: -1 "
                       "FLAGS: 0 0 0 LIGHT: 0\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("C-style comments parse OK", r);
    if (r) {
      TEST_CHECK("comment: level_name", strcmp(ls2.level_name, "commented") == 0);
      TEST_CHECK("comment: palette", strcmp(ls2.palette_name, "P.PAL") == 0);
      TEST_CHECK("comment: sector_count", ls2.sector_count == 1);
    }
    stream_close(&sr2);
  }

  // Negative LVT palette count
  game_level_clear();
  {
    const char *data = "LVT 1.1\nLEVELNAME t\nVERSION 1.0\nPALETTES -1\n";
    StreamReader sr2 = stream_from_memory(data, (int32_t)strlen(data));
    LevelState ls2;
    bool r = level_load_geometry(&sr2, &ls2);
    TEST_CHECK("negative palette count fails", !r);
    stream_close(&sr2);
  }

  game_level_clear();
  game_memory_shutdown();

  TEST_SUITE_END();
}
