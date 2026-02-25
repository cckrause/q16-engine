# ISSUE-003 — Portal Frustum Built From Degenerate Data When Near-Clip Fails

**Status**: Fixed
**Date**: 2026-02-25
**Severity**: Medium
**File(s)**: `src/game/render/render_sector.c`

## Symptom

Potential rendering corruption or over-rendering when a portal wall's original (unclipped) vertices both lie behind the camera's near plane.

## Root Cause

During portal recursion, the code re-transforms the original wall vertices (`w0`, `w1`) to camera space and clips them against the near plane. The return value of `frustum_clip_near` was ignored. If both vertices are behind the near plane, `frustum_clip_near` returns `false` and the vertex values are unchanged (both behind camera). The subsequent `frustum_build_portal` then constructs a frustum from degenerate positions.

## Fix

Moved the near-clip check before the save/enter adjoin block. When `frustum_clip_near` returns `false`, the loop `continue`s — skipping portal recursion entirely without touching the adjoin state stack:

```c
if (!frustum_clip_near(&pw0x, &pw0z, &pw1x, &pw1z, NEAR_PLANE_EPSILON, NULL))
    continue;
```

This is cleaner than the `goto skip_portal` alternative because no state needs unwinding — the save/enter has not happened yet.
