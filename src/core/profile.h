/* A profile: one way to play -- the game as it is, later a mod -- with its
 * own inis, engine settings, saves and shots, all in the launcher's folders
 * (core/xdg.h), none in the game's:
 *
 *   <config>/profiles/<id>/profile.ini   its name and its install
 *   <config>/profiles/<id>/DeusEx.ini    the engine's --ini
 *   <config>/profiles/<id>/User.ini      --userini
 *   <config>/profiles/<id>/engine/       --userdir: Settings.json, a class's
 *                                        own config files, the engine's log
 *   <data>/profiles/<id>/Save/           SavePath
 *   <data>/profiles/<id>/Screenshots/    the shots (engine/Screenshots links here)
 *   <state>/profiles/<id>/               Running.ini, LastRun.ini, logs/
 *   <cache>/profiles/<id>/Cache/         CachePath: a net game's downloads
 *
 * The engine resolves [Core.System]'s Paths against the game's System
 * folder, so they stay as the game has them; SavePath and CachePath are
 * made absolute, so saves and downloads land here. The inis are written LF,
 * as the engine writes them back.
 */
#ifndef DXL_PROFILE_H
#define DXL_PROFILE_H

#include "core/common.h"
#include "core/ini.h"
#include "core/xdg.h"

#define DXL_PROFILE_SECTION "Profile"
#define DXL_PROFILE_DEFAULT "default"

typedef struct {
    char    *id;
    char    *dir;          /* <config>/profiles/<id> */
    char    *ini_path;     /* profile.ini */
    dxl_ini *ini;          /* profile.ini, or an empty one */
    char    *system_ini;   /* DeusEx.ini */
    char    *user_ini;     /* User.ini */
    char    *engine_dir;   /* the engine's --userdir */
    char    *save_dir;
    char    *shots_dir;
    char    *state_dir;    /* the crash sentinel and the last run */
    char    *logs_dir;
    char    *cache_dir;
} dxl_profile;

/* The longest id. */
#define DXL_PROFILE_ID_MAX 64

/* The profile's paths, and its profile.ini when there is one. Creates
 * nothing. An id is letters, digits, '-' and '_': -1 for any other. */
int  dxl_profile_open(dxl_profile *p, const dxl_xdg *x, const char *id, dxl_err *err);
void dxl_profile_free(dxl_profile *p);

/* Made already: profile.ini and DeusEx.ini are there. */
int  dxl_profile_exists(const dxl_profile *p);

/* Its folders, and engine/Screenshots linked to the shots folder. */
int  dxl_profile_make_dirs(const dxl_profile *p, dxl_err *err);

/* Its saves: the save folder's SaveNNNN folders and QuickSave. */
int  dxl_profile_count_saves(const dxl_profile *p);

/* Its four folders, each the profile's alone: <config>/profiles/<id>,
 * <data>/profiles/<id>, <state>/profiles/<id>, <cache>/profiles/<id>.
 * Caller frees each. */
void dxl_profile_folders(const dxl_profile *p, char *out[4]);

/* The ids of the profiles made in the launcher's folders, sorted, at most
 * max into ids; returns how many there are. */
size_t dxl_profiles_list(const dxl_xdg *x, char ids[][DXL_PROFILE_ID_MAX + 1], size_t max);

/* A new profile's id from its name: its letters and digits, lower case, a
 * '-' for each run of anything else ("Deus Ex 2" deus-ex-2), "profile" for
 * none; "-2", "-3"... after it when a profile's folder has it already. */
void dxl_profile_new_id(const dxl_xdg *x, const char *name, char *out, size_t size);

/* to made a copy of from's settings -- profile.ini, the two inis with to's
 * own SavePath and CachePath, the engine folder's Settings.json and a
 * class's own config files -- not its saves, shots, runs or logs. to must
 * not be made yet. */
int  dxl_profile_copy(const dxl_profile *from, const dxl_profile *to, dxl_err *err);

/* profile.ini's [Profile] keys: Name, Install (an id of launcher.ini's
 * installs). */
const char *dxl_profile_get(const dxl_profile *p, const char *key);
void        dxl_profile_set(dxl_profile *p, const char *key, const char *value);
int         dxl_profile_save(dxl_profile *p, dxl_err *err);

/* What a profile is made of, from an install: what the first run shows
 * before it runs, and what it did. */
typedef struct {
    char system_from[512];   /* the file DeusEx.ini is made from */
    char user_from[512];     /* User.ini's */
    char engine_from[512];   /* where the engine's own sections came from; "" for none */
    char saves_from[512];    /* the game's save folder */
    int  saves;              /* save folders (SaveNNNN, QuickSave) to copy, or copied */
    int  shots;              /* System/Shot*.bmp */
    int  master_replaced;    /* GameSpy's dead master server given 333networks' */
    int  bindings_folded;    /* [Extension.InputExt]'s keys, the original's, put in [Engine.Input] */
} dxl_import;

/* Makes the profile's inis from the files of the game's System folder, and
 * copies its saves (never Current) and shots; with dry set, only fills r.
 * Nothing is written to the game's folder. */
int dxl_profile_import(dxl_profile *p, const char *game_system_dir, int dry, dxl_import *r,
                       dxl_err *err);
/* The User.ini the import makes from the file at path (dxl_import's
 * user_from), in memory: the bindings the original saved folded in as the
 * import folds them. NULL when it cannot be read. */
dxl_ini *dxl_profile_user_preview(const char *path);

/* The settings a profile has before any is changed, as the import makes
 * them of the install's defaults: Default.ini, GameSpy's master given
 * 333networks', the engine's sections seeded from its own (core/
 * engine_sections.h); and DefUser.ini. NULL for a file the install lacks. */
void dxl_profile_defaults(const char *game_system_dir, dxl_ini **system, dxl_ini **user);

#endif
