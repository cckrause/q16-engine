// ===========================================================================
// TextParser — Buffered Line Tokenizer
// ===========================================================================
// Reads lines and key-value pairs from a StreamReader with internal buffering.

#include "io/text_parser.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// Buffer management

// Shift unconsumed tail to front, then refill the rest from the reader.
static void refill(TextParser *p) {
  if (p->eof) {
    return;
  }

  int32_t remaining = p->buf_len - p->buf_pos;
  if (remaining > 0) {
    memmove(p->buf, p->buf + p->buf_pos, (size_t)remaining);
  }
  p->buf_pos = 0;
  p->buf_len = remaining;

  int32_t space = TEXT_PARSER_BUF_SIZE - p->buf_len;
  if (space > 0) {
    int32_t n = p->reader->read(p->reader->ctx, p->buf + p->buf_len, space);
    if (n <= 0) {
      p->eof = true;
    } else {
      p->buf_len += n;
    }
  }
}

// Ensure at least 1 byte is available in buf. Returns false at true EOF.
static bool ensure(TextParser *p) {
  if (p->buf_pos < p->buf_len) {
    return true;
  }
  refill(p);
  return p->buf_pos < p->buf_len;
}

static char advance(TextParser *p) {
  if (!ensure(p)) {
    return '\0';
  }
  char c = p->buf[p->buf_pos++];
  if (c == '\n') {
    p->line++;
  }
  return c;
}

// Public API

void parser_init(TextParser *p, StreamReader *reader) {
  memset(p, 0, sizeof(*p));
  p->reader = reader;
  p->line = 1;
  refill(p);
}

bool parser_at_end(const TextParser *p) {
  return p->eof && p->buf_pos >= p->buf_len;
}

void parser_skip_whitespace(TextParser *p) {
  for (;;) {
    while (ensure(p)) {
      char c = p->buf[p->buf_pos];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        advance(p);
      } else {
        break;
      }
    }
    if (!ensure(p)) {
      break;
    }

    char c = p->buf[p->buf_pos];

    // '#' line comment
    if (c == '#') {
      parser_skip_line(p);
      continue;
    }

    if (c == '/') {
      // Ensure 2 bytes buffered for lookahead
      while (p->buf_len - p->buf_pos < 2 && !p->eof) {
        refill(p);
      }
      if (p->buf_len - p->buf_pos >= 2) {
        char c2 = p->buf[p->buf_pos + 1];
        if (c2 == '/') {
          parser_skip_line(p);
          continue;
        }
        if (c2 == '*') {
          advance(p);
          advance(p);
          while (ensure(p)) {
            if (p->buf[p->buf_pos] == '*') {
              advance(p);
              if (ensure(p) && p->buf[p->buf_pos] == '/') {
                advance(p);
                break;
              }
            } else {
              advance(p);
            }
          }
          continue;
        }
      }
    }

    break;
  }
}

void parser_skip_line(TextParser *p) {
  while (ensure(p)) {
    char c = advance(p);
    if (c == '\n') {
      return;
    }
  }
}

bool parser_match(TextParser *p, const char *keyword) {
  parser_skip_whitespace(p);

  int32_t kw_len = (int32_t)strlen(keyword);
  if (kw_len == 0) {
    return true;
  }

  // Ensure enough bytes are buffered for the keyword comparison.
  // If the keyword is longer than what we have, refill.
  while (p->buf_len - p->buf_pos < kw_len && !p->eof) {
    refill(p);
  }

  int32_t avail = p->buf_len - p->buf_pos;
  if (avail < kw_len) {
    return false;
  }

  if (memcmp(p->buf + p->buf_pos, keyword, (size_t)kw_len) != 0) {
    return false;
  }

  // Check that the match is at a word boundary (next char is whitespace,
  // colon, EOF, or end of available data).
  if (avail > kw_len) {
    char next = p->buf[p->buf_pos + kw_len];
    if (!isspace((unsigned char)next) && next != ':' && next != '\0') {
      return false;
    }
  }

  // Consume the keyword.
  for (int32_t i = 0; i < kw_len; i++) {
    advance(p);
  }
  return true;
}

bool parser_read_token(TextParser *p, char *out, int32_t max_len) {
  parser_skip_whitespace(p);

  int32_t len = 0;
  while (ensure(p)) {
    char c = p->buf[p->buf_pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      break;
    }
    if (len < max_len - 1) {
      out[len++] = c;
    }
    advance(p);
  }
  out[len] = '\0';
  return len > 0;
}

bool parser_read_line_token(TextParser *p, char *out, int32_t max_len) {
  // Skip spaces and tabs only — stop at newlines
  while (ensure(p)) {
    char c = p->buf[p->buf_pos];
    if (c == ' ' || c == '\t') {
      advance(p);
    } else {
      break;
    }
  }

  // If next char is newline, CR, or EOF -> no token on this line
  if (!ensure(p) || p->buf[p->buf_pos] == '\n' || p->buf[p->buf_pos] == '\r') {
    out[0] = '\0';
    return false;
  }

  // '#' or '//' inline comment → end of line content
  if (p->buf[p->buf_pos] == '#') {
    out[0] = '\0';
    return false;
  }
  if (p->buf[p->buf_pos] == '/') {
    while (p->buf_len - p->buf_pos < 2 && !p->eof) {
      refill(p);
    }
    if (p->buf_len - p->buf_pos >= 2 && p->buf[p->buf_pos + 1] == '/') {
      out[0] = '\0';
      return false;
    }
  }

  int32_t len = 0;
  while (ensure(p)) {
    char c = p->buf[p->buf_pos];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      break;
    }
    if (len < max_len - 1) {
      out[len++] = c;
    }
    advance(p);
  }
  out[len] = '\0';
  return len > 0;
}

int32_t parser_read_int(TextParser *p) {
  char token[32];
  if (!parser_read_token(p, token, sizeof(token))) {
    return 0;
  }
  return (int32_t)strtol(token, NULL, 10);
}

int32_t parser_read_hex_int(TextParser *p) {
  char token[32];
  if (!parser_read_token(p, token, sizeof(token))) {
    return 0;
  }
  return (int32_t)strtol(token, NULL, 16);
}

float parser_read_float(TextParser *p) {
  char token[32];
  if (!parser_read_token(p, token, sizeof(token))) {
    return 0.0f;
  }
  return strtof(token, NULL);
}
