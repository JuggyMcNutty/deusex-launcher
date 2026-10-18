/* Mods. A mod is a folder of its own, in the game's folder or anywhere,
 * with a System folder (any case) of its packages, and is played through a
 * profile of its own (core/profile.h's dxl_profile_import_mod) whose paths
 * name the mod's folders before the game's, by their absolute paths: the
 * engine finds its packages and maps there, and, with its int-paths
 * feature, its .int files' text (VibeEngine's NATIVES.md, mods). Nothing
 * of it is copied, and nothing is written to it.
 *
 * Its settings are its own ini's when it has one: one of System's with
 * [Core.System] Paths, the one named after the mod's folder first, then
 * any but Default.ini, then Default.ini. Its controls are the user ini
 * beside that one: <name>User.ini, User.ini beside DeusEx.ini, else
 * DefUser.ini.
 */
#ifndef DXL_MODS_H
#define DXL_MODS_H

#include "core/common.h"
#include "core/ini.h"

typedef enum {
    DXL_MOD_GAME,        /* a folder of the game's folder */
    DXL_MOD_LAUNCHER,    /* a folder of the launcher's mods folder */
    DXL_MOD_ELSEWHERE    /* anywhere else: one shown, or a profile's */
} dxl_mod_where;

typedef struct {
    char *dir;           /* its folder, absolute */
    char *name;          /* the folder's name */
    char *system_dir;    /* its System folder */
    char *ini;           /* its own settings; NULL for none */
    char *user_ini;      /* its own controls; NULL for none */
    int   packages;      /* System's packages (.u) */
    dxl_mod_where where;
} dxl_mod;

/* dir as a mod: 0 and m filled, else -1 and why: no System folder, no
 * packages in it, or a copy of the game (System's DeusEx.exe). */
int  dxl_mod_check(const char *dir, dxl_mod *m, dxl_err *err);
void dxl_mod_free(dxl_mod *m);
/* to a copy of from, its own strings. */
void dxl_mod_copy(dxl_mod *to, const dxl_mod *from);
/* What a mod's where says: "in the game's folder", "in the launcher's mods
 * folder", "elsewhere". */
const char *dxl_mod_where_text(dxl_mod_where where);

#define DXL_MODS_MAX 64

typedef struct {
    dxl_mod items[DXL_MODS_MAX];
    int     count;
} dxl_mod_list;

/* The mods in the game's folder (each of game_root's folders that is one)
 * then in the launcher's mods folder (mods_dir's), each in name order, at
 * most DXL_MODS_MAX; either may be NULL. */
void dxl_mods_find(dxl_mod_list *l, const char *game_root, const char *mods_dir);
/* A copy of m put in l, unless one of its folder is there already or l is
 * full: 0 when it is put in. */
int  dxl_mod_list_add(dxl_mod_list *l, const dxl_mod *m);
void dxl_mod_list_free(dxl_mod_list *l);

/* The folder of the mod's a folder named in one of its ini's paths comes
 * to, absolute (value with backslashes or not, a path's pattern left off):
 * a folder of the mod's as the engine resolves it, a relative one against
 * the game's System folder in any case; or ..\<the mod's name>\<rest>,
 * the mod's ini's path for it in the game's folder, where it is not: the
 * mod's <rest>, when it has it. NULL when it is not the mod's. Caller
 * frees. */
char *dxl_mod_folder(const char *value, const char *game_system_dir, const dxl_mod *m);

/* A mod's profile's paths, made in sys, the ini the profile is made from
 * (the mod's own, or the game's): the mod's folders first, by their
 * absolute paths -- each path to a folder of the mod's (dxl_mod_folder),
 * or with none (no ini, or one written for a folder of another name) the
 * mod's System, Maps, Textures, Sounds and Music --, then the other paths
 * as they are, which the engine resolves against the game's System
 * folder; each once. */
void dxl_mod_set_paths(dxl_ini *sys, const char *game_system_dir, const dxl_mod *m);

#endif
