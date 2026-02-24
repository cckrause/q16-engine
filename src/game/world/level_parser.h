// ===========================================================================
// Level Geometry Parser
// ===========================================================================
#ifndef Q16_LEVEL_PARSER_H
#define Q16_LEVEL_PARSER_H

#include "io/stream.h"
#include "types/forward.h"
#include <stdbool.h>

// Auto-detects format (LEV 2.1 / LVT 1.1) from the magic line, then parses
// with a unified keyword-driven sector/wall parser that handles both formats.

typedef enum LevelFormat {
  LEVEL_FMT_LEV, // Dark Forces text .LEV 2.1
  LEVEL_FMT_LVT, // Outlaws text .LVT 1.1
  LEVEL_FMT_LVB, // binary .LVB (future)
} LevelFormat;

// Auto-detect format and parse. Allocates sectors, walls, vertices, stub
// textures from the level region. Returns true on success.
bool level_load_geometry(StreamReader *reader, LevelState *state);

#endif /* Q16_LEVEL_PARSER_H */
