// ===========================================================================
// Test Harness — Minimal Unit-test Framework
// ===========================================================================
#ifndef Q16_TEST_HARNESS_H
#define Q16_TEST_HARNESS_H

#include <stdio.h>

// Usage in a test file:
//   #include "test_harness.h"
//   void test_my_module(void) {
//     TEST_SUITE_BEGIN("my_module");
//     TEST_CHECK("description", some_condition);
//     TEST_SUITE_END();
//   }
//
// The runner (test_runner.c) defines the counter variables and calls
// each test_xxx() function, then prints the summary.

// Global counters — defined in test_runner.c, shared across all suites.
extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;

// Per-suite counters for local summary.
extern int g_suite_run;
extern int g_suite_failed;

// Check a single test condition. Prints FAIL with file:line on failure.
#define TEST_CHECK(name, cond)                                     \
  do {                                                             \
    g_tests_run++;                                                 \
    g_suite_run++;                                                 \
    if (cond) {                                                    \
      g_tests_passed++;                                            \
    } else {                                                       \
      g_tests_failed++;                                            \
      g_suite_failed++;                                            \
      printf("  FAIL: %s  (%s:%d)\n", (name), __FILE__, __LINE__); \
    }                                                              \
  } while (0)

// Print a section header and reset per-suite counters.
#define TEST_SUITE_BEGIN(name)        \
  do {                                \
    g_suite_run = 0;                  \
    g_suite_failed = 0;               \
    printf("\n--- %s ---\n", (name)); \
  } while (0)

// Print per-suite summary after all checks in a suite.
#define TEST_SUITE_END()                                                                \
  do {                                                                                  \
    if (g_suite_failed == 0) {                                                          \
      printf("  %d/%d passed\n", g_suite_run, g_suite_run);                             \
    } else {                                                                            \
      printf("  %d/%d passed (%d FAILED)\n", g_suite_run - g_suite_failed, g_suite_run, \
             g_suite_failed);                                                           \
    }                                                                                   \
  } while (0)

// Print overall summary. Returns 0 on success, 1 on failure (for main()).
#define TEST_PRINT_SUMMARY()                                                 \
  do {                                                                       \
    printf("\n=== %d / %d tests passed ===\n", g_tests_passed, g_tests_run); \
    if (g_tests_failed > 0) {                                                \
      printf("=== %d FAILED ===\n", g_tests_failed);                         \
    }                                                                        \
  } while (0)

#endif /* Q16_TEST_HARNESS_H */
