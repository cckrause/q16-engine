// ===========================================================================
// String Utilities
// ===========================================================================

#include "util/strings.h"

#include <ctype.h>
#include <string.h>

bool str_equal_nocase(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
    a++;
    b++;
  }
  return *a == *b;
}

bool str_ends_with_nocase(const char *str, const char *suffix) {
  size_t str_len = strlen(str);
  size_t suf_len = strlen(suffix);
  if (suf_len > str_len)
    return false;
  const char *tail = str + str_len - suf_len;
  for (size_t i = 0; i < suf_len; i++) {
    if (tolower((unsigned char)tail[i]) != tolower((unsigned char)suffix[i]))
      return false;
  }
  return true;
}
