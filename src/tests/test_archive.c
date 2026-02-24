#include "test_harness.h"
#include "archive/archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helpers

static void write_u32(uint8_t *buf, uint32_t v) {
  buf[0] = (uint8_t)(v);
  buf[1] = (uint8_t)(v >> 8);
  buf[2] = (uint8_t)(v >> 16);
  buf[3] = (uint8_t)(v >> 24);
}

// GOB builder — synthetic archive with two files
// "TEST.TXT"  → "Hello World" (11 bytes)
// "DATA.BIN"  → { 0xDE, 0xAD, 0xBE, 0xEF } (4 bytes)

#define TEST_GOB_PATH "__test_archive_gob.gob"
#define GOB_NAME_LEN  13

static bool create_test_gob(void) {
  const uint8_t file0_data[] = "Hello World";
  const uint32_t file0_len = 11;
  const uint8_t file1_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  const uint32_t file1_len = 4;

  const uint32_t header_size = 8;
  const uint32_t file0_offset = header_size;
  const uint32_t file1_offset = file0_offset + file0_len;
  const uint32_t dir_offset = file1_offset + file1_len;
  const uint32_t entry_size = 4 + 4 + GOB_NAME_LEN;
  const uint32_t dir_size = 4 + entry_size * 2;
  const uint32_t total = dir_offset + dir_size;

  uint8_t *buf = (uint8_t *)calloc(total, 1);
  if (!buf) {
    return false;
  }

  buf[0] = 'G'; buf[1] = 'O'; buf[2] = 'B'; buf[3] = '\n';
  write_u32(buf + 4, dir_offset);

  memcpy(buf + file0_offset, file0_data, file0_len);
  memcpy(buf + file1_offset, file1_data, file1_len);

  write_u32(buf + dir_offset, 2);

  uint8_t *e0 = buf + dir_offset + 4;
  write_u32(e0, file0_offset);
  write_u32(e0 + 4, file0_len);
  memcpy(e0 + 8, "TEST.TXT", 8);

  uint8_t *e1 = e0 + entry_size;
  write_u32(e1, file1_offset);
  write_u32(e1 + 4, file1_len);
  memcpy(e1 + 8, "DATA.BIN", 8);

  FILE *fp = fopen(TEST_GOB_PATH, "wb");
  if (!fp) {
    free(buf);
    return false;
  }
  bool ok = fwrite(buf, 1, total, fp) == total;
  fclose(fp);
  free(buf);
  return ok;
}

// LAB builder — synthetic archive with two files
// "HELLO.TXT"    → "Hello World" (11 bytes)
// "LONGNAME.DAT" → { 0xCA, 0xFE, 0xBA, 0xBE } (4 bytes)

#define TEST_LAB_PATH "__test_archive_lab.lab"

static bool create_test_lab(void) {
  const char *name0 = "HELLO.TXT";
  const char *name1 = "LONGNAME.DAT";
  const uint8_t file0_data[] = "Hello World";
  const uint32_t file0_len = 11;
  const uint8_t file1_data[] = {0xCA, 0xFE, 0xBA, 0xBE};
  const uint32_t file1_len = 4;

  uint32_t name0_size = (uint32_t)strlen(name0) + 1;
  uint32_t name1_off = name0_size;
  uint32_t name1_size = (uint32_t)strlen(name1) + 1;
  uint32_t string_table_size = name0_size + name1_size;

  uint32_t header_size = 16;
  uint32_t entry_size = 16;
  uint32_t dir_size = entry_size * 2;
  uint32_t data_start = header_size + dir_size + string_table_size;
  uint32_t file0_offset = data_start;
  uint32_t file1_offset = file0_offset + file0_len;
  uint32_t total = file1_offset + file1_len;

  uint8_t *buf = (uint8_t *)calloc(total, 1);
  if (!buf) {
    return false;
  }

  buf[0] = 'L'; buf[1] = 'A'; buf[2] = 'B'; buf[3] = 'N';
  write_u32(buf + 4, 0x00010000);
  write_u32(buf + 8, 2);
  write_u32(buf + 12, string_table_size);

  uint8_t *e0 = buf + header_size;
  write_u32(e0 + 0, 0);
  write_u32(e0 + 4, file0_offset);
  write_u32(e0 + 8, file0_len);
  write_u32(e0 + 12, 0);

  uint8_t *e1 = e0 + entry_size;
  write_u32(e1 + 0, name1_off);
  write_u32(e1 + 4, file1_offset);
  write_u32(e1 + 8, file1_len);
  write_u32(e1 + 12, 0);

  uint8_t *st = buf + header_size + dir_size;
  memcpy(st, name0, name0_size);
  memcpy(st + name1_off, name1, name1_size);

  memcpy(buf + file0_offset, file0_data, file0_len);
  memcpy(buf + file1_offset, file1_data, file1_len);

  FILE *fp = fopen(TEST_LAB_PATH, "wb");
  if (!fp) {
    free(buf);
    return false;
  }
  bool ok = fwrite(buf, 1, total, fp) == total;
  fclose(fp);
  free(buf);
  return ok;
}

// Shared I/O test suite — runs against any Archive handle
// Expects two files:
//   index 0: text file  "Hello World" (11 bytes)  — name0
//   index 1: binary file 4 bytes                   — name1, bytes b0-b3

static void run_io_tests(Archive *ar,
                         const char *name0, const char *name1,
                         uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  // Directory queries
  TEST_CHECK("file count is 2", archive_get_file_count(ar) == 2);

  TEST_CHECK("file 0 name correct",
             strcmp(archive_get_file_name(ar, 0), name0) == 0);
  TEST_CHECK("file 1 name correct",
             strcmp(archive_get_file_name(ar, 1), name1) == 0);

  TEST_CHECK("file 0 length is 11", archive_get_file_length(ar, 0) == 11);
  TEST_CHECK("file 1 length is 4", archive_get_file_length(ar, 1) == 4);

  TEST_CHECK("get_file_name(-1) returns NULL",
             archive_get_file_name(ar, -1) == NULL);
  TEST_CHECK("get_file_name(2) returns NULL",
             archive_get_file_name(ar, 2) == NULL);
  TEST_CHECK("get_file_length(-1) returns 0",
             archive_get_file_length(ar, -1) == 0);

  // Case-insensitive lookup
  TEST_CHECK("find exact name0", archive_get_file_index(ar, name0) == 0);
  TEST_CHECK("find name1", archive_get_file_index(ar, name1) == 1);
  TEST_CHECK("not found", archive_get_file_index(ar, "NOPE.BIN") == -1);

  TEST_CHECK("file_exists name0", archive_file_exists(ar, name0));
  TEST_CHECK("!file_exists MISSING", !archive_file_exists(ar, "MISSING"));

  // Read text file
  {
    TEST_CHECK("open text file", archive_open_file(ar, name0));

    char buf[32] = {0};
    int32_t n = archive_read_file(ar, buf, 32);
    TEST_CHECK("read returns 11 bytes", n == 11);
    TEST_CHECK("read content matches", memcmp(buf, "Hello World", 11) == 0);

    n = archive_read_file(ar, buf, 1);
    TEST_CHECK("read past end returns 0", n == 0);

    archive_close_file(ar);
  }

  // Read binary file
  {
    TEST_CHECK("open binary file", archive_open_file(ar, name1));

    uint8_t buf[8] = {0};
    int32_t n = archive_read_file(ar, buf, 8);
    TEST_CHECK("read returns 4 bytes", n == 4);
    TEST_CHECK("byte 0 correct", buf[0] == b0);
    TEST_CHECK("byte 1 correct", buf[1] == b1);
    TEST_CHECK("byte 2 correct", buf[2] == b2);
    TEST_CHECK("byte 3 correct", buf[3] == b3);

    archive_close_file(ar);
  }

  // Open by index
  {
    TEST_CHECK("open index 1", archive_open_file_index(ar, 1));

    uint8_t buf[4];
    int32_t n = archive_read_file(ar, buf, 4);
    TEST_CHECK("read index 1 returns 4", n == 4);
    TEST_CHECK("index 1 byte 0 correct", buf[0] == b0);

    archive_close_file(ar);
  }

  // Invalid open
  TEST_CHECK("open nonexistent file fails",
             !archive_open_file(ar, "NOPE.TXT"));
  TEST_CHECK("open index -1 fails", !archive_open_file_index(ar, -1));
  TEST_CHECK("open index 2 fails", !archive_open_file_index(ar, 2));

  // Seek
  {
    TEST_CHECK("open text file for seek", archive_open_file(ar, name0));

    TEST_CHECK("loc starts at 0", archive_get_loc_in_file(ar) == 0);

    TEST_CHECK("seek SET 6", archive_seek_file(ar, 6, SEEK_SET));
    TEST_CHECK("loc after SET 6", archive_get_loc_in_file(ar) == 6);

    char buf[8] = {0};
    int32_t n = archive_read_file(ar, buf, 8);
    TEST_CHECK("read after seek returns 5", n == 5);
    TEST_CHECK("read after seek = 'World'", memcmp(buf, "World", 5) == 0);

    TEST_CHECK("seek SET 0", archive_seek_file(ar, 0, SEEK_SET));
    TEST_CHECK("loc after SET 0", archive_get_loc_in_file(ar) == 0);

    TEST_CHECK("seek CUR +5", archive_seek_file(ar, 5, SEEK_CUR));
    TEST_CHECK("loc after CUR +5", archive_get_loc_in_file(ar) == 5);

    TEST_CHECK("seek END -5", archive_seek_file(ar, -5, SEEK_END));
    TEST_CHECK("loc after END -5", archive_get_loc_in_file(ar) == 6);

    TEST_CHECK("seek SET -1 fails", !archive_seek_file(ar, -1, SEEK_SET));
    TEST_CHECK("seek SET 12 fails", !archive_seek_file(ar, 12, SEEK_SET));

    archive_close_file(ar);
  }

  // Loc with no file open
  TEST_CHECK("loc with no file returns -1", archive_get_loc_in_file(ar) == -1);

  // Opening a new file closes the previous one
  {
    TEST_CHECK("open text file", archive_open_file(ar, name0));
    TEST_CHECK("open binary file replaces", archive_open_file(ar, name1));

    uint8_t buf[4];
    int32_t n = archive_read_file(ar, buf, 4);
    TEST_CHECK("replaced file reads correctly", n == 4);
    TEST_CHECK("replaced read byte 0", buf[0] == b0);

    archive_close_file(ar);
  }

  // Partial read
  {
    TEST_CHECK("open text file for partial", archive_open_file(ar, name0));

    char buf[5] = {0};
    int32_t n = archive_read_file(ar, buf, 5);
    TEST_CHECK("partial read returns 5", n == 5);
    TEST_CHECK("partial content 'Hello'", memcmp(buf, "Hello", 5) == 0);
    TEST_CHECK("loc after partial read", archive_get_loc_in_file(ar) == 5);

    char buf2[8] = {0};
    n = archive_read_file(ar, buf2, 8);
    TEST_CHECK("rest read returns 6", n == 6);
    TEST_CHECK("rest content ' World'", memcmp(buf2, " World", 6) == 0);

    archive_close_file(ar);
  }

  // Read with zero/negative size
  {
    TEST_CHECK("open for edge cases", archive_open_file(ar, name0));
    char buf[4];
    TEST_CHECK("read size 0 returns 0", archive_read_file(ar, buf, 0) == 0);
    TEST_CHECK("read size -1 returns 0", archive_read_file(ar, buf, -1) == 0);
    TEST_CHECK("read NULL data returns 0", archive_read_file(ar, NULL, 4) == 0);
    archive_close_file(ar);
  }
}

// Test suites

static void test_archive_gob(void) {
  TEST_SUITE_BEGIN("archive (GOB)");

  bool created = create_test_gob();
  TEST_CHECK("create test GOB file", created);
  if (!created) {
    TEST_SUITE_END();
    return;
  }

  Archive *ar = archive_open(TEST_GOB_PATH);
  TEST_CHECK("archive_open GOB succeeds", ar != NULL);
  if (!ar) {
    remove(TEST_GOB_PATH);
    TEST_SUITE_END();
    return;
  }

  TEST_CHECK("format is GOB", archive_get_format(ar) == ARCHIVE_FMT_GOB);

  run_io_tests(ar, "TEST.TXT", "DATA.BIN", 0xDE, 0xAD, 0xBE, 0xEF);

  archive_close(ar);
  remove(TEST_GOB_PATH);

  TEST_SUITE_END();
}

static void test_archive_lab(void) {
  TEST_SUITE_BEGIN("archive (LAB)");

  bool created = create_test_lab();
  TEST_CHECK("create test LAB file", created);
  if (!created) {
    TEST_SUITE_END();
    return;
  }

  Archive *ar = archive_open(TEST_LAB_PATH);
  TEST_CHECK("archive_open LAB succeeds", ar != NULL);
  if (!ar) {
    remove(TEST_LAB_PATH);
    TEST_SUITE_END();
    return;
  }

  TEST_CHECK("format is LAB", archive_get_format(ar) == ARCHIVE_FMT_LAB);

  run_io_tests(ar, "HELLO.TXT", "LONGNAME.DAT", 0xCA, 0xFE, 0xBA, 0xBE);

  archive_close(ar);
  remove(TEST_LAB_PATH);

  TEST_SUITE_END();
}

static void test_archive_format_detection(void) {
  TEST_SUITE_BEGIN("archive (format detection)");

  bool gob_ok = create_test_gob();
  bool lab_ok = create_test_lab();
  TEST_CHECK("create GOB for detection", gob_ok);
  TEST_CHECK("create LAB for detection", lab_ok);

  if (gob_ok) {
    Archive *ar = archive_open(TEST_GOB_PATH);
    TEST_CHECK("GOB opens", ar != NULL);
    if (ar) {
      TEST_CHECK("detected GOB format",
                 archive_get_format(ar) == ARCHIVE_FMT_GOB);
      archive_close(ar);
    }
    remove(TEST_GOB_PATH);
  }

  if (lab_ok) {
    Archive *ar = archive_open(TEST_LAB_PATH);
    TEST_CHECK("LAB opens", ar != NULL);
    if (ar) {
      TEST_CHECK("detected LAB format",
                 archive_get_format(ar) == ARCHIVE_FMT_LAB);
      archive_close(ar);
    }
    remove(TEST_LAB_PATH);
  }

  TEST_SUITE_END();
}

static void test_archive_invalid(void) {
  TEST_SUITE_BEGIN("archive (invalid input)");

  TEST_CHECK("NULL path returns NULL", archive_open(NULL) == NULL);
  TEST_CHECK("nonexistent file returns NULL",
             archive_open("__nonexistent_archive.dat") == NULL);

  // Bad magic bytes
  {
    const char *bad_path = "__test_archive_bad_magic.bin";
    FILE *fp = fopen(bad_path, "wb");
    if (fp) {
      uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
      fwrite(garbage, 1, sizeof(garbage), fp);
      fclose(fp);

      TEST_CHECK("bad magic returns NULL", archive_open(bad_path) == NULL);
      remove(bad_path);
    }
  }

  // Truncated file (only 2 bytes)
  {
    const char *trunc_path = "__test_archive_truncated.bin";
    FILE *fp = fopen(trunc_path, "wb");
    if (fp) {
      uint8_t two_bytes[] = {0x47, 0x4F};
      fwrite(two_bytes, 1, 2, fp);
      fclose(fp);

      TEST_CHECK("truncated file returns NULL",
                 archive_open(trunc_path) == NULL);
      remove(trunc_path);
    }
  }

  // archive_close(NULL) is safe
  archive_close(NULL);
  TEST_CHECK("close NULL is safe", true);

  TEST_SUITE_END();
}

// Public entry point

void test_archive(void) {
  test_archive_gob();
  test_archive_lab();
  test_archive_format_detection();
  test_archive_invalid();
}
