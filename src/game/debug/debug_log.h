// ===========================================================================
// Debug Log — Structured logger with levels, modules, and indentation
// ===========================================================================
// On Win32: writes to a file for post-mortem hang diagnosis.
// On Mac/Linux: writes to stderr for live console output.
// Every entry is flushed immediately so the log survives hangs.
#ifndef Q16_DEBUG_LOG_H
#define Q16_DEBUG_LOG_H

#include <stdbool.h>
#include <stdint.h>

// --- Log levels ------------------------------------------------------------

typedef enum {
  DLOG_ERROR,
  DLOG_WARN,
  DLOG_INFO,
  DLOG_TRACE
} LogLevel;

// --- Lifecycle -------------------------------------------------------------

// Open the log. Pass a filename for file output, or NULL for stderr.
bool debug_log_init(const char *filename);

// Close the log file (no-op for stderr mode).
void debug_log_shutdown(void);

// Set minimum level. Entries above this threshold are dropped.
// Default: DLOG_TRACE (everything).
void debug_log_set_level(LogLevel min_level);

// --- Writing ---------------------------------------------------------------

// Write a structured log entry: [timestamp] LEVEL module | <indent>message
void debug_log_write(LogLevel level, const char *module, int32_t indent,
                     const char *fmt, ...);

// True when the log output is open.
bool debug_log_is_active(void);

// --- Shorthand macros (flat, indent 0) -------------------------------------

#define LOG_ERROR(mod, fmt, ...) \
  debug_log_write(DLOG_ERROR, mod, 0, fmt, ##__VA_ARGS__)
#define LOG_WARN(mod, fmt, ...) \
  debug_log_write(DLOG_WARN, mod, 0, fmt, ##__VA_ARGS__)
#define LOG_INFO(mod, fmt, ...) \
  debug_log_write(DLOG_INFO, mod, 0, fmt, ##__VA_ARGS__)
#define LOG_TRACE(mod, fmt, ...) \
  debug_log_write(DLOG_TRACE, mod, 0, fmt, ##__VA_ARGS__)

// --- Shorthand macros (with indent depth n) ---------------------------------

#define LOG_ERROR_INDENT(mod, n, fmt, ...) \
  debug_log_write(DLOG_ERROR, mod, n, fmt, ##__VA_ARGS__)
#define LOG_WARN_INDENT(mod, n, fmt, ...) \
  debug_log_write(DLOG_WARN, mod, n, fmt, ##__VA_ARGS__)
#define LOG_INFO_INDENT(mod, n, fmt, ...) \
  debug_log_write(DLOG_INFO, mod, n, fmt, ##__VA_ARGS__)
#define LOG_TRACE_INDENT(mod, n, fmt, ...) \
  debug_log_write(DLOG_TRACE, mod, n, fmt, ##__VA_ARGS__)

#endif /* Q16_DEBUG_LOG_H */
