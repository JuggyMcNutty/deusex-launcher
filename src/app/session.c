#define _GNU_SOURCE
#include "app/session.h"
#include "core/log.h"
#include "core/paths.h"

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
    const char *engine = getenv("DXL_ENGINE");
    s->engine = engine && *engine ? dxl_xstrdup(engine) : dxl_path_join(s->app_dir, ENGINE_NAME);

    s->ini_path = dxl_path_join(s->xdg.config, "launcher.ini");
    s->ini = dxl_ini_load(s->ini_path, NULL);
    if (!s->ini) {
        s->ini = dxl_ini_new();
        dxl_ini_set_eol(s->ini, "\n");
    }

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

char *dxl_session_instance_id(const dxl_session *s) {
    size_t n = strlen(s->xdg.config) + 16;
    char *id = dxl_xmalloc(n);
    snprintf(id, n, "dxlauncher:%s", s->xdg.config);
    return id;
}
