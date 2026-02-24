#ifndef Q16_STRINGS_H
#define Q16_STRINGS_H

#include <stdbool.h>

// Case-insensitive string equality. Uses tolower() from <ctype.h>,
// safe for all single-byte encodings. Returns true if both strings
// have the same length and identical characters ignoring case.
bool str_equal_nocase(const char *a, const char *b);

#endif /* Q16_STRINGS_H */
