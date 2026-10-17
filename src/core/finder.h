/* Where Deus Ex may be on this machine: the places the first run looks in
 * before the player shows it the folder.
 *
 *   the folder given (--game=<dir>)
 *   the installs launcher.ini lists
 *   Steam's own library, as Steam installs it natively, as a Flatpak and as
 *   a Snap: <Steam>/steamapps/common/Deus Ex
 *   each folder in ~/Games/Heroic and in ~/GOG Games
 *
 * Each folder there is checked (core/install.h) and listed once, however
 * many ways lead to it; one that is not there is not listed, though the
 * folder given always is, with what is wrong with it. Nothing is written.
 */
#ifndef DXL_FINDER_H
#define DXL_FINDER_H

#include "core/install.h"

#define DXL_FOUND_MAX 16

typedef struct {
    const char *source;     /* where it was found, for the screens: "the folder given",
                               "in Steam's library", ... */
    dxl_install install;    /* the folder, checked */
} dxl_found;

typedef struct {
    dxl_found items[DXL_FOUND_MAX];
    int       count;
} dxl_found_list;

/* Looks in home's places (home NULL: $HOME), given first (may be NULL),
 * then known (NULL-terminated, may be NULL). */
void dxl_finder_quick(dxl_found_list *out, const char *home, const char *given, const char *const *known);
void dxl_found_list_free(dxl_found_list *l);

#endif
