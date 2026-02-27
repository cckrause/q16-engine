# ISSUE-006 — S-Buffer Depth Comparison Incorrect for Angled Walls

**Status**: Fixed
**Date**: 2026-02-27
**Severity**: High
**File(s)**: `src/game/render/sbuffer.c`, `src/game/render/sbuffer.h`

## Symptom

With S-Buffer occlusion enabled (toggle `3:sbuf`), walls at oblique angles to each other are incorrectly culled. A near wall angled away from the camera is sometimes classified as "behind" a far wall that faces the camera head-on, causing the near wall to be discarded and revealing geometry behind it.

The issue is most visible in complex portal chains where walls from different sectors overlap in projected screen space at different angles.

## Root Cause

The original S-Buffer stored only `z0, z1` (cam-space depth at projected endpoints) and used linear interpolation to compare depths at overlap midpoints:

```c
float depth_new = z0 + t * (z1 - z0);
float depth_cur = cur->z0 + t * (cur->z1 - cur->z0);
if (depth_new < depth_cur) { /* new is closer */ }
```

This is geometrically incorrect: Z does not interpolate linearly in `atan2` projection space. For two walls at different angles, the linear approximation can invert the depth relationship at certain projected positions, producing wrong occlusion decisions.

## Fix

Replaced the Z-interpolation depth comparison with a separating-plane test. Each S-Buffer segment now stores the full cam-space wall geometry (vertices + normal):

```c
typedef struct SBufferSeg {
  float start, end;
  float vx0, vz0, vx1, vz1;   // cam-space vertices
  float normal_x, normal_z;    // wall normal
  float normal_d;               // dot(normal, vertex)
  // ...
} SBufferSeg;
```

The depth test classifies both endpoints of one wall against the other wall's plane, then compares against the camera side (origin):

```c
side_a0 = dot(new.v0, cur.normal) - cur.normal_d;
side_a1 = dot(new.v1, cur.normal) - cur.normal_d;
side_cam = -cur.normal_d;
// Both on camera side → NEW_FRONT
// Both on far side → check reverse → CUR_FRONT
// Split → INTERSECT (walls cross)
```

This is geometrically exact (no approximation) and costs only 2 dot-products per comparison.

## Verification

- Build succeeds with no warnings.
- All unit tests pass (including new crossing-wall test).
- Stable opaque counts in TOWN level with S-Buffer enabled (no flickering).
