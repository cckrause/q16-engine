/*
 * Glide 2.x rotating-wall demo.
 * Renders a single quad with per-face diffuse lighting to verify
 * Glide init, projection, and basic 3D math.
 */

#include "demo.h"

#include <stdbool.h>
#include <string.h>

#include "glide2/glide.h"
#include "glide_loader.h"
#include "math3d.h"

#define SCREEN_W  640
#define SCREEN_H  480
#define FOV       320.0f
#define CAM_DIST  3.5f
#define ROT_SPEED 0.02f
#define PI2       6.2831853f

static bool s_running = true;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_CLOSE:
  case WM_DESTROY:
    s_running = false;
    PostQuitMessage(0);
    return 0;
  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) {
      s_running = false;
      PostQuitMessage(0);
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

int demo_run(HINSTANCE hInstance) {
  Vec3 light_dir = vec3_normalize(vec3(1.0f, 1.0f, -1.0f));
  const float base_r = 220.0f;
  const float base_g = 180.0f;
  const float base_b = 120.0f;

  if (glide_load() != 0)
    return 1;

  WNDCLASS wc;
  memset(&wc, 0, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = "GlideQ16Engine";
  RegisterClass(&wc);

  HWND hwnd = CreateWindow("GlideQ16Engine", "q16 Engine", WS_POPUP | WS_VISIBLE, 0,
                           0, SCREEN_W, SCREEN_H, NULL, NULL, hInstance, NULL);
  ShowCursor(FALSE);

  gl_grGlideInit();
  gl_grSstSelect(0);

  FxBool ok = gl_grSstWinOpen((FxU32)hwnd, GR_RESOLUTION_640x480, GR_REFRESH_60Hz,
                              GR_COLORFORMAT_ABGR, GR_ORIGIN_UPPER_LEFT, 2, 1);

  if (!ok) {
    MessageBox(NULL,
               "Failed to initialise Glide!\n"
               "Make sure a Voodoo card (or nGlide wrapper) is present.",
               "q16 Engine", MB_OK | MB_ICONERROR);
    DestroyWindow(hwnd);
    glide_unload();
    return 1;
  }

  gl_grColorCombine(GR_COMBINE_FUNCTION_LOCAL, GR_COMBINE_FACTOR_NONE,
                    GR_COMBINE_LOCAL_ITERATED, GR_COMBINE_OTHER_NONE, FXFALSE);
  gl_grDepthBufferMode(GR_DEPTHBUFFER_DISABLE);
  gl_grCullMode(GR_CULL_DISABLE);

  float angle = 0.0f;
  MSG msg;

  while (s_running) {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        s_running = false;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!s_running)
      break;

    angle += ROT_SPEED;
    if (angle > PI2)
      angle -= PI2;

    Vec3 world[4] = {
        vec3(-1.0f, -1.0f, 0.0f),
        vec3(1.0f, -1.0f, 0.0f),
        vec3(1.0f, 1.0f, 0.0f),
        vec3(-1.0f, 1.0f, 0.0f),
    };

    Vec3 rot[4];
    for (int i = 0; i < 4; i++) {
      rot[i] = vec3_rotate_y(world[i], angle);
      rot[i].z += CAM_DIST;
    }

    Vec3 edge1 = vec3_sub(rot[1], rot[0]);
    Vec3 edge2 = vec3_sub(rot[3], rot[0]);
    Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));
    float diffuse = vec3_dot(normal, light_dir);

    float intensity;
    if (diffuse >= 0.0f) {
      intensity = 0.15f + diffuse * 0.85f;
    } else {
      intensity = 0.15f + (-diffuse) * 0.25f;
    }
    if (intensity > 1.0f)
      intensity = 1.0f;

    float cr = intensity * base_r;
    float cg = intensity * base_g;
    float cb = intensity * base_b;

    GrVertex v[4];
    for (int i = 0; i < 4; i++) {
      memset(&v[i], 0, sizeof(GrVertex));
      float inv_z = 1.0f / rot[i].z;
      v[i].x = rot[i].x * FOV * inv_z + (SCREEN_W * 0.5f);
      v[i].y = -rot[i].y * FOV * inv_z + (SCREEN_H * 0.5f);
      v[i].ooz = 65535.0f * inv_z;
      v[i].oow = inv_z;
      v[i].r = cr;
      v[i].g = cg;
      v[i].b = cb;
      v[i].a = 255.0f;
    }

    gl_grBufferClear(0x00102030, 0, GR_ZDEPTHVALUE_FARTHEST);
    gl_grDrawTriangle(&v[0], &v[1], &v[2]);
    gl_grDrawTriangle(&v[0], &v[2], &v[3]);
    gl_grBufferSwap(1);
  }

  gl_grGlideShutdown();
  ShowCursor(TRUE);
  DestroyWindow(hwnd);
  glide_unload();

  return 0;
}
