// ===========================================================================
// Sector Traversal Orchestrator
// ===========================================================================
// Per-frame entry point that walks the portal graph, processes walls,
// fills the S-Buffer, builds the display list, and sorts objects.

#include "render/render_sector.h"
#include "debug/debug_log.h"
#include "world/flags.h"
#include "world/sector.h"
#include "world/wall.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int32_t s_sector_visits;

bool render_state_init(RenderState *rs, int32_t screen_width, int32_t screen_height,
                       int32_t max_adjoin_depth) {
  memset(rs, 0, sizeof(RenderState));

  if (!sbuffer_init(&rs->sbuffer, SEG_CLIP_POOL_SIZE))
    return false;

  if (!depth_buffer_init(&rs->depth, screen_width, max_adjoin_depth))
    goto fail_depth;

  if (!render_window_init(&rs->window, screen_width, screen_height, max_adjoin_depth))
    goto fail_window;

  if (!flat_init(&rs->flat, screen_width))
    goto fail_flat;

  if (!display_list_init(&rs->display_list, MAX_DISP_ITEMS, MAX_PORTALS))
    goto fail_dlist;

  int32_t depth_levels = max_adjoin_depth + 1;
  rs->seg_pool =
      (WallSegment *)malloc((size_t)depth_levels * MAX_WALL_SEG * sizeof(WallSegment));
  if (!rs->seg_pool)
    goto fail_seg_pool;

  rs->adjoin_pool = (AdjoinList *)malloc((size_t)depth_levels * sizeof(AdjoinList));
  if (!rs->adjoin_pool)
    goto fail_adjoin_pool;

  frustum_stack_init(&rs->frustum_stack);
  rs->draw_frame = 0;
  rs->adjoin_depth = 0;
  rs->max_adjoin_depth = max_adjoin_depth;
  rs->portals_traversed = 0;
  rs->max_portals = 4096;
  rs->cull_mask = CULL_ALL;
  rs->visited_sectors = NULL;
  rs->visited_capacity = 0;
  return true;

fail_adjoin_pool:
  free(rs->seg_pool);
fail_seg_pool:
  display_list_destroy(&rs->display_list);
fail_dlist:
  flat_destroy(&rs->flat);
fail_flat:
  render_window_destroy(&rs->window);
fail_window:
  depth_buffer_destroy(&rs->depth);
fail_depth:
  sbuffer_destroy(&rs->sbuffer);
  return false;
}

void render_state_destroy(RenderState *rs) {
  free(rs->adjoin_pool);
  free(rs->seg_pool);
  rs->adjoin_pool = NULL;
  rs->seg_pool = NULL;
  display_list_destroy(&rs->display_list);
  flat_destroy(&rs->flat);
  render_window_destroy(&rs->window);
  depth_buffer_destroy(&rs->depth);
  sbuffer_destroy(&rs->sbuffer);
}

void render_state_reset(RenderState *rs) {
  sbuffer_reset(&rs->sbuffer);
  depth_buffer_reset(&rs->depth);
  render_window_reset(&rs->window);
  flat_reset(&rs->flat, rs->camera.screen_height);
  display_list_reset(&rs->display_list);
  rs->adjoin_depth = 0;
  rs->portals_traversed = 0;
  rs->cull_count_backface = 0;
  rs->cull_count_frustum = 0;
  rs->cull_count_sbuffer = 0;
  rs->cull_count_dfs = 0;
  rs->cull_count_budget = 0;
  if (rs->visited_sectors && rs->visited_capacity > 0)
    memset(rs->visited_sectors, 0, (size_t)rs->visited_capacity * sizeof(bool));
}

// Emit display list entries for a wall segment.
// Heights are derived from the Wall's sector pointers (actual world heights,
// no PIT/EXTERIOR inflation) and packed into each entry's y_bot/y_top.

static void emit_wall_entries(RenderState *rs, const WallSegment *seg,
                              float sector_ambient, int32_t sector_id) {
  Wall *w = seg->src_wall;
  float wall_light = fixed16_to_float(w->wall_light);
  int32_t light_int = (int32_t)roundf(wall_light);
  bool flip = (w->flags1 & WF1_FLIP_HORIZ) != 0;
  bool fullbright = lighting_is_fullbright(sector_ambient);
  int32_t next_sid = seg->is_adjoin ? w->next_sector->id : 0x3FFFFF;

  float floor_h = fixed16_to_float(w->sector->floor_height);
  float ceil_h = fixed16_to_float(w->sector->ceiling_height);

  float next_floor = floor_h, next_ceil = ceil_h;
  if (w->next_sector) {
    next_floor = fixed16_to_float(w->next_sector->floor_height);
    next_ceil = fixed16_to_float(w->next_sector->ceiling_height);
  }

  bool has_dadj = seg->has_dadjoin && w->dadjoin_sector != NULL;
  float dadj_floor = 0.0f, dadj_ceil = 0.0f;
  if (has_dadj) {
    dadj_floor = fixed16_to_float(w->dadjoin_sector->floor_height);
    dadj_ceil = fixed16_to_float(w->dadjoin_sector->ceiling_height);
  }

  DisplayListEntry entry;
  entry.pos.v0x = seg->vx0;
  entry.pos.v0z = seg->vz0;
  entry.pos.v1x = seg->vx1;
  entry.pos.v1z = seg->vz1;

  entry.data.sector_id = (uint32_t)sector_id;
  entry.data.light_info = display_list_pack_light(light_int, flip, 0, 0);

  // Mid wall: solid walls, deadjoins, and the solid section between dadjoin
  // portals. Open single-adjoins never draw MID (transparent mid-textures
  // like grates are handled via WF1_ADJ_MID_TEX below).
  if (!seg->is_adjoin || seg->is_solid || (seg->draw_flags & WDF_MIDDLE)) {
    if (has_dadj && (seg->draw_flags & WDF_MIDDLE)) {
      // Dadjoin MID: wall between the two portal openings.
      // Spans from dadjoin ceiling to adjoin floor.
      entry.pos.y_bot = dadj_ceil;
      entry.pos.y_top = next_floor;
    } else {
      entry.pos.y_bot = floor_h;
      entry.pos.y_top = ceil_h;
    }
    entry.data.flags_part = display_list_pack_flags(
        PART_MID_WALL, false, false, fullbright, true, false, false, next_sid);
    entry.data.wall_tex_id = display_list_pack_wall_tex(seg->wall_index, 0);
    display_list_add_opaque(&rs->display_list, &entry);
  }

  if (seg->draw_flags & WDF_TOP) {
    // Top piece: adjoin ceiling → own ceiling (same for adjoin and dadjoin).
    entry.pos.y_bot = next_ceil;
    entry.pos.y_top = ceil_h;
    entry.data.flags_part = display_list_pack_flags(
        PART_TOP_WALL, false, false, fullbright, true, false, false, next_sid);
    entry.data.wall_tex_id = display_list_pack_wall_tex(seg->wall_index, 0);
    display_list_add_opaque(&rs->display_list, &entry);
  }

  if (seg->draw_flags & WDF_BOT) {
    if (has_dadj) {
      // Dadjoin BOT: own floor → dadjoin floor, referencing dadjoin sector.
      entry.pos.y_bot = floor_h;
      entry.pos.y_top = dadj_floor;
      int32_t dadj_sid = w->dadjoin_sector->id;
      entry.data.flags_part = display_list_pack_flags(
          PART_BOT_WALL, false, false, fullbright, true, false, false, dadj_sid);
    } else {
      // Regular adjoin BOT: own floor → adjoin floor.
      entry.pos.y_bot = floor_h;
      entry.pos.y_top = next_floor;
      entry.data.flags_part = display_list_pack_flags(
          PART_BOT_WALL, false, false, fullbright, true, false, false, next_sid);
    }
    entry.data.wall_tex_id = display_list_pack_wall_tex(seg->wall_index, 0);
    display_list_add_opaque(&rs->display_list, &entry);
  }

  // Floor and ceiling entries.
  bool is_sky_ceil = (w->sector->flags1 & SEC_FLAG1_EXTERIOR) != 0;
  bool is_sky_floor = (w->sector->flags1 & SEC_FLAG1_PIT) != 0;

  entry.pos.y_bot = floor_h;
  entry.pos.y_top = floor_h;
  entry.data.flags_part = display_list_pack_flags(PART_FLOOR, false, false, fullbright,
                                                  true, false, is_sky_floor, next_sid);
  entry.data.wall_tex_id = display_list_pack_wall_tex(seg->wall_index, 0);
  display_list_add_opaque(&rs->display_list, &entry);

  entry.pos.y_bot = ceil_h;
  entry.pos.y_top = ceil_h;
  entry.data.flags_part = display_list_pack_flags(PART_CEILING, false, false, fullbright,
                                                  true, false, is_sky_ceil, next_sid);
  display_list_add_opaque(&rs->display_list, &entry);

  // Transparent mid-texture (adjoin with WF1_ADJ_MID_TEX): drawn in
  // transparent pass AFTER adjoin recursion.
  if (seg->is_adjoin && (w->flags1 & WF1_ADJ_MID_TEX)) {
    // Opening bounds (Y-down): tighter floor = min, tighter ceiling = max.
    float open_bot = floor_h < next_floor ? floor_h : next_floor;
    float open_top = ceil_h > next_ceil ? ceil_h : next_ceil;
    entry.pos.y_bot = open_bot;
    entry.pos.y_top = open_top;
    entry.data.flags_part = display_list_pack_flags(
        PART_MID_WALL, false, false, fullbright, false, false, false, next_sid);
    display_list_add_transparent(&rs->display_list, &entry);
  }

  // Sign overlays.
  if (w->sign_tex) {
    int32_t sign_part = PART_MID_SIGN;
    if (seg->draw_flags & WDF_TOP)
      sign_part = PART_TOP_SIGN;
    if (seg->draw_flags & WDF_BOT)
      sign_part = PART_BOT_SIGN;

    entry.pos.y_bot = floor_h;
    entry.pos.y_top = ceil_h;
    bool illum = (w->flags1 & WF1_ILLUM_SIGN) != 0;
    entry.data.flags_part = display_list_pack_flags(sign_part, false, false, illum, false,
                                                    false, false, next_sid);
    display_list_add_transparent(&rs->display_list, &entry);
  }
}

void render_draw_sector(RenderState *rs, Sector *sector, const Frustum *frustum) {
  s_sector_visits++;
  if (debug_log_is_active() && (s_sector_visits & 0xFF) == 0)
    LOG_TRACE_INDENT("render", rs->adjoin_depth + 3,
                     "HEARTBEAT visits=%d depth=%d sec=%d portals=%d", s_sector_visits,
                     rs->adjoin_depth, sector->id, rs->portals_traversed);

  bool trace = rs->debug_trace;

  if (rs->adjoin_depth > rs->max_adjoin_depth) {
    if (trace)
      LOG_TRACE_INDENT("render", rs->adjoin_depth + 3,
                       "[SKIP] sec %d — depth %d > max %d", sector->id, rs->adjoin_depth,
                       rs->max_adjoin_depth);
    return;
  }

  if ((rs->cull_mask & CULL_PORTAL_BUDGET) && rs->portals_traversed >= rs->max_portals) {
    rs->cull_count_budget++;
    if (trace)
      LOG_TRACE_INDENT("render", rs->adjoin_depth + 3,
                       "[SKIP] sec %d — portal budget exhausted (%d)", sector->id,
                       rs->portals_traversed);
    return;
  }

  if (trace && sector->prev_draw_frame == rs->draw_frame)
    LOG_TRACE_INDENT("render", rs->adjoin_depth + 3,
                     "[RE-ENTER] sec %d  frustum_planes=%d", sector->id,
                     frustum ? frustum->plane_count : -1);
  sector->prev_draw_frame = rs->draw_frame;

  if (trace)
    LOG_TRACE_INDENT("render", rs->adjoin_depth + 3,
                     "[ENTER] sec %d  depth=%d  frustum_planes=%d", sector->id,
                     rs->adjoin_depth, frustum ? frustum->plane_count : -1);

  // Track visited sectors for debug visualization.
  if (rs->visited_sectors && sector->id >= 0 && sector->id < rs->visited_capacity)
    rs->visited_sectors[sector->id] = true;

  float floor_h = fixed16_to_float(sector->floor_height);
  float ceil_h = fixed16_to_float(sector->ceiling_height);
  float ambient = fixed16_to_float(sector->ambient);
  int32_t sector_id = sector->id;

  // Set flat sky flags for this sector.
  rs->flat.is_exterior_ceiling = (sector->flags1 & SEC_FLAG1_EXTERIOR) != 0;
  rs->flat.is_pit_floor = (sector->flags1 & SEC_FLAG1_PIT) != 0;

  // Process walls: cull, clip, project.
  WallSegment *segments = rs->seg_pool + (size_t)rs->adjoin_depth * MAX_WALL_SEG;
  int32_t seg_count = 0;

#if ENGINE_FORMAT == ENGINE_FORMAT_DF
  bool nowall = (sector->flags1 & SEC_FLAG1_NOWALL_DRAW) != 0;
#else
  bool nowall = false;
#endif

  AdjoinList *adjoin_list = &rs->adjoin_pool[rs->adjoin_depth];
  adjoin_list_reset(adjoin_list);

  for (int32_t i = 0; i < sector->wall_count && seg_count < MAX_WALL_SEG; i++) {
    Wall *wall = &sector->walls[i];
    int32_t wall_idx = sector->start_wall + i;

    if ((rs->cull_mask & CULL_DFS_MARKING) && wall->traverse_frame == rs->draw_frame) {
      rs->cull_count_dfs++;
      if (trace && wall->next_sector)
        LOG_TRACE_INDENT("render", rs->adjoin_depth + 4,
                         "wall %d (->sec %d) SKIP dfs-marked", i, wall->next_sector->id);
      continue;
    }

    float next_floor = 0.0f, next_ceil = 0.0f;
    if (wall->next_sector) {
      next_floor = fixed16_to_float(wall->next_sector->floor_height);
      next_ceil = fixed16_to_float(wall->next_sector->ceiling_height);
    }

    float dadj_floor = 0.0f, dadj_ceil = 0.0f;
    if (wall->dadjoin_sector) {
      dadj_floor = fixed16_to_float(wall->dadjoin_sector->floor_height);
      dadj_ceil = fixed16_to_float(wall->dadjoin_sector->ceiling_height);
    }

    WallSegment *seg = &segments[seg_count];
    if (!wall_process(&rs->camera, frustum, wall, wall_idx, floor_h, ceil_h, next_floor,
                      next_ceil, seg, rs->cull_mask)) {
      if (trace && wall->next_sector)
        LOG_TRACE_INDENT("render", rs->adjoin_depth + 4,
                         "wall %d (->sec %d) CULLED by wall_process", i,
                         wall->next_sector->id);
      continue;
    }
    if (trace && wall->next_sector)
      LOG_TRACE_INDENT(
          "render", rs->adjoin_depth + 4,
          "wall %d (->sec %d) PASSED  vx=[%.3f,%.3f] vz=[%.3f,%.3f] adjoin=%d "
          "solid=%d dadjoin=%d",
          i, wall->next_sector->id, seg->vx0, seg->vx1, seg->vz0, seg->vz1,
          seg->is_adjoin, seg->is_solid, seg->has_dadjoin);

    // Double adjoin: override draw flags and solidity based on both openings.
    // ADJOIN/MIRROR = upper portal, DADJOIN/DMIRROR = lower portal.
    // The solid wall between the two openings is emitted as MID.
    bool has_dadjoin = seg->has_dadjoin && wall->dadjoin_sector && seg->is_adjoin;
    bool dadj_upper_open = false;
    bool dadj_lower_open = false;
    if (has_dadjoin) {
      float vis_ceil_upper = ceil_h > next_ceil ? ceil_h : next_ceil;
      float vis_floor_upper = floor_h < next_floor ? floor_h : next_floor;
      float vis_ceil_lower = ceil_h > dadj_ceil ? ceil_h : dadj_ceil;
      float vis_floor_lower = floor_h < dadj_floor ? floor_h : dadj_floor;

      dadj_upper_open = (vis_ceil_upper < vis_floor_upper);
      dadj_lower_open = (vis_ceil_lower < vis_floor_lower);

      uint32_t dflags = 0;
      if (next_ceil > ceil_h)
        dflags |= WDF_TOP;
      if (dadj_floor < floor_h)
        dflags |= WDF_BOT;
      if (vis_floor_upper < vis_ceil_lower)
        dflags |= WDF_MIDDLE;

      seg->draw_flags = dflags;
      seg->is_solid = !dadj_upper_open && !dadj_lower_open;
    }

    // Exterior adjoin overrides: sky/pit sectors suppress wall parts so the
    // portal is fully or partially open.
    if (seg->is_adjoin && wall->next_sector) {
      if (wall->next_sector->flags1 & SEC_FLAG1_EXT_FLOOR_ADJ) {
        seg->draw_flags = 0;
        seg->is_solid = false;
      } else if (wall->next_sector->flags1 & SEC_FLAG1_EXT_ADJ) {
        seg->draw_flags &= ~WDF_TOP;
        seg->is_solid = false;
      }
    }

    if ((rs->cull_mask & CULL_SBUFFER) && (seg->is_solid || !seg->is_adjoin)) {
      float proj0 = sbuffer_project(seg->vx0, seg->vz0);
      float proj1 = sbuffer_project(seg->vx1, seg->vz1);
      SBufferSeg *sb_seg = sbuffer_insert(
          &rs->sbuffer, proj0, proj1, seg->vx0, seg->vz0, seg->vx1, seg->vz1,
          seg->normal_x, seg->normal_z, seg->normal_d, wall_idx, false, wall);
      if (!sb_seg) {
        rs->cull_count_sbuffer++;
        continue;
      }
    }

    if (!nowall) {
      if (wall->emit_frame != rs->draw_frame) {
        int32_t op_before = rs->display_list.opaque_count;
        int32_t tr_before = rs->display_list.transparent_count;
        emit_wall_entries(rs, seg, ambient, sector_id);
        wall->emit_frame = rs->draw_frame;
        wall->emit_t0 = seg->t_clip0;
        wall->emit_t1 = seg->t_clip1;
        wall->emit_opaque_idx = op_before;
        wall->emit_opaque_count = rs->display_list.opaque_count - op_before;
        wall->emit_trans_idx = tr_before;
        wall->emit_trans_count = rs->display_list.transparent_count - tr_before;
      } else if (seg->t_clip0 < wall->emit_t0 || seg->t_clip1 > wall->emit_t1) {
        float mt0 = seg->t_clip0 < wall->emit_t0 ? seg->t_clip0 : wall->emit_t0;
        float mt1 = seg->t_clip1 > wall->emit_t1 ? seg->t_clip1 : wall->emit_t1;

        float vx0_raw, vz0_raw, vx1_raw, vz1_raw;
        camera_transform_vertex_xz(&rs->camera, fixed16_to_float(wall->w0->x),
                                   fixed16_to_float(wall->w0->z), &vx0_raw, &vz0_raw);
        camera_transform_vertex_xz(&rs->camera, fixed16_to_float(wall->w1->x),
                                   fixed16_to_float(wall->w1->z), &vx1_raw, &vz1_raw);

        float dx = vx1_raw - vx0_raw;
        float dz = vz1_raw - vz0_raw;
        float mvx0 = vx0_raw + mt0 * dx;
        float mvz0 = vz0_raw + mt0 * dz;
        float mvx1 = vx0_raw + mt1 * dx;
        float mvz1 = vz0_raw + mt1 * dz;

        for (int32_t k = 0; k < wall->emit_opaque_count; k++) {
          DisplayListEntry *e = &rs->display_list.opaque[wall->emit_opaque_idx + k];
          e->pos.v0x = mvx0;
          e->pos.v0z = mvz0;
          e->pos.v1x = mvx1;
          e->pos.v1z = mvz1;
        }
        for (int32_t k = 0; k < wall->emit_trans_count; k++) {
          DisplayListEntry *e = &rs->display_list.transparent[wall->emit_trans_idx + k];
          e->pos.v0x = mvx0;
          e->pos.v0z = mvz0;
          e->pos.v1x = mvx1;
          e->pos.v1z = mvz1;
        }

        wall->emit_t0 = mt0;
        wall->emit_t1 = mt1;
      }
    }

    // Queue visible portals for recursion.
    if (seg->is_adjoin && !seg->is_solid) {
      if (!has_dadjoin) {
        EdgePair edge;
        adjoin_compute_edge_pair(rs->camera.focal_len_aspect, rs->camera.proj_offset_y,
                                 rs->camera.pos_y, floor_h, ceil_h, next_floor, next_ceil,
                                 seg, &edge);
        adjoin_list_add(adjoin_list, seg, &edge, wall->next_sector);
      } else {
        if (dadj_upper_open) {
          EdgePair edge;
          adjoin_compute_edge_pair(rs->camera.focal_len_aspect, rs->camera.proj_offset_y,
                                   rs->camera.pos_y, floor_h, ceil_h, next_floor,
                                   next_ceil, seg, &edge);
          adjoin_list_add(adjoin_list, seg, &edge, wall->next_sector);
        }
        if (dadj_lower_open) {
          EdgePair dadj_edge;
          adjoin_compute_edge_pair(rs->camera.focal_len_aspect, rs->camera.proj_offset_y,
                                   rs->camera.pos_y, floor_h, ceil_h, dadj_floor,
                                   dadj_ceil, seg, &dadj_edge);
          adjoin_list_add(adjoin_list, seg, &dadj_edge, wall->dadjoin_sector);
        }
      }
    }

    seg_count++;
  }

  // NOTE: Do NOT sort segments[] here. The adjoin_list stores pointers into
  // this array, and sorting would invalidate them. Display list entries are
  // already emitted during the wall loop above; the GPU handles draw order.

  // Recurse through portals, merging multiple portals to the same target
  // sector into a single wider frustum. Without merging, the first portal
  // marks the sector as visited and subsequent portals are skipped, causing
  // walls visible through the wider opening to be incorrectly culled.
  bool adjoin_done[MAX_ADJOIN_SEG];
  if (adjoin_list->count > 0)
    memset(adjoin_done, 0, sizeof(bool) * (size_t)adjoin_list->count);

  for (int32_t i = 0; i < adjoin_list->count; i++) {
    if (adjoin_done[i])
      continue;

    AdjoinEntry *ae = &adjoin_list->entries[i];
    Sector *target = ae->next_sector;

    // Collect portal vertices from all walls adjoining to this target sector.
    float portal_vx[64];
    float portal_vz[64];
    int32_t pv_count = 0;
    int32_t merged_x0 = ae->edge_pair.x0;
    int32_t merged_x1 = ae->edge_pair.x1;

    Wall *merged_walls[32];
    int32_t merged_wall_count = 0;

    for (int32_t j = i; j < adjoin_list->count; j++) {
      AdjoinEntry *aj = &adjoin_list->entries[j];
      if (aj->next_sector != target)
        continue;
      adjoin_done[j] = true;

      if (merged_wall_count < 32)
        merged_walls[merged_wall_count++] = aj->seg->src_wall;

      if (pv_count + 2 > 64)
        continue;

      // Use the frustum-clipped segment vertices so the child frustum
      // never exceeds the parent's angular bounds. Using raw (unclipped)
      // wall vertices would produce an oversized frustum when the wall
      // extends far beyond the visible opening, wasting visit budget on
      // sectors that contribute no visible geometry (e.g. 606↔609 cycle).
      float pw0x = aj->seg->vx0, pw0z = aj->seg->vz0;
      float pw1x = aj->seg->vx1, pw1z = aj->seg->vz1;

      if (trace)
        LOG_TRACE_INDENT(
            "render", rs->adjoin_depth + 4,
            "portal->sec %d  clipped cam-space v0=(%.3f,%.3f) v1=(%.3f,%.3f)", target->id,
            pw0x, pw0z, pw1x, pw1z);

      if (!frustum_clip_near(&pw0x, &pw0z, &pw1x, &pw1z, NEAR_PLANE_EPSILON, NULL))
        continue;

      portal_vx[pv_count] = pw0x;
      portal_vz[pv_count] = pw0z;
      pv_count++;
      portal_vx[pv_count] = pw1x;
      portal_vz[pv_count] = pw1z;
      pv_count++;

      if (aj->edge_pair.x0 < merged_x0)
        merged_x0 = aj->edge_pair.x0;
      if (aj->edge_pair.x1 > merged_x1)
        merged_x1 = aj->edge_pair.x1;
    }

    if (pv_count < 2)
      continue;

    // Build the portal frustum from the two outermost vertices (widest
    // angular span as seen from the camera at the origin).
    int32_t left_idx = 0, right_idx = 0;
    float left_ratio = portal_vx[0] / portal_vz[0];
    float right_ratio = left_ratio;
    for (int32_t k = 1; k < pv_count; k++) {
      float ratio = portal_vx[k] / portal_vz[k];
      if (ratio < left_ratio) {
        left_ratio = ratio;
        left_idx = k;
      }
      if (ratio > right_ratio) {
        right_ratio = ratio;
        right_idx = k;
      }
    }

    // Save state.
    AdjoinSaveState save;
    adjoin_save_state(&save, rs->window.min_x, rs->window.max_x, rs->window.min_y,
                      rs->window.max_y, ambient, ambient * 0.875f);

    // Enter adjoin with merged x range.
    render_window_enter_adjoin(&rs->window, merged_x0, merged_x1);
    depth_buffer_enter_adjoin(&rs->depth, merged_x0, merged_x1);
    rs->adjoin_depth++;

    // DFS path marking: mark all merged portal walls before recursion
    // so they are skipped if the child re-enters this sector.
    for (int32_t m = 0; m < merged_wall_count; m++)
      merged_walls[m]->traverse_frame = rs->draw_frame;

    rs->portals_traversed++;

    if (rs->cull_mask & CULL_PORTAL_FRUSTUM) {
      Frustum portal_frustum;
      float pvx[2] = {portal_vx[left_idx], portal_vx[right_idx]};
      float pvz[2] = {portal_vz[left_idx], portal_vz[right_idx]};
      frustum_build_portal(&portal_frustum, pvx, pvz, 2);

      if (trace)
        LOG_TRACE_INDENT("render", rs->adjoin_depth + 4,
                         "portal->sec %d  frustum planes=%d", target->id,
                         portal_frustum.plane_count);

      frustum_stack_push(&rs->frustum_stack, &portal_frustum);
      render_draw_sector(rs, target, frustum_stack_top(&rs->frustum_stack));
      frustum_stack_pop(&rs->frustum_stack);
    } else {
      if (trace)
        LOG_TRACE_INDENT("render", rs->adjoin_depth + 4,
                         "portal->sec %d  PORTAL_FRUSTUM OFF — using parent frustum",
                         target->id);
      render_draw_sector(rs, target, frustum);
    }

    for (int32_t m = 0; m < merged_wall_count; m++)
      merged_walls[m]->traverse_frame = 0;

    // Exit adjoin.
    rs->adjoin_depth--;

#if ENGINE_FORMAT == ENGINE_FORMAT_DF
    bool is_subsector = (sector->flags1 & SEC_FLAG1_SUBSECTOR) != 0;
#else
    bool is_subsector = false;
#endif
    depth_buffer_exit_adjoin(&rs->depth, merged_x0, merged_x1, !is_subsector);
    render_window_exit_adjoin(&rs->window);

    // Restore state.
    float dummy_ambient, dummy_scaled;
    adjoin_restore_state(&save, &rs->window.min_x, &rs->window.max_x, &rs->window.min_y,
                         &rs->window.max_y, &dummy_ambient, &dummy_scaled);
  }

  // Sort objects for this sector (back-to-front, bridge priority).
  if (sector->object_count > 0 && sector->object_list) {
    // Allocate on the stack — MAX 128 objects per sector is reasonable.
    ObjectSortEntry obj_entries[128];
    int32_t max_objs = sector->object_capacity;
    if (max_objs > 128)
      max_objs = 128;

    int32_t obj_count =
        object_sort_transform(&rs->camera, sector->object_list, max_objs, obj_entries);
    object_sort(obj_entries, obj_count);

    // Object display list emission is a HAL concern — the orchestrator
    // only ensures they are sorted. The caller iterates obj_entries.
  }
}

void render_draw_frame(RenderState *rs, Sector *player_sector, float eye_x, float eye_y,
                       float eye_z, Angle14 yaw, Angle14 pitch) {
  s_sector_visits = 0;
  rs->draw_frame++;
  render_state_reset(rs);

  camera_compute_transform(&rs->camera, eye_x, eye_y, eye_z, yaw, pitch);

  Frustum camera_frustum;
  frustum_build_camera(&camera_frustum, rs->camera.focal_length, rs->camera.half_width,
                       rs->camera.y_plane_top, rs->camera.y_plane_bot, 0.98f,
                       NEAR_PLANE_EPSILON);

  frustum_stack_push(&rs->frustum_stack, &camera_frustum);
  render_draw_sector(rs, player_sector, frustum_stack_top(&rs->frustum_stack));
  frustum_stack_pop(&rs->frustum_stack);
}
