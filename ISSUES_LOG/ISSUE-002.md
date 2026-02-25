# ISSUE-002 — Transparent Mid-Texture Y-Bounds Inverted (Y-Down)

**Status**: Fixed
**Date**: 2026-02-25
**Severity**: High
**File(s)**: `src/game/render/render_sector.c` (lines 180–181)

## Symptom

Transparent mid-textures (grates, bars) on adjoin walls span the wider sector bounds instead of the tighter portal opening. The texture is too tall vertically.

## Root Cause

`emit_wall_entries` computes the portal opening bounds for the transparent mid-texture using the **Y-Up convention** (`max` of floors, `min` of ceilings), but the engine now operates in Y-Down after the OL coordinate conversion.

```c
// Before (WRONG for Y-Down):
float open_bot = floor_h > next_floor ? floor_h : next_floor;  // max
float open_top = ceil_h < next_ceil ? ceil_h : next_ceil;      // min
```

`adjoin_compute_edge_pair` in `adjoin.c` was correctly updated to Y-Down (`max` for ceiling, `min` for floor), but this spot in `emit_wall_entries` was missed.

## Fix

Swapped comparisons to match Y-Down convention (consistent with `adjoin_compute_edge_pair`):

```c
float open_bot = floor_h < next_floor ? floor_h : next_floor;  // min = tighter floor (Y-Down)
float open_top = ceil_h > next_ceil ? ceil_h : next_ceil;      // max = tighter ceiling (Y-Down)
```

## Verification

Traced all 20 sectors of `house2.lvt` against the reference renderer output (`rendered_geometry.json`). All adjoin classification (TOP/BOT/MID), double-adjoin logic, and height assignments matched. Only this transparent opening calculation was incorrect.
