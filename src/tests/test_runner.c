/*
 * Unit-test runner for the q16 Engine.
 *
 * Calls every test suite and prints a summary.
 * Build: cmake --build build && ./build/q16_tests
 */

#include "test_harness.h"

#include <stdio.h>

// Global counters shared by all suites via test_harness.h.
int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;
int g_suite_run = 0;
int g_suite_failed = 0;

// Forward declarations — each suite lives in its own test_*.c file.

// Math primitives
void test_fixed16(void);
void test_core_math(void);
void test_trig(void);

// Memory subsystem
void test_memory_region(void);
void test_game_memory(void);

// Archive & filesystem
void test_archive(void);

// I/O & parsing
void test_text_parser(void);

// World / level
void test_level_parser(void);

// Render pipeline
void test_camera(void);
void test_frustum(void);
void test_depth_window(void);
void test_sbuffer(void);
void test_lighting(void);
void test_wall_process(void);
void test_adjoin(void);
void test_display_list(void);
void test_object_sort(void);
void test_render_sector(void);
void test_flat(void);

int main(void) {
  printf("q16 engine — unit tests\n");

  // Section 1 — Math Primitives
  test_fixed16();
  test_core_math();
  test_trig();

  // Section 2 — Memory Subsystem
  test_memory_region();
  test_game_memory();

  // Section 3 — Archive & Filesystem
  test_archive();

  // Section 4 — I/O & Parsing
  test_text_parser();

  // Section 5 — World / Level
  test_level_parser();

  // Section 6 — Render Pipeline
  test_camera();
  test_frustum();
  test_depth_window();
  test_sbuffer();
  test_lighting();
  test_wall_process();
  test_adjoin();
  test_display_list();
  test_object_sort();
  test_render_sector();
  test_flat();

  TEST_PRINT_SUMMARY();
  return g_tests_failed == 0 ? 0 : 1;
}
