/* The finder's places apart from its looking (core/finder.h): where a
 * platform's stores and drives put the game are finder_places.c's, desktop
 * Linux's, or finder_places_win32.c's, Windows', made of what
 * core/finder.c's looking offers below; one finder's state, shared by the
 * two.
 */
#ifndef DXL_FINDER_PLACES_H
#define DXL_FINDER_PLACES_H

#include "core/finder.h"

typedef struct {
    const char        *home, *config, *data, *wineprefix, *mounts;
    const char *const *media;
    char              *own[3];      /* the paths made up for those */
    dxl_found_fn       found;
    void              *ctx;
    const atomic_int  *stop;
    char              *seen[DXL_FOUND_MAX];   /* the folders listed, as they really are */
    int                seen_count;
} finder;

/* ---- the looking (core/finder.c) ---------------------------------------- */

/* Whether to stop: asked to, or the list is full. */
int   dxl_finder_stopped(const finder *f);
/* A regular file's text, if it is no larger than max. Caller frees. */
char *dxl_finder_read_small(const char *path, long max);
/* dir checked and listed, once; always: listed whatever is there. */
void  dxl_finder_add(finder *f, const char *source, const char *dir, int always);
/* Whether dir holds the game. */
int   dxl_finder_has_game(const char *dir);
/* Each folder in dir that holds the game. */
void  dxl_finder_add_children(finder *f, const char *source, const char *dir);
/* dir itself, and its folders depth deep. */
void  dxl_finder_within(finder *f, const char *source, const char *dir, int depth);
/* A Windows drive's top: where Windows' installers put the game. */
void  dxl_finder_drive(finder *f, const char *source, const char *root);
/* A Wine prefix, and each folder in dir that is one. */
void  dxl_finder_prefix(finder *f, const char *source, const char *dir);
void  dxl_finder_prefixes_in(finder *f, const char *source, const char *dir);
/* A Steam's libraries; wine, the prefix of a Windows Steam's (NULL: none). */
void  dxl_finder_steam(finder *f, const char *source, const char *root, const char *wine);
/* Heroic's GOG games, from its config folder. */
void  dxl_finder_heroic(finder *f, const char *config);

/* ---- the platform's places (core/finder_places.c, _win32.c) ------------- */

/* Where from: w's, else this machine's own. */
void dxl_finder_setup(finder *f, const dxl_finder_where *w);
/* Each place in turn, after the folder given and the known ones. */
void dxl_finder_places(finder *f);

#endif
