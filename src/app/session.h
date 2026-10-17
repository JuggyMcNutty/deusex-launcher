/* What one start of the launcher works with: its own folder, its folders
 * in the home (core/xdg.h), launcher.ini, the installs it knows, and the
 * profile it plays with and that profile's install.
 *
 *   <config>/launcher.ini   [Launcher] Profile, the active profile's id;
 *                           [Install.<id>] Path, Version, SHA1, each install
 *                           the launcher has met (core/install.h's id)
 *
 * The launcher's own folder is where its program is (/proc/self/exe): the
 * engine beside it (SurrealEngine; DXL_ENGINE names another) and its data/.
 */
#ifndef DXL_SESSION_H
#define DXL_SESSION_H

#include "core/common.h"
#include "core/ini.h"
#include "core/install.h"
#include "core/profile.h"
#include "core/xdg.h"

typedef struct {
    dxl_xdg      xdg;
    char        *app_dir;     /* the launcher's own folder */
    char        *data_dir;    /* <app_dir>/data */
    char        *engine;      /* the engine's program */
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
/* Lists an install in launcher.ini; its id into id (may be NULL). */
void dxl_session_remember_install(dxl_session *s, const dxl_install *in, char *id, size_t size);

/* The profile's install checked again, from launcher.ini's path for it. 0
 * when it is one the engine plays. */
int  dxl_session_check_install(dxl_session *s);

/* The first run: the active profile made from an install, named, pointed
 * at it and made active (core/profile.h's import). */
int  dxl_session_make_profile(dxl_session *s, const dxl_install *in, const char *name,
                              dxl_import *report, dxl_err *err);

/* An id for the single-instance lock: one launcher for a home, a scratch
 * one (DXL_HOME) apart from the real one. Caller frees. */
char *dxl_session_instance_id(const dxl_session *s);

#endif
