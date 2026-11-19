/* The builds of Deus Ex the engine plays, by System/DeusEx.exe's SHA1.
 *
 * The engine takes a game folder only when that file's hash is in its own
 * list (DXEngine's UE1GameDatabase.h); with any other it finds no game
 * and exits. This list is the same, and Port Ex Machina's scripts/dx.sh
 * check holds the two together. 1112fm is the build the engine is made
 * against; 1002f, the first release, it starts too, with a warning here.
 *
 * An exe of no hash here (another launcher's in its place, a patched or a
 * localized one) is taken, with a warning, when the packages beside it
 * are 1112fm's: Core.u, Engine.u and DeusEx.u each with a GUID of the list
 * below in its header (bugs.md D.89), the engine taking such a folder too.
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

/* The build an install of an unknown exe is, when its packages are
 * 1112fm's. */
const dxl_game_build *dxl_gamedb_by_packages(void);

/* The packages that make a copy 1112fm's, in System, and the GUIDs each
 * may have (DXEngine's Package::GetGuidString: four little-endian 32-bit
 * words in upper-case hex), a row each: a store whose copy carries another
 * is a row more. */
typedef struct {
    const char *file;   /* "Core.u" */
    const char *guid;
} dxl_game_package;

size_t dxl_gamedb_package_count(void);
const dxl_game_package *dxl_gamedb_package_at(size_t i);
/* Whether guid (any case) is one of that package's (file, any case). */
int dxl_gamedb_package_known(const char *file, const char *guid);

#endif
