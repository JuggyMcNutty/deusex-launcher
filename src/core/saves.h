/* A profile's saves: the folders of its Save folder (the engine's SavePath)
 * that the engine loads -- SaveNNNN, slot NNNN, as the engine names one
 * ("Save%04d"), and QuickSave, the quick save, slot -1 -- each with its
 * SaveInfo.dxs read (core/upkg_assets.h), by that name alone, as the
 * engine opens it: when it was made, what it says, where. Current, the
 * mission in progress, is no save. Newest first, as
 * the game's own Load Game lists them; one whose SaveInfo does not read
 * after them all.
 *
 * A save is started on with the engine's start URL ?loadgame=N (DXEngine's
 * NATIVES.md, the command line), when its --version has loadgame-url.
 */
#ifndef DXL_SAVES_H
#define DXL_SAVES_H

#include "core/upkg_assets.h"

#define DXL_SAVE_QUICK (-1)

typedef struct {
    int          slot;          /* NNNN; DXL_SAVE_QUICK for the quick save */
    char         folder[24];    /* SaveNNNN, QuickSave */
    int          readable;      /* its SaveInfo read; else info is empty */
    dxl_saveinfo info;          /* its snapshot not read */
} dxl_save;

typedef struct {
    dxl_save *items;
    int       count;
} dxl_saves;

/* The saves in dir. None there, or no dir: 0 and none. -1 and why for a
 * dir that is there and cannot be read. */
int  dxl_saves_list(const char *dir, dxl_saves *out, dxl_err *err);
void dxl_saves_free(dxl_saves *s);
/* How many dxl_saves_list gives for dir, no info read: what a profile's
 * save count and the import's say. */
int  dxl_saves_count(const char *dir);

/* The newest one that reads: what Continue loads. NULL for none. */
const dxl_save *dxl_saves_latest(const dxl_saves *s);

/* The slot of a folder's name, as the engine names its folders: 1, or 0
 * for a name that is not one of them. */
int  dxl_save_slot_of(const char *folder, int *slot);
/* Its start URL: "?loadgame=N". */
void dxl_save_url(int slot, char *out, size_t size);
/* Its folder, in dir. Caller frees. */
char *dxl_save_path(const char *dir, const dxl_save *s);
/* When it was made, for showing: "14 October 2026, 20:47"; "" for an info
 * that names no date. */
void dxl_save_when(const dxl_save *s, char *out, size_t size);
/* How long it had been played: "7 min", "2 h 05 min". */
void dxl_save_played(const dxl_save *s, char *out, size_t size);

#endif
