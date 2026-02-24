/*
 * 3dfx base types for Glide 2.x
 * Minimal header for cross-compilation with MinGW
 */
#ifndef __3DFX_H__
#define __3DFX_H__

/* ---- Basic data types ---- */
typedef unsigned char FxU8;
typedef signed char FxI8;
typedef unsigned short FxU16;
typedef signed short FxI16;
typedef unsigned int FxU32;
typedef signed int FxI32;
typedef int FxBool;
typedef float FxFloat;

#define FXTRUE 1
#define FXFALSE 0

/* ---- DLL linkage macros ---- */
#if defined(__MINGW32__) || defined(_WIN32)
#define FX_ENTRY __declspec(dllimport)
#define FX_CALL __stdcall
#else
#define FX_ENTRY extern
#define FX_CALL
#endif

/* ---- Color types ---- */
typedef FxU32 GrColor_t;
typedef FxU8 GrAlpha_t;

#endif /* __3DFX_H__ */
