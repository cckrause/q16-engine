# ISSUE-007 — S-Buffer Wall Flickering / Incorrect Culling in Certain Configurations

**Status**: Open
**Date**: 2026-02-27
**Severity**: High
**File(s)**: `src/game/render/sbuffer.c`

## Symptom

With S-Buffer occlusion enabled, certain wall configurations cause visible flickering or incorrect culling. Walls that should be visible are intermittently hidden, or walls that should be occluded bleed through. The effect is view-angle dependent — small camera movements can cause a wall to pop in and out of visibility between frames.

This is distinct from ISSUE-006 (which was caused by using linear Z interpolation instead of the separating-plane test). The separating-plane depth comparison itself appears to produce unstable results for specific geometric arrangements.

## Root Cause

Under investigation. Suspected areas:

1. **Near-coplanar walls and zero-epsilon comparisons.** The separating-plane test in `sbuffer_segment_in_front` classifies endpoints using strict `> 0` / `< 0` sign checks with no epsilon tolerance. When two walls are nearly coplanar (or the camera is nearly on the plane of one wall), floating-point rounding can flip the sign of `side_a0 * side_cam` between frames, causing the depth ordering to oscillate.

2. **Asymmetric forward/reverse test.** The function tests new-against-cur first, then cur-against-new only in certain branches. When both tests are borderline (near-zero signed distances), the asymmetry can produce inconsistent results depending on insertion order — wall A vs B may get a different answer than B vs A.

3. **INTERSECT fallback depth comparison.** When `sbuffer_segment_in_front` returns `SBUF_INTERSECT`, the crossing-point handler falls back to `sbuffer_lerp_depth` (linear Z interpolation in projected space) to decide which half is in front. This is the same approximation that ISSUE-006 replaced for the non-intersecting case, and it can produce incorrect splits for walls at steep angles.

## Fix

(Pending investigation.)

## Verification

(Pending fix.)
