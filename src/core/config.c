#include "core/config.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One ini file and where it lives. */
typedef struct {
    dxl_ini *ini;
    char    *path;
    int      existed;   /* on disk when opened (or since saved) */
} ini_file;

struct dxl_config {
    ini_file base;      /* <Package>.ini */
    ini_file se;        /* SE-<Package>.ini, when the engine has made one */
    ini_file user;      /* SE-User.ini or User.ini */
    int      seeded;
};

static char *leaf_path(const char *dir, const char *prefix, const char *name) {
    size_t n = strlen(prefix) + strlen(name) + 8;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, "%s%s.ini", prefix, name);
    /* Installs vary in case; prefer whatever is really on disk. */
    char *found = dxl_path_resolve_ci(dir, leaf);
    char *path = found ? found : dxl_path_join(dir, leaf);
    free(leaf);
    return path;
}

static void file_load(ini_file *f, char *path) {
    f->path = path;
    f->ini = dxl_ini_load(path, NULL);
    f->existed = f->ini != NULL;
}

static void file_free(ini_file *f) {
    dxl_ini_free(f->ini);
    free(f->path);
    memset(f, 0, sizeof *f);
}

/* A config the engine can start from names its package search paths. */
static int usable_system_ini(const dxl_ini *ini) {
    return ini && dxl_ini_get(ini, "Core.System", "Paths") != NULL;
}

dxl_config *dxl_config_open(const char *system_dir, const char *package) {
    dxl_config *c = dxl_xmalloc(sizeof *c);
    memset(c, 0, sizeof *c);

    file_load(&c->base, leaf_path(system_dir, "", package));
    if (!usable_system_ini(c->base.ini)) {
        char *def_path = leaf_path(system_dir, "", "Default");
        dxl_ini *def = dxl_ini_load(def_path, NULL);
        free(def_path);
        if (def) {
            /* Keep whatever the partial file said -- FirstRun above all --
             * on top of the full default set. */
            if (c->base.ini) dxl_ini_overlay(def, c->base.ini);
            c->seeded = c->base.ini ? 2 : 1;
            dxl_ini_free(c->base.ini);
            c->base.ini = def;
            c->base.existed = 0;    /* what is on disk is not this; write it */
        }
    }
    if (!c->base.ini) c->base.ini = dxl_ini_new();

    char *se_path = leaf_path(system_dir, "SE-", package);
    if (dxl_path_exists(se_path)) file_load(&c->se, se_path);
    else free(se_path);

    char *user_path = leaf_path(system_dir, "SE-", "User");
    if (dxl_path_exists(user_path)) {
        file_load(&c->user, user_path);
    } else {
        free(user_path);
        file_load(&c->user, leaf_path(system_dir, "", "User"));
        if (!c->user.ini) {
            char *def_path = leaf_path(system_dir, "", "DefUser");
            dxl_ini *def = dxl_ini_load(def_path, NULL);
            free(def_path);
            /* existed stays 0, so the next save writes User.ini out. */
            if (def) c->user.ini = def;
        }
    }
    return c;
}

void dxl_config_free(dxl_config *c) {
    if (!c) return;
    file_free(&c->base);
    file_free(&c->se);
    file_free(&c->user);
    free(c);
}

static int file_save(ini_file *f, dxl_err *err) {
    if (!f->ini || !f->path) return 0;
    if (!dxl_ini_dirty(f->ini) && f->existed) return 0;
    if (dxl_ini_save(f->ini, f->path, err) != 0) return -1;
    f->existed = 1;
    return 0;
}

int dxl_config_save(dxl_config *c, dxl_err *err) {
    if (file_save(&c->base, err) != 0) return -1;
    if (file_save(&c->se, err) != 0) return -1;
    if (file_save(&c->user, err) != 0) return -1;
    return 0;
}

static int file_pending(const ini_file *f) {
    return f->ini && (dxl_ini_dirty(f->ini) || !f->existed);
}

int dxl_config_dirty(const dxl_config *c) {
    return file_pending(&c->base) || file_pending(&c->se) || file_pending(&c->user);
}

const char *dxl_config_path(const dxl_config *c) { return c->base.path; }
dxl_ini *dxl_config_ini(dxl_config *c) { return c->base.ini; }
int dxl_config_seeded(const dxl_config *c) { return c->seeded; }

dxl_ini *dxl_config_client_ini(dxl_config *c) {
    return c->se.ini ? c->se.ini : c->base.ini;
}
const char *dxl_config_client_section(const dxl_config *c) {
    return c->se.ini ? "Engine.SurrealClient" : "WinDrv.WindowsClient";
}
const char *dxl_config_client_path(const dxl_config *c) {
    return c->se.ini ? c->se.path : c->base.path;
}

dxl_ini *dxl_config_user_ini(dxl_config *c) { return c->user.ini; }
const char *dxl_config_user_path(const dxl_config *c) { return c->user.path; }

int dxl_config_first_run(const dxl_config *c) {
    return dxl_ini_get_int(c->base.ini, "FirstRun", "FirstRun", 0);
}

void dxl_config_clamp_first_run(dxl_config *c) {
    if (dxl_config_first_run(c) < DXL_FIRSTRUN_CURRENT)
        dxl_ini_set_int(c->base.ini, "FirstRun", "FirstRun", DXL_FIRSTRUN_CURRENT);
}

const char *dxl_config_render_device(const dxl_config *c) {
    return dxl_ini_get(c->base.ini, "Engine.Engine", "GameRenderDevice");
}

const char *dxl_config_cd_path(const dxl_config *c) {
    return dxl_ini_get(c->base.ini, "Engine.Engine", "CdPath");
}

const char *dxl_config_game_engine(const dxl_config *c) {
    return dxl_ini_get(c->base.ini, "Engine.Engine", "GameEngine");
}

double dxl_config_brightness(dxl_config *c) {
    const char *v = dxl_ini_get(dxl_config_client_ini(c), dxl_config_client_section(c),
                                "Brightness");
    double b = v ? atof(v) : 0.5;   /* USurrealClient's default */
    return b < 0 ? 0 : b > 1 ? 1 : b;
}

void dxl_config_set_brightness(dxl_config *c, double v) {
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    /* The engine's own spelling (IniPropertyConverter<float>): six decimals. */
    char buf[32];
    snprintf(buf, sizeof buf, "%.6f", v);
    const char *have = dxl_ini_get(dxl_config_client_ini(c), dxl_config_client_section(c),
                                   "Brightness");
    if (have && atof(have) == atof(buf)) return;
    dxl_ini_set(dxl_config_client_ini(c), dxl_config_client_section(c), "Brightness", buf);
}

int dxl_config_decals(dxl_config *c) {
    return dxl_ini_get_bool(dxl_config_client_ini(c), dxl_config_client_section(c),
                            "Decals", 1);
}

void dxl_config_set_decals(dxl_config *c, int on) {
    if (dxl_config_decals(c) == (on ? 1 : 0)) return;
    dxl_ini_set_bool(dxl_config_client_ini(c), dxl_config_client_section(c), "Decals", on);
}

int dxl_config_reset_files(const char *system_dir, const char *package, dxl_err *err) {
    char *def = leaf_path(system_dir, "", "Default");
    int have_default = dxl_path_exists(def);
    free(def);
    if (!have_default) {
        dxl_err_set(err, "Default.ini is missing from %s; nothing to rebuild from", system_dir);
        return -1;
    }
    char *paths[4] = {
        leaf_path(system_dir, "SE-", package),
        leaf_path(system_dir, "SE-", "User"),
        leaf_path(system_dir, "", package),
        leaf_path(system_dir, "", "User"),
    };
    int rc = 0;
    for (int i = 0; i < 4; i++) {
        if (dxl_path_exists(paths[i]) && remove(paths[i]) != 0) {
            dxl_err_set(err, "cannot delete %s", paths[i]);
            rc = -1;
        }
        free(paths[i]);
    }
    return rc;
}
