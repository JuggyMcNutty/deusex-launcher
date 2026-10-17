#define _GNU_SOURCE
#include "app/session.h"
#include "core/config.h"
#include "core/engine_settings.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/trash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAUNCHER "Launcher"
#define ENGINE_NAME "SurrealEngine"

int dxl_session_open(dxl_session *s, const char *profile_id, dxl_err *err) {
    memset(s, 0, sizeof *s);
    if (dxl_xdg_open(&s->xdg, err) != 0) return -1;

    char *self = dxl_path_self();
    s->app_dir = self ? dxl_path_dirname(self) : dxl_xstrdup(".");
    free(self);
    s->data_dir = dxl_path_join(s->app_dir, "data");

    s->ini_path = dxl_path_join(s->xdg.config, "launcher.ini");
    s->ini = dxl_ini_load(s->ini_path, NULL);
    if (!s->ini) {
        s->ini = dxl_ini_new();
        dxl_ini_set_eol(s->ini, "\n");
    }
    /* DXL_ENGINE's, launcher.ini's, or the launcher's own. */
    const char *engine = getenv("DXL_ENGINE"), *chosen = dxl_ini_get(s->ini, LAUNCHER, "Engine");
    s->engine_by_env = engine && *engine;
    if (s->engine_by_env) s->engine = dxl_xstrdup(engine);
    else if (chosen && *chosen) s->engine = dxl_xstrdup(chosen);
    else s->engine = dxl_session_own_engine(s);

    const char *id = profile_id;
    if (!id || !*id) id = dxl_ini_get(s->ini, LAUNCHER, "Profile");
    if (!id || !*id) id = DXL_PROFILE_DEFAULT;
    if (dxl_profile_open(&s->profile, &s->xdg, id, err) != 0) return -1;
    dxl_ini_set(s->ini, LAUNCHER, "Profile", s->profile.id);
    return 0;
}

void dxl_session_close(dxl_session *s) {
    if (!s) return;
    dxl_install_free(&s->install);
    dxl_profile_free(&s->profile);
    dxl_ini_free(s->ini);
    dxl_xdg_free(&s->xdg);
    free(s->app_dir); free(s->data_dir); free(s->engine); free(s->ini_path);
    memset(s, 0, sizeof *s);
}

int dxl_session_save(dxl_session *s, dxl_err *err) {
    if (dxl_ini_dirty(s->ini) || !dxl_path_exists(s->ini_path)) {
        if (dxl_path_mkdirs(s->xdg.config) != 0) {
            dxl_err_set(err, "cannot create %s", s->xdg.config);
            return -1;
        }
        if (dxl_ini_save(s->ini, s->ini_path, err) != 0) return -1;
    }
    if (dxl_profile_exists(&s->profile) && dxl_ini_dirty(s->profile.ini))
        return dxl_profile_save(&s->profile, err);
    return 0;
}

static void install_section(const char *id, char *out, size_t size) {
    snprintf(out, size, "Install.%s", id);
}

const char *dxl_session_install_path(const dxl_session *s, const char *id) {
    if (!id || !*id) return NULL;
    char section[128];
    install_section(id, section, sizeof section);
    return dxl_ini_get(s->ini, section, "Path");
}

size_t dxl_session_installs(const dxl_session *s, const char **paths, size_t max) {
    const char *sections[64];
    size_t n = dxl_ini_sections(s->ini, "Install.", sections, 64), k = 0;
    for (size_t i = 0; i < n && i < 64; i++) {
        const char *path = dxl_ini_get(s->ini, sections[i], "Path");
        if (!path || !*path) continue;
        if (paths && k < max) paths[k] = path;
        k++;
    }
    return k;
}

void dxl_session_remember_install(dxl_session *s, const dxl_install *in, char *id, size_t size) {
    char mine[96];
    dxl_install_id(in, mine, sizeof mine);
    char section[128];
    install_section(mine, section, sizeof section);
    dxl_ini_set(s->ini, section, "Path", in->root);
    dxl_ini_set(s->ini, section, "Version", in->build ? in->build->version : "unknown");
    dxl_ini_set(s->ini, section, "SHA1", in->sha1);
    if (id) snprintf(id, size, "%s", mine);
}

int dxl_session_check_install(dxl_session *s) {
    dxl_install_free(&s->install);
    const char *path = dxl_session_install_path(s, dxl_profile_get(&s->profile, "Install"));
    if (!path) return -1;
    return dxl_install_check(path, &s->install);
}

int dxl_session_make_profile(dxl_session *s, const dxl_install *in, const char *name,
                             dxl_import *report, dxl_err *err) {
    if (dxl_profile_import(&s->profile, in->system_dir, 0, report, err) != 0) return -1;
    char id[96];
    dxl_session_remember_install(s, in, id, sizeof id);
    dxl_profile_set(&s->profile, "Name", name && *name ? name : "Deus Ex");
    dxl_profile_set(&s->profile, "Install", id);
    if (dxl_profile_save(&s->profile, err) != 0) return -1;
    dxl_ini_set(s->ini, LAUNCHER, "Profile", s->profile.id);
    dxl_log("profile %s made from %s (DeusEx.ini from %s, User.ini from %s, %d saves, %d shots)",
            s->profile.id, in->root, report->system_from, report->user_from, report->saves,
            report->shots);
    dxl_session_check_install(s);   /* what the screens say of it; the profile is made either way */
    return 0;
}

int dxl_session_begin_with(dxl_session *s, const dxl_preset *graphics, const dxl_pad_preset *layout,
                           dxl_err *err) {
    if (!graphics && !layout) return 0;
    dxl_config *c = dxl_config_open_files(s->install.system_dir ? s->install.system_dir : ".", DXL_GAME_PACKAGE,
                                          s->profile.system_ini, s->profile.user_ini);
    char *json = dxl_path_join(s->profile.engine_dir, "Settings.json");
    char *def = dxl_path_join(s->data_dir, "engine-settings.json.default");
    dxl_stores st;
    memset(&st, 0, sizeof st);
    st.system = dxl_config_ini(c);
    st.user = dxl_config_user_ini(c);
    st.launcher = s->ini;
    st.profile = s->profile.ini;
    st.engine = dxl_es_open(json, def);
    if (graphics) dxl_preset_apply(&st, graphics);
    if (layout && st.user) dxl_bindings_apply(st.user, layout);
    int rc = dxl_config_save(c, err);
    if (rc == 0 && st.engine && dxl_es_dirty(st.engine)) rc = dxl_es_save(st.engine, err);
    dxl_es_free(st.engine);
    dxl_config_free(c);
    free(json);
    free(def);
    if (rc == 0)
        dxl_log("profile %s begun with %s graphics and %s", s->profile.id, graphics ? graphics->label : "the engine's",
                layout ? layout->label : "the controls as imported");
    return rc;
}

int dxl_session_use_profile(dxl_session *s, const char *id, dxl_err *err) {
    dxl_profile next;
    if (dxl_profile_open(&next, &s->xdg, id, err) != 0) return -1;
    /* launcher.ini names a made one first: when it cannot, nothing changes. */
    if (dxl_profile_exists(&next)) {
        char was[DXL_PROFILE_ID_MAX + 1];
        const char *old = dxl_ini_get(s->ini, LAUNCHER, "Profile");
        snprintf(was, sizeof was, "%s", old ? old : "");
        dxl_ini_set(s->ini, LAUNCHER, "Profile", next.id);
        if (dxl_session_save(s, err) != 0) {
            if (was[0]) dxl_ini_set(s->ini, LAUNCHER, "Profile", was);
            dxl_profile_free(&next);
            return -1;
        }
    }
    dxl_profile_free(&s->profile);
    s->profile = next;
    dxl_session_check_install(s);
    dxl_log("profile %s: in use%s", s->profile.id, dxl_profile_exists(&s->profile) ? "" : ", to be made");
    return 0;
}

int dxl_session_copy_profile(dxl_session *s, const char *from, const char *name, char *id, size_t size,
                             dxl_err *err) {
    dxl_profile src, to;
    if (dxl_profile_open(&src, &s->xdg, from, err) != 0) return -1;
    dxl_profile_new_id(&s->xdg, name, id, size);
    if (dxl_profile_open(&to, &s->xdg, id, err) != 0) {
        dxl_profile_free(&src);
        return -1;
    }
    int rc = dxl_profile_copy(&src, &to, err);
    dxl_profile_free(&to);
    dxl_profile_free(&src);
    if (rc == 0) rc = dxl_session_rename_profile(s, id, name, err);
    if (rc == 0) dxl_log("profile %s: a copy of %s's settings", id, from);
    return rc;
}

int dxl_session_rename_profile(dxl_session *s, const char *id, const char *name, dxl_err *err) {
    if (!strcmp(id, s->profile.id)) {
        dxl_profile_set(&s->profile, "Name", name);
        return dxl_profile_save(&s->profile, err);
    }
    dxl_profile p;
    if (dxl_profile_open(&p, &s->xdg, id, err) != 0) return -1;
    dxl_profile_set(&p, "Name", name);
    int rc = dxl_profile_save(&p, err);
    dxl_profile_free(&p);
    return rc;
}

int dxl_session_delete_profile(dxl_session *s, const char *id, dxl_err *err) {
    if (!strcmp(id, s->profile.id)) {
        dxl_err_set(err, "%s is the profile in use", id);
        return -1;
    }
    dxl_profile p;
    if (dxl_profile_open(&p, &s->xdg, id, err) != 0) return -1;
    char *dirs[4];
    dxl_profile_folders(&p, dirs);
    dxl_profile_free(&p);
    int rc = 0, gone = 0;
    char names[4][512];
    /* All of them, or none: each checked before any goes, and those gone
     * put back when a later one cannot go. */
    for (int i = 0; i < 4 && rc == 0; i++)
        if (dxl_path_exists(dirs[i]) && !dxl_trash_can(dirs[i], s->xdg.data_home)) {
            dxl_err_set(err, "%s is not on the trash's filesystem: nothing was deleted", dirs[i]);
            rc = -1;
        }
    for (; gone < 4 && rc == 0; gone++) {
        names[gone][0] = '\0';
        if (dxl_path_exists(dirs[gone]))
            rc = dxl_trash(dirs[gone], s->xdg.data_home, names[gone], sizeof names[gone], err);
    }
    if (rc != 0) {
        for (int i = gone - 2; i >= 0; i--)
            if (names[i][0] && dxl_trash_undo(s->xdg.data_home, names[i], dirs[i], NULL) != 0)
                dxl_log("warning: %s stays in the trash as %s", dirs[i], names[i]);
    } else {
        dxl_log("profile %s: to the trash", id);
    }
    for (int i = 0; i < 4; i++) free(dirs[i]);
    return rc;
}

char *dxl_session_own_engine(const dxl_session *s) { return dxl_path_join(s->app_dir, ENGINE_NAME); }

void dxl_session_set_engine(dxl_session *s, const char *path) {
    char *own = dxl_session_own_engine(s);
    int theirs = path && *path && strcmp(path, own) != 0;
    if (theirs) dxl_ini_set(s->ini, LAUNCHER, "Engine", path);
    else dxl_ini_remove(s->ini, LAUNCHER, "Engine");
    if (!s->engine_by_env) {
        free(s->engine);
        if (theirs) {
            s->engine = dxl_xstrdup(path);
        } else {
            s->engine = own;
            own = NULL;
        }
    }
    free(own);
    dxl_log("engine: %s", theirs ? path : "the launcher's own");
}

char *dxl_session_instance_id(const dxl_session *s) {
    size_t n = strlen(s->xdg.config) + 16;
    char *id = dxl_xmalloc(n);
    snprintf(id, n, "dxlauncher:%s", s->xdg.config);
    return id;
}
