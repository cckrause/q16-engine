// ===========================================================================
// TextParser — Buffered tokenizer over StreamReader
// ===========================================================================
#ifndef Q16_TEXT_PARSER_H
#define Q16_TEXT_PARSER_H

#include "io/stream.h"
#include <stdbool.h>
#include <stdint.h>

// Reads tokens from a StreamReader through a small internal buffer.
// Never holds more than TEXT_PARSER_BUF_SIZE bytes in memory at once.
// Reusable for LEV, O, 3DO, INF, GOL and any other text-based format.
//
// All parse functions skip leading whitespace (spaces + tabs) before
// reading. Newlines are NOT whitespace — they must be consumed explicitly
// via parser_skip_line or implicitly when encountered during skip.

#define TEXT_PARSER_BUF_SIZE 4096

typedef struct TextParser {
  StreamReader *reader;
  char buf[TEXT_PARSER_BUF_SIZE];
  int32_t buf_pos; // next byte to consume
  int32_t buf_len; // valid bytes in buf
  int32_t line;    // current line number (1-based)
  bool eof;        // underlying reader exhausted
} TextParser;

// Initialize parser over an open StreamReader.
void parser_init(TextParser *p, StreamReader *reader);

// True when all data has been consumed.
bool parser_at_end(const TextParser *p);

// Skip spaces, tabs, carriage returns, and newlines.
void parser_skip_whitespace(TextParser *p);

// Advance past the next newline (consuming the rest of the current line).
void parser_skip_line(TextParser *p);

// Try to match a keyword at the current position (after skipping whitespace).
// On match, the keyword is consumed and true is returned.
// On mismatch, the position is NOT rewound — use parser_peek_token first
// if you need non-destructive lookahead.
bool parser_match(TextParser *p, const char *keyword);

// Read the next whitespace-delimited token into out[].
// Returns false at EOF or if no token found. Always null-terminates.
bool parser_read_token(TextParser *p, char *out, int32_t max_len);

// Read a token from the remainder of the current line only.
// Skips inline whitespace (spaces, tabs) but does NOT cross newlines.
// Returns false if no token exists before the next newline/EOF.
bool parser_read_line_token(TextParser *p, char *out, int32_t max_len);

// Parse the next token as a decimal integer.
int32_t parser_read_int(TextParser *p);

// Parse the next token as a hexadecimal integer (no "0x" prefix expected).
int32_t parser_read_hex_int(TextParser *p);

// Parse the next token as a floating-point number.
float parser_read_float(TextParser *p);

#endif /* Q16_TEXT_PARSER_H */
