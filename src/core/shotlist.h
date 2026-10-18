/* A profile's screenshots: the pictures in its Screenshots folder -- the
 * engine's ShotNNNN.bmp (its --userdir's Screenshots links there,
 * core/profile.h), and the game's own, copied in as the profile was made
 * -- each a BMP or a PNG, newest first by when it was written.
 */
#ifndef DXL_SHOTLIST_H
#define DXL_SHOTLIST_H

#include "core/common.h"

typedef struct {
    char *path;
    char  name[256];
    long  mtime;       /* seconds since the epoch */
    long  size;        /* bytes */
} dxl_shot;

typedef struct {
    dxl_shot *items;
    int       count;
} dxl_shots;

/* The pictures in dir. None there, or no dir: 0 and none. -1 and why for a
 * dir that is there and cannot be read. */
int  dxl_shots_list(const char *dir, dxl_shots *out, dxl_err *err);
void dxl_shots_free(dxl_shots *s);
/* Whether a file's name is one of a picture the list takes. */
int  dxl_shot_name_ok(const char *name);
/* When it was written, for showing: "14 October 2026, 20:47". */
void dxl_shot_when(const dxl_shot *s, char *out, size_t size);

#endif
