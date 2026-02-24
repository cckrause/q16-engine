/*
 * Glide 2.x API - Minimal header for Retro Engine
 * Compatible with Glide 2.4x / 2.60 (Voodoo 1 / Voodoo 2)
 */
#ifndef __GLIDE_H__
#define __GLIDE_H__

#include "3dfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Type definitions
 * ================================================================ */
typedef FxI32 GrScreenResolution_t;
typedef FxI32 GrScreenRefresh_t;
typedef FxI32 GrColorFormat_t;
typedef FxI32 GrOriginLocation_t;
typedef FxI32 GrCombineFunction_t;
typedef FxI32 GrCombineFactor_t;
typedef FxI32 GrCombineLocal_t;
typedef FxI32 GrCombineOther_t;
typedef FxI32 GrDepthBufferMode_t;
typedef FxI32 GrCullMode_t;
typedef FxI32 GrBuffer_t;

/* ================================================================
 *  Screen resolution
 * ================================================================ */
#define GR_RESOLUTION_320x200 0x0
#define GR_RESOLUTION_320x240 0x1
#define GR_RESOLUTION_400x256 0x2
#define GR_RESOLUTION_512x384 0x3
#define GR_RESOLUTION_640x200 0x4
#define GR_RESOLUTION_640x350 0x5
#define GR_RESOLUTION_640x400 0x6
#define GR_RESOLUTION_640x480 0x7
#define GR_RESOLUTION_800x600 0x8
#define GR_RESOLUTION_960x720 0x9
#define GR_RESOLUTION_856x480 0xa
#define GR_RESOLUTION_512x256 0xb

/* ================================================================
 *  Screen refresh rate
 * ================================================================ */
#define GR_REFRESH_60Hz 0x0
#define GR_REFRESH_70Hz 0x1
#define GR_REFRESH_72Hz 0x2
#define GR_REFRESH_75Hz 0x3
#define GR_REFRESH_80Hz 0x4
#define GR_REFRESH_90Hz 0x5
#define GR_REFRESH_100Hz 0x6
#define GR_REFRESH_85Hz 0x7
#define GR_REFRESH_120Hz 0x8

/* ================================================================
 *  Color format
 * ================================================================ */
#define GR_COLORFORMAT_ABGR 0x0
#define GR_COLORFORMAT_ARGB 0x1

/* ================================================================
 *  Origin location
 * ================================================================ */
#define GR_ORIGIN_UPPER_LEFT 0x0
#define GR_ORIGIN_LOWER_LEFT 0x1

/* ================================================================
 *  Color combine function
 * ================================================================ */
#define GR_COMBINE_FUNCTION_ZERO 0x0
#define GR_COMBINE_FUNCTION_NONE GR_COMBINE_FUNCTION_ZERO
#define GR_COMBINE_FUNCTION_LOCAL 0x1
#define GR_COMBINE_FUNCTION_LOCAL_ALPHA 0x2
#define GR_COMBINE_FUNCTION_SCALE_OTHER 0x3

/* ================================================================
 *  Color combine factor
 * ================================================================ */
#define GR_COMBINE_FACTOR_ZERO 0x0
#define GR_COMBINE_FACTOR_NONE GR_COMBINE_FACTOR_ZERO
#define GR_COMBINE_FACTOR_LOCAL 0x1
#define GR_COMBINE_FACTOR_ONE 0x8

/* ================================================================
 *  Color combine local / other source
 * ================================================================ */
#define GR_COMBINE_LOCAL_ITERATED 0x0
#define GR_COMBINE_LOCAL_CONSTANT 0x1
#define GR_COMBINE_LOCAL_NONE GR_COMBINE_LOCAL_CONSTANT

#define GR_COMBINE_OTHER_ITERATED 0x0
#define GR_COMBINE_OTHER_TEXTURE 0x1
#define GR_COMBINE_OTHER_CONSTANT 0x2
#define GR_COMBINE_OTHER_NONE GR_COMBINE_OTHER_CONSTANT

/* ================================================================
 *  Depth buffer mode
 * ================================================================ */
#define GR_DEPTHBUFFER_DISABLE 0x0
#define GR_DEPTHBUFFER_ZBUFFER 0x1
#define GR_DEPTHBUFFER_WBUFFER 0x2
#define GR_DEPTHBUFFER_ZBUFFER_COMPARE_TO_BIAS 0x3
#define GR_DEPTHBUFFER_WBUFFER_COMPARE_TO_BIAS 0x4

/* ================================================================
 *  Cull mode
 * ================================================================ */
#define GR_CULL_DISABLE 0x0
#define GR_CULL_NEGATIVE 0x1
#define GR_CULL_POSITIVE 0x2

/* ================================================================
 *  Buffer identifiers
 * ================================================================ */
#define GR_BUFFER_FRONTBUFFER 0x0
#define GR_BUFFER_BACKBUFFER 0x1
#define GR_BUFFER_AUXBUFFER 0x2
#define GR_BUFFER_DEPTHBUFFER 0x3

/* ================================================================
 *  Vertex structure (Glide 2.x - fixed layout)
 *  Color components r, g, b, a are floats in range 0..255
 * ================================================================ */
#define GLIDE_NUM_TMU 1

typedef struct {
  float sow; /* s/w texture coordinate */
  float tow; /* t/w texture coordinate */
  float oow; /* 1/w for LOD/mipmap     */
} GrTmuVertex;

typedef struct {
  float x, y;       /* screen-space position                */
  float ooz;        /* 65535/z for depth buffering           */
  float oow;        /* 1/w for perspective correction        */
  float r, g, b, a; /* vertex color, floats in [0..255]     */
  float z;          /* z coordinate (if not using ooz)      */
  GrTmuVertex tmuvtx[GLIDE_NUM_TMU];
} GrVertex;

/* ================================================================
 *  Smoothing mode (anti-aliasing filter)
 * ================================================================ */
#define GR_SMOOTHING_DISABLE 0x0
#define GR_SMOOTHING_ENABLE 0x1

/* ================================================================
 *  Depth value range
 * ================================================================ */
#define GR_WDEPTHVALUE_FARTHEST 0xFFFF
#define GR_ZDEPTHVALUE_FARTHEST 0xFFFF

/* ================================================================
 *  Function declarations (Glide 2.x)
 * ================================================================ */

/* Initialization & shutdown */
FX_ENTRY void FX_CALL grGlideInit(void);

FX_ENTRY void FX_CALL grGlideShutdown(void);

FX_ENTRY void FX_CALL grSstSelect(FxU32 which_sst);

/* Window / context management (returns FxBool in Glide 2.x) */
FX_ENTRY FxBool FX_CALL grSstWinOpen(FxU32 hWnd,
                                     GrScreenResolution_t screen_resolution,
                                     GrScreenRefresh_t refresh_rate,
                                     GrColorFormat_t color_format,
                                     GrOriginLocation_t origin_location,
                                     int nColBuffers, int nAuxBuffers);

/* Color combine pipeline */
FX_ENTRY void FX_CALL grColorCombine(GrCombineFunction_t function,
                                     GrCombineFactor_t factor,
                                     GrCombineLocal_t local,
                                     GrCombineOther_t other, FxBool invert);

/* Depth buffer */
FX_ENTRY void FX_CALL grDepthBufferMode(GrDepthBufferMode_t mode);

/* Culling */
FX_ENTRY void FX_CALL grCullMode(GrCullMode_t mode);

/* Buffer operations */
FX_ENTRY void FX_CALL grBufferClear(GrColor_t color, GrAlpha_t alpha,
                                    FxU16 depth);

FX_ENTRY void FX_CALL grBufferSwap(FxU32 swap_interval);

/* Triangle rendering (takes GrVertex pointers) */
FX_ENTRY void FX_CALL grDrawTriangle(const GrVertex *a, const GrVertex *b,
                                     const GrVertex *c);

/* Render buffer selection */
FX_ENTRY void FX_CALL grRenderBuffer(GrBuffer_t buffer);

#ifdef __cplusplus
}
#endif

#endif /* __GLIDE_H__ */
