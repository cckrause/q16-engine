// ===========================================================================
// Debug Log — Structured logger with levels, modules, and indentation
// ===========================================================================

#include "debug/debug_log.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

static FILE *s_file;
static LogLevel s_min_level = DLOG_TRACE;

static const char *s_level_tags[] = {"ERROR", "WARN ", "INFO ", "TRACE"};

#ifdef _WIN32
static LARGE_INTEGER s_freq;
static LARGE_INTEGER s_start;
#else
static struct timeval s_start;
#endif

static double elapsed_ms(void) {
#ifdef _WIN32
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return (double)(now.QuadPart - s_start.QuadPart) * 1000.0 / (double)s_freq.QuadPart;
#else
  struct timeval now;
  gettimeofday(&now, NULL);
  return (double)(now.tv_sec - s_start.tv_sec) * 1000.0 +
         (double)(now.tv_usec - s_start.tv_usec) / 1000.0;
#endif
}

// --- Lifecycle -------------------------------------------------------------

bool debug_log_init(const char *filename) {
  if (filename) {
    s_file = fopen(filename, "w");
    if (!s_file)
      return false;
  } else {
    s_file = stderr;
  }

#ifdef _WIN32
  QueryPerformanceFrequency(&s_freq);
  QueryPerformanceCounter(&s_start);
#else
  gettimeofday(&s_start, NULL);
#endif

  fprintf(s_file, "=== q16 Engine Debug Log ===\n");
  fflush(s_file);
  return true;
}

void debug_log_shutdown(void) {
  if (!s_file)
    return;
  double ms = elapsed_ms();
  fprintf(s_file, "[%10.3f] INFO  log    | === Log End (%.1fs) ===\n", ms, ms / 1000.0);
  fflush(s_file);
  if (s_file != stderr)
    fclose(s_file);
  s_file = NULL;
}

void debug_log_set_level(LogLevel min_level) {
  s_min_level = min_level;
}

// --- Writing ---------------------------------------------------------------

void debug_log_write(LogLevel level, const char *module, int32_t indent, const char *fmt,
                     ...) {
  if (!s_file || level > s_min_level)
    return;
  fprintf(s_file, "[%10.3f] %s %-6s | %*s", elapsed_ms(), s_level_tags[level], module,
          indent * 2, "");
  va_list args;
  va_start(args, fmt);
  vfprintf(s_file, fmt, args);
  va_end(args);
  fputc('\n', s_file);
  fflush(s_file);
}

bool debug_log_is_active(void) {
  return s_file != NULL;
}
