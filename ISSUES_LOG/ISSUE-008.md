# ISSUE-008 — Viewer Crash (OOM Kill) When Portal Budget or Portal Frustum Toggle Disabled

**Status**: Open
**Date**: 2026-02-27
**Severity**: Medium
**File(s)**: `src/game/render/render_sector.c`, `src/game/render/render_limits.h`

## Symptom

In `q16_view`, pressing key **5** (`budg=off`) combined with key **7** (`pfrust=off`) — or key **6** (`fclip=off`) while budget is already off — causes the process to be killed by macOS jetsam (SIGKILL / OOM). The terminal shows:

```
[1]    74552 killed     ./build/q16_view mock/ol/OLGEO.LAB TOWN
```

Reproduced on the TOWN level (610 sectors, 6171 walls). The crash occurs within seconds of toggling, not after a long hang.

## Root Cause

The `CULL_PORTAL_BUDGET` toggle gates the **only** bound on total portal traversals per frame:

```c
if ((rs->cull_mask & CULL_PORTAL_BUDGET) && rs->portals_traversed >= rs->max_portals) {
    rs->cull_count_budget++;
    return;
}
```

When this check is bypassed, recursion depth is still capped at 40 (`max_adjoin_depth`), but the **branching factor** is unbounded. DFS wall marking (`CULL_DFS_MARKING`) only prevents cycling through the same wall in the current recursion path — it does not prevent revisiting a sector via different walls. For a well-connected 610-sector portal graph, the number of acyclic paths of length ≤ 40 is combinatorially explosive (potentially millions of recursive calls).

When `CULL_PORTAL_FRUSTUM` is also off, child sectors receive the parent's wide frustum instead of a narrowed portal frustum, so even more walls pass and generate additional sub-portals, amplifying the explosion.

Each call to `render_draw_sector` allocates ~113 KB on the stack (`WallSegment segments[768]` + `AdjoinList`). While the max simultaneous stack depth is bounded at 40 (~4.5 MB), the total CPU work per frame becomes enormous — the process either exhausts memory indirectly or runs long enough to trigger jetsam.

## Fix

Add a non-toggleable hard safety cap on `portals_traversed` in `render_limits.h`:

```c
#define HARD_PORTAL_LIMIT 16384
```

Enforce it unconditionally at the top of `render_draw_sector`, before the soft budget check:

```c
if (rs->portals_traversed >= HARD_PORTAL_LIMIT)
    return;
```

The soft budget (4096) continues to work normally when `CULL_PORTAL_BUDGET` is on. When toggled off, traversal can run 4× further (16384) — enough to observe the visual effect of disabling the budget — but cannot explode into millions of calls.

## Verification

(Pending fix.)
