/* Where Deus Ex may be on this machine: the places the first run looks in
 * before the player shows it the folder, in this order:
 *
 *   the folder given (--game=<dir>), and the installs launcher.ini lists
 *   Steam's libraries, each its libraryfolders.vdf lists, the game where
 *     its appmanifest_6910.acf puts it: Steam's own, natively, as a
 *     Flatpak and as a Snap
 *   Heroic's GOG games (gog_store/installed.json, natively and as a
 *     Flatpak), and each folder in ~/Games/Heroic
 *   each folder in ~/GOG Games and in ~/Games
 *   Lutris' games: what each of its games/<game>.yml runs, and its Wine
 *     prefix
 *   Bottles' bottles, and the other Wine prefixes: $WINEPREFIX, ~/.wine,
 *     winetricks', those in ~/Games and Heroic's
 *   the drives mounted in /run/media, /media and /mnt
 *
 * In a Wine prefix's drive_c, and on a drive, the game is looked for where
 * Windows' installers put it: DeusEx, the CD's folder; each folder in GOG
 * Games, GOG Galaxy's Games and Games; Steam's library, Windows' own and
 * one of its own. On a drive also at its top and two folders down.
 *
 * Each folder there is checked (core/install.h) and listed once, however
 * many ways lead to it; one that is not there is not listed, though the
 * folder given always is, with what is wrong with it. Nothing is written.
 */
#ifndef DXL_FINDER_H
#define DXL_FINDER_H

#include "core/install.h"

#define DXL_FOUND_MAX 32

typedef struct {
    const char *source;     /* where it was found, for the screens: "the folder given",
                               "in Steam's library", ... */
    dxl_install install;    /* the folder, checked */
} dxl_found;

typedef struct {
    dxl_found items[DXL_FOUND_MAX];
    int       count;
} dxl_found_list;

/* Where to look. home NULL: this machine's own -- $HOME, and the
 * environment's XDG folders, $WINEPREFIX and the folders drives are
 * mounted in; given a home, none of the environment's, and each NULL left
 * as home has it (its .config, its .local/share, no prefix, no drives). */
typedef struct {
    const char        *home;
    const char        *config_home;   /* $XDG_CONFIG_HOME */
    const char        *data_home;     /* $XDG_DATA_HOME */
    const char        *wineprefix;
    const char *const *media;         /* NULL-terminated: /run/media/<user>, /run/media,
                                         /media/<user>, /media, /mnt */
    const char        *given;         /* listed first, whatever is there */
    const char *const *known;         /* NULL-terminated, may be NULL */
} dxl_finder_where;

/* Each install found, handed over as it is, in the list's order: its
 * strings are the finder's, to copy. */
typedef void (*dxl_found_fn)(void *ctx, const dxl_found *f);

/* Looks in each place in turn; stops early once *stop is set (stop may be
 * NULL), between one place and the next. */
void dxl_finder_run(const dxl_finder_where *where, dxl_found_fn found, void *ctx, const volatile int *stop);
/* All of them, into out. */
void dxl_finder_find(dxl_found_list *out, const dxl_finder_where *where);

/* A copy of f at the list's end, its strings the list's own: -1 when the
 * list is full. */
int  dxl_found_list_add(dxl_found_list *l, const dxl_found *f);
void dxl_found_list_free(dxl_found_list *l);

#endif
