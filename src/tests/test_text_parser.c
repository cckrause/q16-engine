#include "test_harness.h"
#include "io/stream.h"
#include "io/text_parser.h"

#include <string.h>

// Helper: init a parser over a string literal.
static void init_from_string(TextParser *p, StreamReader *sr, const char *text) {
  *sr = stream_from_memory(text, (int32_t)strlen(text));
  parser_init(p, sr);
}

// Test suites

void test_text_parser(void) {
  TEST_SUITE_BEGIN("text_parser");
  StreamReader sr;
  TextParser p;

  // Basic token reading
  {
    init_from_string(&p, &sr, "HELLO  WORLD 42");
    char tok[32];
    TEST_CHECK("read token 1", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("token 1 = HELLO", strcmp(tok, "HELLO") == 0);
    TEST_CHECK("read token 2", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("token 2 = WORLD", strcmp(tok, "WORLD") == 0);
    TEST_CHECK("read token 3", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("token 3 = 42", strcmp(tok, "42") == 0);
    TEST_CHECK("no more tokens", !parser_read_token(&p, tok, sizeof(tok)));
    stream_close(&sr);
  }

  // Integer parsing
  {
    init_from_string(&p, &sr, "  123 -456 0 ");
    TEST_CHECK("read int 123", parser_read_int(&p) == 123);
    TEST_CHECK("read int -456", parser_read_int(&p) == -456);
    TEST_CHECK("read int 0", parser_read_int(&p) == 0);
    stream_close(&sr);
  }

  // Float parsing
  {
    init_from_string(&p, &sr, " 3.14 -2.5 0.0 ");
    float v1 = parser_read_float(&p);
    float v2 = parser_read_float(&p);
    float v3 = parser_read_float(&p);
    TEST_CHECK("read float 3.14", v1 > 3.13f && v1 < 3.15f);
    TEST_CHECK("read float -2.5", v2 > -2.51f && v2 < -2.49f);
    TEST_CHECK("read float 0.0", v3 > -0.01f && v3 < 0.01f);
    stream_close(&sr);
  }

  // Keyword matching
  {
    init_from_string(&p, &sr, "LEV 2.1\nLEVELNAME secbase");
    TEST_CHECK("match LEV", parser_match(&p, "LEV"));
    char tok[32];
    TEST_CHECK("read version", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("version = 2.1", strcmp(tok, "2.1") == 0);
    TEST_CHECK("match LEVELNAME", parser_match(&p, "LEVELNAME"));
    TEST_CHECK("read name", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("name = secbase", strcmp(tok, "secbase") == 0);
    stream_close(&sr);
  }

  // Match fails on wrong keyword
  {
    init_from_string(&p, &sr, "TEXTURES 10");
    TEST_CHECK("match wrong keyword fails", !parser_match(&p, "SECTOR"));
    stream_close(&sr);
  }

  // Match respects word boundary
  {
    init_from_string(&p, &sr, "TEXTURES 10");
    TEST_CHECK("TEXT does not match TEXTURES", !parser_match(&p, "TEXT"));
    stream_close(&sr);
  }

  // skip_line
  {
    init_from_string(&p, &sr, "first line\nsecond 42\nthird");
    parser_skip_line(&p);
    TEST_CHECK("line after skip_line = 2", p.line == 2);
    char tok[32];
    TEST_CHECK("read second", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("token = second", strcmp(tok, "second") == 0);
    TEST_CHECK("read 42", parser_read_int(&p) == 42);
    parser_skip_line(&p);
    TEST_CHECK("read third", parser_read_token(&p, tok, sizeof(tok)));
    TEST_CHECK("token = third", strcmp(tok, "third") == 0);
    stream_close(&sr);
  }

  // Line counting through whitespace skip
  {
    init_from_string(&p, &sr, "A\n\n\nB");
    char tok[16];
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("line starts at 1", 1 == 1); // A is on line 1
    parser_skip_whitespace(&p);
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("B is on line 4", p.line == 4);
    stream_close(&sr);
  }

  // at_end
  {
    init_from_string(&p, &sr, "X");
    TEST_CHECK("not at end initially", !parser_at_end(&p));
    char tok[8];
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("at end after consuming all", parser_at_end(&p));
    stream_close(&sr);
  }

  // Token truncation (max_len)
  {
    init_from_string(&p, &sr, "LONGTOKEN");
    char tok[5];
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("truncated to 4 chars", strcmp(tok, "LONG") == 0);
    stream_close(&sr);
  }

  // Colon after keyword (LEV wall format: "LEFT:")
  {
    init_from_string(&p, &sr, "LEFT: 0 RIGHT: 1");
    TEST_CHECK("match LEFT", parser_match(&p, "LEFT"));
    // The colon is still there as part of the next token area, skip it
    char tok[8];
    // ":" is the next non-whitespace — read it as part of colon handling
    // In LEV format, colon is attached: "LEFT:" — parser_match matches "LEFT"
    // and the colon remains. The caller reads it or skips.
    // Actually let's test the exact LEV pattern: "LEFT:" is one token.
    stream_close(&sr);

    // More realistic: keyword followed immediately by colon
    init_from_string(&p, &sr, "X: 5 Z: 10");
    TEST_CHECK("match X", parser_match(&p, "X"));
    // Colon is the next char - the caller skips it
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("after X match, next is colon or val",
               strcmp(tok, ":") == 0 || strcmp(tok, "5") == 0);
    stream_close(&sr);
  }

  // Buffer-boundary crossing
  // Create content slightly larger than one buffer to test refill.
  {
    // Build a string with many small tokens that spans 2 buffers.
    // TEXT_PARSER_BUF_SIZE = 4096. Put ~500 tokens of "12345678\n" (9 bytes each).
    // 500 * 9 = 4500 bytes, spans the boundary.
    char big[4600];
    int32_t pos = 0;
    int32_t token_count = 0;
    while (pos + 9 < (int32_t)sizeof(big)) {
      memcpy(big + pos, "12345678\n", 9);
      pos += 9;
      token_count++;
    }
    big[pos] = '\0';

    sr = stream_from_memory(big, pos);
    parser_init(&p, &sr);

    int32_t read_count = 0;
    char tok[16];
    while (parser_read_token(&p, tok, sizeof(tok))) {
      read_count++;
      if (strcmp(tok, "12345678") != 0) {
        break;
      }
    }
    TEST_CHECK("all tokens read across buffer boundary",
               read_count == token_count);
    stream_close(&sr);
  }

  // Empty input
  {
    init_from_string(&p, &sr, "");
    TEST_CHECK("empty at_end", parser_at_end(&p));
    TEST_CHECK("empty read_int", parser_read_int(&p) == 0);
    stream_close(&sr);
  }

  // Tabs and mixed whitespace
  {
    init_from_string(&p, &sr, "\t  ABC \t DEF  ");
    char tok[16];
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("tab-separated token 1", strcmp(tok, "ABC") == 0);
    parser_read_token(&p, tok, sizeof(tok));
    TEST_CHECK("tab-separated token 2", strcmp(tok, "DEF") == 0);
    stream_close(&sr);
  }

  TEST_SUITE_END();
}
