/* What one start of the launcher works with: its own folder, its folders
 * in the home (core/xdg.h), launcher.ini, the installs it knows, and the
 * profile it plays with and that profile's install.
 *
 *   <config>/launcher.ini   [Launcher] Profile, the active profile's id;
 *                           Engine, another engine's program than the
 *                           launcher's own; [Install.<id>] Path, Version,
 *                           SHA1, each install the launcher has met
 *                           (core/install.h's id); [Window] Width, Height,
 *                           Maximized, the window as it last closed
 *                           (screens/window.h)
 *
 * The launcher's own folder is where its program is (/proc/self/exe): its
 * own engine beside it (DXEngine) and its data/. DXL_ENGINE names the
 * engine over launcher.ini's.
 */
#ifndef DXL_SESSION_H
#define DXL_SESSION_H

#include "core/bindings.h"
#include "core/common.h"
#include "core/ini.h"
#include "core/install.h"
#include "core/mods.h"
#include "core/profile.h"
#include "core/settings.h"
#include "core/xdg.h"

#include <stdatomic.h>

/* The game's ini by its package: DeusEx.ini. */
#define DXL_GAME_PACKAGE "DeusEx"

typedef struct {
    dxl_xdg      xdg;
    char        *app_dir;     /* the launcher's own folder */
    char        *data_dir;    /* <app_dir>/data */
    char        *engine;      /* the engine's program */
    int          engine_by_env;   /* DXL_ENGINE named it */
    char        *ini_path;    /* launcher.ini */
    dxl_ini     *ini;
    dxl_profile  profile;     /* the active one; it may not be made yet */
    dxl_install  install;     /* the profile's, checked; root NULL when it has none */
} dxl_session;

/* profile_id NULL: launcher.ini's, else "default". */
int  dxl_session_open(dxl_session *s, const char *profile_id, dxl_err *err);
void dxl_session_close(dxl_session *s);

/* launcher.ini, and the profile's profile.ini, when changed. */
int  dxl_session_save(dxl_session *s, dxl_err *err);

/* The folder of an install launcher.ini lists, or NULL. */
const char *dxl_session_install_path(const dxl_session *s, const char *id);
/* The folders of every install launcher.ini lists, at most max, in its
 * order; returns how many there are. Valid until launcher.ini changes. */
size_t dxl_session_installs(const dxl_session *s, const char **paths, size_t max);
/* Lists an install in launcher.ini; its id into id (may be NULL). */
void dxl_session_remember_install(dxl_session *s, const dxl_install *in, char *id, size_t size);

/* The profile's install checked again, from launcher.ini's path for it. 0
 * when it is one the engine plays. */
int  dxl_session_check_install(dxl_session *s);

/* The first run: the active profile made from an install, and a mod for a
 * mod's profile (mod NULL for none), named, pointed at them and made active
 * (core/profile.h's import). */
int  dxl_session_make_profile(dxl_session *s, const dxl_install *in, const dxl_mod *mod, const char *name,
                              dxl_import *report, dxl_err *err);
/* Its second half, once the profile's import (dxl_profile_import_watched)
 * made what made holds: profile.ini, launcher.ini. made is kept, or taken
 * away when profile.ini cannot be written, and freed either way. */
int  dxl_session_made_profile(dxl_session *s, const dxl_install *in, const dxl_mod *mod, const char *name,
                              dxl_made *made, const dxl_import *report, dxl_err *err);
/* The launcher's mods folder, <data>/mods (core/mods.h). Caller frees. */
char *dxl_session_mods_dir(const dxl_session *s);
/* The mods to offer: the install's folder's, the launcher's mods folder's,
 * then those of the profiles' Mod that are still mods. */
void dxl_session_mods(const dxl_session *s, const char *game_root, dxl_mod_list *l);
/* The profiles that play the mod of folder dir, sorted, at most max into
 * ids; returns how many. */
size_t dxl_session_mod_profiles(const dxl_session *s, const char *dir, char ids[][DXL_PROFILE_ID_MAX + 1],
                                size_t max);
/* The engine's audit of a mod's natives (core/mods.h, dxl_mod_natives), for
 * an engine whose --version has audit-natives: its words, the session's
 * engine on the mod's System and the game's folder game_root, NULL-ended
 * (core/argv.h's dxl_argv_free); and the audit run with them, in
 * DXL_AUDIT_TIMEOUT_MS at most, given up once *stop is set (NULL: never):
 * 0 with n its answer, else -1, n unknown -- an engine that failed, gave no
 * whole answer or ran out of time marks nothing. */
#define DXL_AUDIT_TIMEOUT_MS 30000
char **dxl_session_audit_argv(const dxl_session *s, const char *game_root, const dxl_mod *m);
int  dxl_session_audit_run(char *const *argv, const atomic_int *stop, dxl_mod_natives *n);
/* And how it begins, as the first run's quick setup has it: a graphics
 * preset, NULL for the engine's defaults as made, and a pad layout, NULL
 * for the controls as imported; its files written. */
int  dxl_session_begin_with(dxl_session *s, const dxl_preset *graphics, const dxl_pad_preset *layout,
                            dxl_err *err);

/* The active profile id from now on, opened, its install checked;
 * launcher.ini's Profile once it is made (one not made yet, the first
 * run's pages make). -1 for an id no profile can have. */
int  dxl_session_use_profile(dxl_session *s, const char *id, dxl_err *err);
/* A new profile, a copy of the settings of profile from (core/profile.h's
 * dxl_profile_copy) named name: its id into id. */
int  dxl_session_copy_profile(dxl_session *s, const char *from, const char *name, char *id, size_t size,
                              dxl_err *err);
/* A profile's name, profile.ini's Name. */
int  dxl_session_rename_profile(dxl_session *s, const char *id, const char *name, dxl_err *err);
/* A profile's settings, data and state folders to the desktop's trash
 * (platform/trash.h), all or none, its cache folder then deleted: never
 * the active profile's. 0 done; -1 not deleted, err why; 1 gone but for
 * a folder that could not be removed whole, err naming it; 2 (POSIX's
 * alone) not deleted, a folder being where no trash can be made: why
 * (may be NULL) gets a line for each, its path and why, for the player
 * to be asked, and with for_good 1 such a folder is deleted for good, the
 * rest to the trash (bugs.md LAPP.26). */
int  dxl_session_delete_profile(dxl_session *s, const char *id, int for_good, char *why, size_t why_size,
                                dxl_err *err);

/* The launcher's own engine, the one beside it. Caller frees. */
char *dxl_session_own_engine(const dxl_session *s);
/* The engine played with from now on, launcher.ini's: path, or NULL for the
 * launcher's own. Under DXL_ENGINE that one is still the one played with. */
void  dxl_session_set_engine(dxl_session *s, const char *path);

/* An id for the single-instance lock: one launcher for a home, a scratch
 * one (DXL_HOME) apart from the real one. Caller frees. */
char *dxl_session_instance_id(const dxl_session *s);

#endif
