#ifndef Q16_FORWARD_H
#define Q16_FORWARD_H

#include "types/types.h"

// --- World geometry --------------------------------------------------------
typedef struct Sector Sector;
typedef struct Wall Wall;
typedef struct SecObject SecObject;
typedef struct Texture Texture;
typedef struct JediWax JediWax;
typedef struct WaxAnim WaxAnim;
typedef struct WaxView WaxView;
typedef struct WaxFrame WaxFrame;
typedef struct WaxCell WaxCell;
typedef struct JediModel JediModel;
typedef struct JediPolygon JediPolygon;
typedef struct JediSubObject JediSubObject;
typedef struct LevelState LevelState;

// --- Systems (forward-declared for pointer fields) -------------------------
// Guard each typedef: if the real header was already included, skip the
// forward declaration to avoid C11 "redefinition of typedef" warnings.
#ifndef Q16_ALLOCATOR_H
typedef struct Allocator Allocator;
#endif
#ifndef Q16_TASK_H
typedef struct Task Task;
#endif
typedef struct Logic Logic;
typedef struct ProjectileLogic ProjectileLogic;
typedef struct InfElevator InfElevator;
typedef struct InfTrigger InfTrigger;
typedef struct InfLink InfLink;
typedef struct Stop Stop;

#endif /* Q16_FORWARD_H */
