/* What the launcher takes out of a package's objects (core/upkg.h): a
 * texture's first mipmap as RGBA pixels, a sound's or a music's file as the
 * package keeps it, a class's default properties, and a save's
 * DeusExSaveInfo. Each from the bytes alone, every read checked; one it
 * cannot read is -1 and why, and the caller has its fallback (our own art,
 * silence). No SDL here.
 */
#ifndef DXL_UPKG_ASSETS_H
#define DXL_UPKG_ASSETS_H

#include "core/upkg.h"

typedef struct {
    int      w, h;
    uint8_t *rgba;        /* w * h * 4 bytes, top row first */
    int      masked;      /* bMasked: its palette's first colour clear */
} dxl_uimage;

void dxl_uimage_free(dxl_uimage *im);

/* Texture e's first mipmap: P8 through its palette -- the package's own,
 * else a grey ramp -- the first colour clear when it is masked; BGRA8 as it
 * is; DXT1 decoded. -1 and why for a texture of another format, or one
 * whose bytes do not hold together. */
int dxl_upkg_texture(const dxl_upkg *p, int32_t e, dxl_uimage *out, dxl_err *err);

/* Sound or Music e's file -- a WAV, an IT module -- and its type ("wav",
 * "it"), from the bytes after its properties. Caller frees; NULL and why. */
uint8_t *dxl_upkg_file(const dxl_upkg *p, int32_t e, size_t *len, char *type, size_t type_size, dxl_err *err);

/* Class e's default properties: r over them, past its field's, struct's,
 * state's and class's headers. -1 for one with script, whose bytecode this
 * does not read (a menu theme's class has none). */
int dxl_upkg_class_defaults(const dxl_upkg *p, int32_t e, dxl_ureader *r);

/* A save's SaveInfo.dxs: when it was made, what it says, where, and its
 * snapshot; its words as UTF-8 (core/common.h's dxl_text_utf8). */
typedef struct {
    int        year, month, day, hour, minute, second;
    int        directory_index;    /* its SaveNNNN's number */
    char       description[128];
    char       location[128];      /* MissionLocation */
    char       map[64];            /* MapName */
    int        play_seconds;       /* saveTime */
    dxl_uimage snapshot;           /* rgba NULL when it has none */
} dxl_saveinfo;

/* The file at path read; with_snapshot 0, its snapshot not decoded. */
int  dxl_saveinfo_read(const char *path, int with_snapshot, dxl_saveinfo *out, dxl_err *err);
void dxl_saveinfo_free(dxl_saveinfo *s);

#endif
