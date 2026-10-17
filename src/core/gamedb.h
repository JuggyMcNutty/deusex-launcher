/* The builds of Deus Ex the engine plays, by System/DeusEx.exe's SHA1.
 *
 * The engine takes a game folder only when that file's hash is in its own
 * list (VibeEngine's UE1GameDatabase.h); with any other it finds no game
 * and exits. This list is the same, and Port Ex Machina's scripts/dx.sh
 * check holds the two together. 1112fm is the build the engine is made
 * against; 1002f, the first release, it starts too, with a warning here.
 */
#ifndef DXL_GAMEDB_H
#define DXL_GAMEDB_H

#include <stddef.h>

typedef struct {
    const char *sha1;      /* DeusEx.exe's, lower-case hex */
    const char *version;   /* "1112fm", "1002f" */
    const char *edition;   /* who sells it so, for the screens */
    int         current;   /* 1: the build the engine is made against */
} dxl_game_build;

/* The build of that hash (any case), or NULL for one the engine does not
 * take. */
const dxl_game_build *dxl_gamedb_find(const char *sha1_hex);

size_t dxl_gamedb_count(void);
const dxl_game_build *dxl_gamedb_at(size_t i);

#endif
