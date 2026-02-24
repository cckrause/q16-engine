/*
 * q16 Engine - Entry point
 *
 * Build: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw32.cmake
 *        cmake --build build
 * Run:   Copy retro_engine.exe into a Win95/98 emulator with Voodoo card
 *        (86Box/PCem + 3dfx Voodoo driver, or nGlide wrapper).
 */

#include <windows.h>

#include "demo.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
                   int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nCmdShow;

  return demo_run(hInstance);
}
