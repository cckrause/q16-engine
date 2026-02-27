# ISSUE-004 — Multi-Wall Portal Frustum Too Narrow (Missing Portal Merge)

**Status**: Fixed
**Date**: 2026-02-25
**Severity**: High
**File(s)**: `src/game/render/render_sector.c` (portal recursion loop)

## Symptom

Walls visible through a portal opening are incorrectly culled when the camera angle causes multiple walls in a parent sector to adjoin to the same child sector. Sectors "pop" invisible when the camera rotates slightly, despite the portal opening being wide enough to see through.

Observed in TOWN level (OLGEO.LAB): sector 7 entered through sector 605's wall 0 with a very narrow frustum (screen-x ratio -0.865 to -0.809). Wall 1, which also adjoins to sector 7 and covers a much wider opening (-0.809 to +0.099), is skipped because sector 7 is already marked as drawn. Walls 0, 1, 5 inside sector 7 are culled by the narrow frustum.

Also observed in house2 level: same pattern with sectors 7 and 4.

## Root Cause

The portal recursion loop processes adjoin entries one at a time. When multiple walls in a parent sector adjoin to the same child sector, only the **first** portal's frustum is used. The `prev_draw_frame` check prevents re-entry:

```c
// Double-draw prevention.
if (sector->prev_draw_frame == rs->draw_frame) {
    // [SKIP] sec N — already drawn this frame
    return;
}
```

The second (wider) portal is skipped, and the child sector never sees geometry that falls outside the first portal's narrow frustum cone.

## Fix

Merged all adjoin entries targeting the same sector before recursing. The portal recursion loop now:

1. Groups entries by `next_sector` using a `bool adjoin_done[]` array.
2. For each unique target sector, collects all portal vertex pairs and near-clips them.
3. Finds the two outermost vertices by screen-space angle (x/z ratio) to build the widest frustum.
4. Merges the edge pair x-range (min x0, max x1) for render window / depth buffer.
5. Recurses once with the merged frustum.

The `prev_draw_frame` check remains for cycle prevention (A → B → A).

## Verification

- Build succeeds with no new warnings.
- All unit tests pass.
- house2 level: both traces (yaw=-40.8 and yaw=-48.8) now produce consistent sector visibility.
