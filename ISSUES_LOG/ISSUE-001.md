# ISSUE-001 — Stale Pointer After Segment Sort (Portal Frustum Corruption)

**Status**: Fixed
**Date**: 2025-02-21
**Severity**: High
**File(s)**: `src/game/render/render_sector.c`

## Symptom

Portals visible from one direction but not the other. Direction-dependent sector visibility through multi-portal chains (e.g. 172 → 4 → 58).

## Root Cause

`render_draw_sector` stored **pointers** into a stack-local `WallSegment segments[]` array inside the `AdjoinList`. A subsequent `wall_merge_sort` physically moved structs between array slots, leaving the adjoin list's pointers valid but pointing to **wrong data**.

```c
// Wall processing loop — adjoin list captures pointers into segments[]
WallSegment segments[MAX_WALL_SEG];
int32_t seg_count = 0;

for (...) {
    WallSegment *seg = &segments[seg_count];  // pointer to slot N
    wall_process(..., seg);

    adjoin_list_add(&adjoin_list, seg, ...);  // stores &segments[N]
    seg_count++;
}

wall_merge_sort(segments, seg_count);  // <-- physically reorders structs

// Portal recursion — ae->seg now points to a DIFFERENT wall's data
for (int32_t i = 0; i < adjoin_list.count; i++) {
    AdjoinEntry *ae = &adjoin_list.entries[i];
    ae->seg->src_wall->w0  // WRONG wall — slot was overwritten by sort
}
```

## Why It Was Direction-Dependent

The bug only manifested when enough walls passed frustum culling to cause the sort to actually reorder slots. When fewer walls passed (different camera angle), the relative order was preserved and pointers happened to remain correct.

## Fix

Removed `wall_merge_sort` from `render_draw_sector`. The sort was vestigial — display list entries are emitted during the wall loop before any sort, and the GPU handles draw order via depth testing.
