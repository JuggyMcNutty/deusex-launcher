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
 * own engine beside it (SurrealEngine) and its data/. DXL_ENGINE names the
 * engine over launcher.ini's.
 */
#ifndef DXL_SESSION_H
#define DXL_SESSION_H

#include "core/bindings.h"
#include "core/common.h"
#include "core/ini.h"
#include "core/install.h"
#include "core/profile.h"
#include "core/settings.h"
#include "core/xdg.h"

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

/* The first run: the active profile made from an install, named, pointed
 * at it and made active (core/profile.h's import). */
int  dxl_session_make_profile(dxl_session *s, const dxl_install *in, const char *name,
                              dxl_import *report, dxl_err *err);
/* And how it begins, as the first run's quick setup has it: a graphics
 * preset, NULL for the engine's defaults as made, and a pad layout, NULL
 * for the controls as imported; its files written. */
int  dxl_session_begin_with(dxl_session *s, const dxl_preset *graphics, const dxl_pad_preset *layout,
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
