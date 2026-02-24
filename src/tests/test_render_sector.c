#include "test_harness.h"

#include "render/display_list.h"
#include "render/render_limits.h"
#include "render/render_sector.h"
#include "world/sector.h"
#include "world/wall.h"
#include <string.h>

#define SCREEN_W       320
#define SCREEN_H       200
#define TEST_MAX_DEPTH 5

// Build a CW-wound 4-wall box sector in the XZ plane.
// CW winding in DF coords (X right, Z forward, viewed from above):
//   Wall 0: left side   (v0→v1, going +Z)
//   Wall 1: back wall   (v1→v2, going +X)  ← portal wall for adjoin tests
//   Wall 2: right side  (v2→v3, going -Z)
//   Wall 3: front wall  (v3→v0, going -X)

static void build_box_sector(Sector *sec, Wall walls[4], Vec2Fixed verts[4], int32_t id,
                             float x_min, float x_max, float z_min, float z_max,
                             float floor_h, float ceil_h) {
  memset(sec, 0, sizeof(*sec));
  memset(walls, 0, sizeof(Wall) * 4);

  verts[0] = (Vec2Fixed){float_to_fixed16(x_min), float_to_fixed16(z_min)};
  verts[1] = (Vec2Fixed){float_to_fixed16(x_min), float_to_fixed16(z_max)};
  verts[2] = (Vec2Fixed){float_to_fixed16(x_max), float_to_fixed16(z_max)};
  verts[3] = (Vec2Fixed){float_to_fixed16(x_max), float_to_fixed16(z_min)};

  int32_t next[4] = {1, 2, 3, 0};
  for (int32_t i = 0; i < 4; i++) {
    walls[i].w0 = &verts[i];
    walls[i].w1 = &verts[next[i]];
    walls[i].texel_length = float_to_fixed16(80.0f);
    walls[i].sector = sec;
    walls[i].next_sector = NULL;
  }

  sec->id = id;
  sec->walls = walls;
  sec->wall_count = 4;
  sec->start_wall = id * 4;
  sec->floor_height = float_to_fixed16(floor_h);
  sec->ceiling_height = float_to_fixed16(ceil_h);
  sec->ambient = float_to_fixed16(31.0f);
  sec->prev_draw_frame = 0;
}

// Set up RenderState + camera looking along +Z from origin.
static bool setup_render_state(RenderState *rs) {
  if (!render_state_init(rs, SCREEN_W, SCREEN_H, TEST_MAX_DEPTH)) {
    return false;
  }
  camera_set_projection(&rs->camera, SCREEN_W, SCREEN_H, 90.0f, 1.333f);
  return true;
}

void test_render_sector(void) {
  TEST_SUITE_BEGIN("render_sector");

  // 1. Init / destroy lifecycle
  {
    RenderState rs;
    bool ok = setup_render_state(&rs);
    TEST_CHECK("init succeeds", ok);
    TEST_CHECK("draw_frame starts at 0", rs.draw_frame == 0);
    TEST_CHECK("adjoin_depth starts at 0", rs.adjoin_depth == 0);
    TEST_CHECK("display list empty", rs.display_list.opaque_count == 0);
    render_state_destroy(&rs);
  }

  // 2. Reset clears orchestrator state
  {
    RenderState rs;
    setup_render_state(&rs);

    rs.adjoin_depth = 3;
    rs.display_list.opaque_count = 42;
    render_state_reset(&rs);

    TEST_CHECK("reset: adjoin_depth = 0", rs.adjoin_depth == 0);
    TEST_CHECK("reset: opaque_count = 0", rs.display_list.opaque_count == 0);

    render_state_destroy(&rs);
  }

  // 3. Double-draw prevention
  {
    RenderState rs;
    setup_render_state(&rs);

    Vec2Fixed verts[4];
    Wall walls[4];
    Sector sec;
    build_box_sector(&sec, walls, verts, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -10.0f);

    bool visited[1] = {false};
    rs.visited_sectors = visited;
    rs.visited_capacity = 1;

    render_draw_frame(&rs, &sec, 0.0f, -5.0f, 0.0f, 0, 0);
    int32_t count_after_first = rs.display_list.opaque_count;
    TEST_CHECK("double-draw: first draw produces entries", count_after_first > 0);

    // Call render_draw_sector again with same draw_frame — sector already
    // has prev_draw_frame == rs.draw_frame from the first call.
    Frustum cam_frustum;
    frustum_build_camera(&cam_frustum, rs.camera.focal_length, rs.camera.half_width,
                         rs.camera.y_plane_top, rs.camera.y_plane_bot, 0.98f,
                         NEAR_PLANE_EPSILON);
    render_draw_sector(&rs, &sec, &cam_frustum);
    TEST_CHECK("double-draw: no new entries",
               rs.display_list.opaque_count == count_after_first);

    render_state_destroy(&rs);
  }

  // 4. Max depth limiting
  {
    RenderState rs;
    setup_render_state(&rs);

    Vec2Fixed verts[4];
    Wall walls[4];
    Sector sec;
    build_box_sector(&sec, walls, verts, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -10.0f);
    sec.prev_draw_frame = 0;

    // Simulate being deeper than max depth.
    rs.draw_frame = 1;
    rs.adjoin_depth = rs.max_adjoin_depth + 1;

    Frustum cam_frustum;
    frustum_build_camera(&cam_frustum, rs.camera.focal_length, rs.camera.half_width,
                         rs.camera.y_plane_top, rs.camera.y_plane_bot, 0.98f,
                         NEAR_PLANE_EPSILON);

    render_draw_sector(&rs, &sec, &cam_frustum);
    TEST_CHECK("max depth: no entries", rs.display_list.opaque_count == 0);

    render_state_destroy(&rs);
  }

  // 5. Visited sector tracking
  {
    RenderState rs;
    setup_render_state(&rs);

    Vec2Fixed verts[4];
    Wall walls[4];
    Sector sec;
    build_box_sector(&sec, walls, verts, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -10.0f);

    bool visited[2] = {false, false};
    rs.visited_sectors = visited;
    rs.visited_capacity = 2;

    render_draw_frame(&rs, &sec, 0.0f, -5.0f, 0.0f, 0, 0);
    TEST_CHECK("visited: sector 0 marked", visited[0] == true);
    TEST_CHECK("visited: sector 1 untouched", visited[1] == false);

    render_state_destroy(&rs);
  }

  // 6. Bug #001 regression: portal recursion reaches neighbor sector
  {
    RenderState rs;
    setup_render_state(&rs);
    rs.max_adjoin_depth = TEST_MAX_DEPTH;

    // Sector A: box at z=[5,15], front wall is the portal to B.
    Vec2Fixed verts_a[4];
    Wall walls_a[4];
    Sector sec_a;
    build_box_sector(&sec_a, walls_a, verts_a, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -10.0f);

    // Sector B: box at z=[15,25], directly behind A.
    Vec2Fixed verts_b[4];
    Wall walls_b[4];
    Sector sec_b;
    build_box_sector(&sec_b, walls_b, verts_b, 1, -5.0f, 5.0f, 15.0f, 25.0f, 0.0f,
                     -10.0f);

    // Connect A's back wall (wall 1: v1→v2, at z_max=15) to sector B.
    walls_a[1].next_sector = &sec_b;

    // Connect B's front wall (wall 3: v3→v0, at z_min=15) back to A.
    walls_b[3].next_sector = &sec_a;

    bool visited[2] = {false, false};
    rs.visited_sectors = visited;
    rs.visited_capacity = 2;

    render_draw_frame(&rs, &sec_a, 0.0f, -5.0f, 0.0f, 0, 0);

    TEST_CHECK("bug001: sector A visited", visited[0] == true);
    TEST_CHECK("bug001: sector B visited (portal traversed)", visited[1] == true);

    render_state_destroy(&rs);
  }

  // 7. Same-height adjoin: no MID wall emitted for the portal
  {
    RenderState rs;
    setup_render_state(&rs);
    rs.max_adjoin_depth = TEST_MAX_DEPTH;

    Vec2Fixed verts_a[4];
    Wall walls_a[4];
    Sector sec_a;
    build_box_sector(&sec_a, walls_a, verts_a, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -10.0f);

    Vec2Fixed verts_b[4];
    Wall walls_b[4];
    Sector sec_b;
    build_box_sector(&sec_b, walls_b, verts_b, 1, -5.0f, 5.0f, 15.0f, 25.0f, 0.0f,
                     -10.0f);

    walls_a[1].next_sector = &sec_b;
    walls_b[3].next_sector = &sec_a;

    bool visited[2] = {false, false};
    rs.visited_sectors = visited;
    rs.visited_capacity = 2;

    render_draw_frame(&rs, &sec_a, 0.0f, -5.0f, 0.0f, 0, 0);

    // The portal wall (wall_a[1]) connects sectors with identical heights.
    // No MID wall should be emitted for it — only FLOOR and CEILING.
    int32_t portal_wall_idx = sec_a.start_wall + 1;
    bool found_mid_for_portal = false;
    for (int32_t i = 0; i < rs.display_list.opaque_count; i++) {
      DisplayListEntry *e = &rs.display_list.opaque[i];
      int32_t part = (int32_t)(e->data.flags_part & 0x0F);
      int32_t wall_id = (int32_t)(e->data.wall_tex_id & 0xFFFF);
      if (wall_id == portal_wall_idx && part == PART_MID_WALL) {
        found_mid_for_portal = true;
      }
    }
    TEST_CHECK("same-height adjoin: no MID wall for portal", !found_mid_for_portal);
    TEST_CHECK("same-height adjoin: both sectors visited", visited[0] && visited[1]);

    render_state_destroy(&rs);
  }

  // 8. Dadjoin: both portals traversed and MID emitted between openings
  {
    RenderState rs;
    setup_render_state(&rs);
    rs.max_adjoin_depth = TEST_MAX_DEPTH;

    // Sector A (outside): box at z=[5,15], floor=0, ceil=-48.
    Vec2Fixed verts_a[4];
    Wall walls_a[4];
    Sector sec_a;
    build_box_sector(&sec_a, walls_a, verts_a, 0, -5.0f, 5.0f, 5.0f, 15.0f, 0.0f, -48.0f);

    // Sector B (upper / roof): box at z=[15,25], floor=-20, ceil=-48.
    Vec2Fixed verts_b[4];
    Wall walls_b[4];
    Sector sec_b;
    build_box_sector(&sec_b, walls_b, verts_b, 1, -5.0f, 5.0f, 15.0f, 25.0f, -20.0f,
                     -48.0f);

    // Sector C (lower / door): box at z=[15,25], floor=0, ceil=-16.
    Vec2Fixed verts_c[4];
    Wall walls_c[4];
    Sector sec_c;
    build_box_sector(&sec_c, walls_c, verts_c, 2, -5.0f, 5.0f, 15.0f, 25.0f, 0.0f,
                     -16.0f);

    // Wall A[1] (back wall at z=15): dadjoin to both B (upper) and C (lower).
    walls_a[1].next_sector = &sec_b;
    walls_a[1].dadjoin_sector = &sec_c;

    // B and C front walls point back to A (regular single adjoins).
    walls_b[3].next_sector = &sec_a;
    walls_c[3].next_sector = &sec_a;

    bool visited[3] = {false, false, false};
    rs.visited_sectors = visited;
    rs.visited_capacity = 3;

    render_draw_frame(&rs, &sec_a, 0.0f, -10.0f, 0.0f, 0, 0);

    TEST_CHECK("dadjoin: sector A visited", visited[0] == true);
    TEST_CHECK("dadjoin: sector B visited (upper portal)", visited[1] == true);
    TEST_CHECK("dadjoin: sector C visited (lower portal)", visited[2] == true);

    // The dadjoined wall should emit a MID entry for the section between
    // the upper (floor=-20) and lower (ceil=-16) openings.
    int32_t portal_wall_idx = sec_a.start_wall + 1;
    bool found_mid = false;
    for (int32_t i = 0; i < rs.display_list.opaque_count; i++) {
      DisplayListEntry *e = &rs.display_list.opaque[i];
      int32_t part = (int32_t)(e->data.flags_part & 0x0F);
      int32_t wall_id = (int32_t)(e->data.wall_tex_id & 0xFFFF);
      if (wall_id == portal_wall_idx && part == PART_MID_WALL) {
        found_mid = true;
      }
    }
    TEST_CHECK("dadjoin: MID wall emitted between portals", found_mid);

    render_state_destroy(&rs);
  }

  TEST_SUITE_END();
}
