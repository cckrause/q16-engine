# ISSUE-005 — Emit-Once Guard Blocks Wider Frustum Wall Emission

**Status**: Fixed
**Date**: 2026-02-27
**Severity**: High
**File(s)**: `src/game/render/render_sector.c`, `src/game/world/wall.h`, `src/game/world/level_parser.c`

## Symptom

Walls behind portals appear partially missing or show narrow vertical stripes. The missing geometry is angle-dependent: rotating the camera slightly causes the wall to appear or disappear. The pattern follows the angular width of the portal opening — only a thin slice of the wall behind the portal is drawn, matching the frustum of whichever portal path processed the wall first.

Disabling the emit-once guard (culling debug toggle `8:emit1=off`) resolved all missing geometry, confirming this as the source.

## Root Cause

When a wall was first emitted through a narrow portal path, the `prev_emit_frame` flag marked it as "done for this frame." A subsequent wider portal path reaching the same wall skipped emission entirely:

```c
if (wall->prev_emit_frame == rs->draw_frame) {
    continue;  // wall already emitted — skip regardless of frustum width
}
```

The first emission used a narrow frustum clip range (e.g. `t_clip0=0.4, t_clip1=0.5`), producing geometry covering only a fraction of the wall. The wider path (e.g. `t_clip0=0.0, t_clip1=1.0`) was blocked, leaving the rest of the wall invisible.

## Fix

Replaced the blanket skip with an in-place update mechanism. On first emission, the wall records its display list entry positions and clip range:

```c
wall->emit_frame = rs->draw_frame;
wall->emit_t0 = seg->t_clip0;
wall->emit_t1 = seg->t_clip1;
wall->emit_opaque_idx = op_before;
wall->emit_opaque_count = rs->display_list.opaque_count - op_before;
wall->emit_trans_idx = tr_before;
wall->emit_trans_count = rs->display_list.transparent_count - tr_before;
```

On subsequent encounters with a wider clip range, the existing display list entries are updated in-place with the merged (widest) vertex positions. No duplicate entries are created:

```c
if (seg->t_clip0 < wall->emit_t0 || seg->t_clip1 > wall->emit_t1) {
    float mt0 = min(seg->t_clip0, wall->emit_t0);
    float mt1 = max(seg->t_clip1, wall->emit_t1);
    // Recompute cam-space vertices for merged range, update display list entries
    for (k = 0; k < wall->emit_opaque_count; k++)
        rs->display_list.opaque[wall->emit_opaque_idx + k].pos = new_pos;
    wall->emit_t0 = mt0;
    wall->emit_t1 = mt1;
}
```

## Verification

- Build succeeds with no warnings.
- All unit tests pass.
- Visually verified in TOWN level: walls behind portals render completely at all camera angles.
- No Z-fighting stripes from duplicate emission.
