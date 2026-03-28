# ISSUE-009 — Stack Overflow Crash on Win98 During Deep Portal Traversal

**Status**: Fixed
**Date**: 2026-03-01
**Severity**: Critical
**File(s)**: `src/game/render/render_sector.c`, `src/game/render/render_sector.h`

## Symptom

On Win98 with Glide, the engine crashes silently during `render_draw_frame` after several thousand frames. The debug log ends mid-frame — `RENDER sec=115 pos=(...)` is written but `RENDER_DONE` never appears. The crash occurs when the camera moves into a position where the portal graph becomes deep enough (~10+ levels). Timing and op counts in preceding frames appear normal.

## Root Cause

`render_draw_sector` is recursive (portal traversal) and allocates large arrays on the stack at every recursion level:

```c
WallSegment segments[MAX_WALL_SEG];  // 768 × 68 bytes ≈ 52 KB
AdjoinList adjoin_list;              // entries[768] × 56 bytes ≈ 43 KB
```

Total per recursion level: **~100 KB**. With `MAX_ADJOIN_DEPTH = 40`, worst case is **40 × 100 KB = 4 MB** of stack usage. Win98's default stack size is **1 MB**, so the stack overflows at approximately depth 10.

The crash is position-dependent because different camera positions produce different portal graph depths. Frames with shallow traversal (depth < 10) succeed; the first frame that requires deeper traversal crashes without any error message (stack overflow on Win98 is a silent fault).

## Fix

Moved the two large arrays from stack allocation to a heap-allocated per-depth workspace in `RenderState`. Allocated once during `render_state_init`, freed in `render_state_destroy`:

```c
// render_sector.h — added to RenderState:
WallSegment *seg_pool;     // heap: (max_adjoin_depth + 1) * MAX_WALL_SEG
AdjoinList *adjoin_pool;   // heap: (max_adjoin_depth + 1) AdjoinLists

// render_sector.c — render_draw_sector now indexes by depth:
WallSegment *segments = rs->seg_pool + (size_t)rs->adjoin_depth * MAX_WALL_SEG;
AdjoinList *adjoin_list = &rs->adjoin_pool[rs->adjoin_depth];
```

Remaining per-level stack usage is ~4 KB (local variables, `adjoin_done[768]`, loop-scoped arrays). With depth 40 that is ~160 KB — well within the 1 MB limit.

## Verification

- All 632 unit tests pass.
- Mac native build and Win32 cross-compile both clean (no new warnings).
- Heap allocation: ~3.7 MB total (41 depth levels × ~95 KB), acceptable for Win98's 32–256 MB RAM.
