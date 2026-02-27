#include "test_harness.h"

#include "render/adjoin.h"
#include "render/camera.h"
#include "render/frustum.h"
#include "render/render_limits.h"
#include "render/wall_process.h"
#include "world/flags.h"
#include "world/sector.h"
#include "world/wall.h"
#include <math.h>
#include <string.h>

#define NEAR(a, b, eps) (fabsf((a) - (b)) < (eps))

void test_wall_process(void) {
  TEST_SUITE_BEGIN("wall processing");

  // Set up camera at origin, looking along +Z.
  CameraState cam;
  memset(&cam, 0, sizeof(cam));
  camera_set_projection(&cam, 320, 200, 90.0f, 1.333f);
  camera_compute_transform(&cam, 0.0f, 0.0f, 0.0f, 0, 0);

  // Build camera frustum.
  Frustum frustum;
  frustum_build_camera(&frustum, cam.focal_length, cam.half_width, cam.y_plane_top,
                       cam.y_plane_bot, 0.98f, 0.001f);

  // Wall facing the camera: DF CW winding (viewed from above, +Z forward)
  // means left-to-right from camera's view. Interior is on the camera side.
  Vec2Fixed v0 = {float_to_fixed16(-5.0f), float_to_fixed16(10.0f)};
  Vec2Fixed v1 = {float_to_fixed16(5.0f), float_to_fixed16(10.0f)};

  Sector sec;
  memset(&sec, 0, sizeof(sec));
  sec.floor_height = float_to_fixed16(0.0f);
  sec.ceiling_height = float_to_fixed16(-10.0f);
  sec.flags1 = 0;

  Wall wall;
  memset(&wall, 0, sizeof(wall));
  wall.w0 = &v0;
  wall.w1 = &v1;
  wall.texel_length = float_to_fixed16(80.0f);
  wall.next_sector = NULL;
  wall.sector = &sec;
  wall.wall_light = 0;
  wall.flags1 = 0;
  wall.sign_tex = NULL;

  WallSegment seg;
  bool visible =
      wall_process(&cam, &frustum, &wall, 0, 0.0f, -10.0f, 0.0f, 0.0f, &seg, CULL_ALL);
  TEST_CHECK("wall is visible", visible);
  TEST_CHECK("wall_x0 < wall_x1", seg.wall_x0 < seg.wall_x1);
  TEST_CHECK("depth vz0 ~10", NEAR(seg.vz0, 10.0f, 0.5f));
  TEST_CHECK("is_solid (no adjoin)", seg.is_solid);
  TEST_CHECK("draw_flags = WDF_MIDDLE", seg.draw_flags == WDF_MIDDLE);

  // Backface cull: reverse winding so it faces away from camera.
  Wall backwall;
  memset(&backwall, 0, sizeof(backwall));
  backwall.w0 = &v1;
  backwall.w1 = &v0;
  backwall.texel_length = float_to_fixed16(80.0f);
  backwall.next_sector = NULL;
  backwall.sector = &sec;

  WallSegment seg_back;
  bool back_visible =
      wall_process(&cam, &frustum, &backwall, 1, 0.0f, -10.0f, 0.0f, 0.0f, &seg_back,
                   CULL_ALL);
  TEST_CHECK("backface culled", !back_visible);

  // Wall behind camera — both vertices at negative z, culled by frustum.
  Vec2Fixed v2 = {float_to_fixed16(-5.0f), float_to_fixed16(-5.0f)};
  Vec2Fixed v3 = {float_to_fixed16(5.0f), float_to_fixed16(-5.0f)};
  Wall behind;
  memset(&behind, 0, sizeof(behind));
  behind.w0 = &v2;
  behind.w1 = &v3;
  behind.texel_length = float_to_fixed16(80.0f);
  behind.next_sector = NULL;
  behind.sector = &sec;

  WallSegment seg_behind;
  bool behind_visible =
      wall_process(&cam, &frustum, &behind, 2, 0.0f, -10.0f, 0.0f, 0.0f, &seg_behind,
                   CULL_ALL);
  TEST_CHECK("behind camera culled", !behind_visible);

  // Adjoin classification.
  Sector next_sec;
  memset(&next_sec, 0, sizeof(next_sec));
  next_sec.floor_height = float_to_fixed16(-2.0f);
  next_sec.ceiling_height = float_to_fixed16(-8.0f);

  Wall adjwall;
  memset(&adjwall, 0, sizeof(adjwall));
  adjwall.w0 = &v0;
  adjwall.w1 = &v1;
  adjwall.texel_length = float_to_fixed16(80.0f);
  adjwall.next_sector = &next_sec;
  adjwall.sector = &sec;
  adjwall.wall_light = 0;
  adjwall.flags1 = 0;
  adjwall.sign_tex = NULL;

  WallSegment seg_adj;
  // floor=0, ceil=-10 vs next_floor=-2, next_ceil=-8.
  // next_floor < floor => WDF_BOT; next_ceil > ceil => WDF_TOP.
  bool adj_visible =
      wall_process(&cam, &frustum, &adjwall, 3, 0.0f, -10.0f, -2.0f, -8.0f, &seg_adj,
                   CULL_ALL);
  TEST_CHECK("adjoin visible", adj_visible);
  TEST_CHECK("adjoin has WDF_TOP", (seg_adj.draw_flags & WDF_TOP) != 0);
  TEST_CHECK("adjoin has WDF_BOT", (seg_adj.draw_flags & WDF_BOT) != 0);
  TEST_CHECK("adjoin is_adjoin", seg_adj.is_adjoin);
  TEST_CHECK("adjoin not solid", !seg_adj.is_solid);

  // Deadjoin: next_floor >= next_ceil.
  WallSegment seg_dead;
  bool dead_visible =
      wall_process(&cam, &frustum, &adjwall, 4, 0.0f, -10.0f, 0.0f, 0.0f, &seg_dead,
                   CULL_ALL);
  TEST_CHECK("deadjoin visible", dead_visible);
  TEST_CHECK("deadjoin is_solid", seg_dead.is_solid);

  // Merge sort.
  {
    WallSegment segs[4];
    memset(segs, 0, sizeof(segs));
    segs[0].wall_x0 = 200;
    segs[1].wall_x0 = 50;
    segs[2].wall_x0 = 300;
    segs[3].wall_x0 = 10;

    wall_merge_sort(segs, 4);
    TEST_CHECK("sort [0]=10", segs[0].wall_x0 == 10);
    TEST_CHECK("sort [1]=50", segs[1].wall_x0 == 50);
    TEST_CHECK("sort [2]=200", segs[2].wall_x0 == 200);
    TEST_CHECK("sort [3]=300", segs[3].wall_x0 == 300);
  }

  // Dadjoin classification: has_dadjoin set when dadjoin_sector is present.
  {
    Sector dadj_sec;
    memset(&dadj_sec, 0, sizeof(dadj_sec));
    dadj_sec.floor_height = float_to_fixed16(0.0f);
    dadj_sec.ceiling_height = float_to_fixed16(-16.0f);

    Wall dadjwall;
    memset(&dadjwall, 0, sizeof(dadjwall));
    dadjwall.w0 = &v0;
    dadjwall.w1 = &v1;
    dadjwall.texel_length = float_to_fixed16(80.0f);
    dadjwall.next_sector = &next_sec;
    dadjwall.dadjoin_sector = &dadj_sec;
    dadjwall.sector = &sec;
    dadjwall.wall_light = 0;
    dadjwall.flags1 = 0;
    dadjwall.sign_tex = NULL;

    WallSegment seg_dadj;
    bool dadj_visible =
        wall_process(&cam, &frustum, &dadjwall, 5, 0.0f, -10.0f, -2.0f, -8.0f, &seg_dadj,
                     CULL_ALL);
    TEST_CHECK("dadjoin: visible", dadj_visible);
    TEST_CHECK("dadjoin: is_adjoin", seg_dadj.is_adjoin);
    TEST_CHECK("dadjoin: has_dadjoin", seg_dadj.has_dadjoin);
  }

  // No dadjoin when dadjoin_sector is NULL.
  {
    WallSegment seg_nodadj;
    bool nodadj_visible = wall_process(&cam, &frustum, &adjwall, 6, 0.0f, -10.0f, -2.0f,
                                       -8.0f, &seg_nodadj, CULL_ALL);
    TEST_CHECK("no dadjoin: visible", nodadj_visible);
    TEST_CHECK("no dadjoin: !has_dadjoin", !seg_nodadj.has_dadjoin);
  }

  // Bug-A regression: frustum-clipped wall must have correct texture U.
  // Wall extends past the left frustum edge (x=-20 at z=10 is outside 90° FOV).
  // After clipping, u_coord0 > 0 and total U span < texel_length.
  {
    Vec2Fixed vl0 = {float_to_fixed16(-20.0f), float_to_fixed16(10.0f)};
    Vec2Fixed vl1 = {float_to_fixed16(5.0f), float_to_fixed16(10.0f)};

    Wall lwall;
    memset(&lwall, 0, sizeof(lwall));
    lwall.w0 = &vl0;
    lwall.w1 = &vl1;
    lwall.texel_length = float_to_fixed16(80.0f);
    lwall.next_sector = NULL;
    lwall.sector = &sec;
    lwall.wall_light = 0;
    lwall.flags1 = 0;
    lwall.sign_tex = NULL;

    WallSegment seg_clip;
    bool clip_vis =
        wall_process(&cam, &frustum, &lwall, 5, 0.0f, -10.0f, 0.0f, 0.0f, &seg_clip,
                     CULL_ALL);
    TEST_CHECK("frustum clip: visible", clip_vis);

    float u_end =
        seg_clip.u_coord0 + seg_clip.du_dx * (float)(seg_clip.wall_x1 - seg_clip.wall_x0);
    TEST_CHECK("frustum clip: u_coord0 > 0 (left clipped)", seg_clip.u_coord0 > 0.1f);
    TEST_CHECK("frustum clip: u_end <= texel_length", u_end <= 80.0f + 0.5f);
  }

  // Bug-B regression: EdgePair Y bounds use the tighter (inner) sector heights.
  // A: floor=0, ceil=-10. B: floor=-2, ceil=-8.
  // Tighter ceiling = max(-10,-8) = -8. Tighter floor = min(0,-2) = -2.
  {
    WallSegment seg_ep;
    memset(&seg_ep, 0, sizeof(seg_ep));
    seg_ep.wall_x0 = 100;
    seg_ep.wall_x1 = 200;
    seg_ep.vz0 = 10.0f;
    seg_ep.vz1 = 10.0f;

    EdgePair ep;
    adjoin_compute_edge_pair(cam.focal_len_aspect, cam.proj_offset_y, 0.0f, 0.0f, -10.0f,
                             -2.0f, -8.0f, &seg_ep, &ep);

    float expect_ceil_y = (-8.0f) * cam.focal_len_aspect / 10.0f + cam.proj_offset_y;
    float expect_floor_y = (-2.0f) * cam.focal_len_aspect / 10.0f + cam.proj_offset_y;

    TEST_CHECK("edge pair: ceil Y uses tighter bound",
               NEAR(ep.y_ceil0, expect_ceil_y, 0.01f));
    TEST_CHECK("edge pair: floor Y uses tighter bound",
               NEAR(ep.y_floor0, expect_floor_y, 0.01f));
    TEST_CHECK("edge pair: ceil above floor on screen", ep.y_ceil0 < ep.y_floor0);
  }

  TEST_SUITE_END();
}
