// ===========================================================================
// String Utilities
// ===========================================================================

#include "util/strings.h"

#include <ctype.h>

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
