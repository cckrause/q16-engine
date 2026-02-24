/*
 * glide_loader.h - Runtime dynamic loader for Glide 2.x
 * Loads glide2x.dll via LoadLibrary/GetProcAddress to avoid
 * import decoration mismatches between compilers/DLL versions.
 */
#ifndef GLIDE_LOADER_H
#define GLIDE_LOADER_H

#include "glide2/glide.h"
#include <windows.h>

/* ---- Function pointer typedefs (cdecl - no decoration) ---- */
typedef void(__stdcall *pfn_grGlideInit)(void);
typedef void(__stdcall *pfn_grGlideShutdown)(void);
typedef void(__stdcall *pfn_grSstSelect)(FxU32);
typedef FxBool(__stdcall *pfn_grSstWinOpen)(FxU32, GrScreenResolution_t,
                                            GrScreenRefresh_t, GrColorFormat_t,
                                            GrOriginLocation_t, int, int);
typedef void(__stdcall *pfn_grColorCombine)(GrCombineFunction_t, GrCombineFactor_t,
                                            GrCombineLocal_t, GrCombineOther_t, FxBool);
typedef void(__stdcall *pfn_grDepthBufferMode)(GrDepthBufferMode_t);
typedef void(__stdcall *pfn_grCullMode)(GrCullMode_t);
typedef void(__stdcall *pfn_grBufferClear)(GrColor_t, GrAlpha_t, FxU16);
typedef void(__stdcall *pfn_grBufferSwap)(FxU32);
typedef void(__stdcall *pfn_grDrawTriangle)(const GrVertex *, const GrVertex *,
                                            const GrVertex *);

/* ---- Global function pointers ---- */
static pfn_grGlideInit gl_grGlideInit;
static pfn_grGlideShutdown gl_grGlideShutdown;
static pfn_grSstSelect gl_grSstSelect;
static pfn_grSstWinOpen gl_grSstWinOpen;
static pfn_grColorCombine gl_grColorCombine;
static pfn_grDepthBufferMode gl_grDepthBufferMode;
static pfn_grCullMode gl_grCullMode;
static pfn_grBufferClear gl_grBufferClear;
static pfn_grBufferSwap gl_grBufferSwap;
static pfn_grDrawTriangle gl_grDrawTriangle;

static HMODULE g_glideDll;

/* Try to get a function by plain name, then with _name@N decoration */
static FARPROC glide_getproc(HMODULE dll, const char *name, const char *decorated) {
  FARPROC p = GetProcAddress(dll, name);
  if (!p && decorated)
    p = GetProcAddress(dll, decorated);
  return p;
}

/* Load all Glide functions. Returns 0 on success, -1 on failure. */
static int glide_load(void) {
  g_glideDll = LoadLibrary("glide2x.dll");
  if (!g_glideDll) {
    MessageBox(NULL,
               "Could not load glide2x.dll!\n"
               "Install Voodoo drivers or nGlide wrapper.",
               "q16 Engine", MB_OK | MB_ICONERROR);
    return -1;
  }

#define LOAD(fn, dec)                                              \
  gl_##fn = (pfn_##fn)glide_getproc(g_glideDll, #fn, dec);         \
  if (!gl_##fn) {                                                  \
    MessageBox(NULL, "Missing Glide export: " #fn, "q16 Engine", \
               MB_OK | MB_ICONERROR);                              \
    FreeLibrary(g_glideDll);                                       \
    return -1;                                                     \
  }

  LOAD(grGlideInit, "_grGlideInit@0")
  LOAD(grGlideShutdown, "_grGlideShutdown@0")
  LOAD(grSstSelect, "_grSstSelect@4")
  LOAD(grSstWinOpen, "_grSstWinOpen@28")
  LOAD(grColorCombine, "_grColorCombine@20")
  LOAD(grDepthBufferMode, "_grDepthBufferMode@4")
  LOAD(grCullMode, "_grCullMode@4")
  LOAD(grBufferClear, "_grBufferClear@12")
  LOAD(grBufferSwap, "_grBufferSwap@4")
  LOAD(grDrawTriangle, "_grDrawTriangle@12")

#undef LOAD
  return 0;
}

static void glide_unload(void) {
  if (g_glideDll) {
    FreeLibrary(g_glideDll);
    g_glideDll = NULL;
  }
}

#endif /* GLIDE_LOADER_H */
