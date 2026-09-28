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
    ini_file user;      /* User.ini */
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

/* A missing file starts as its default; existed stays 0, so the next save
 * writes it out. */
static void seed(ini_file *f, const char *system_dir, const char *default_name) {
    char *def_path = leaf_path(system_dir, "", default_name);
    f->ini = dxl_ini_load(def_path, NULL);
    free(def_path);
}

dxl_config *dxl_config_open(const char *system_dir, const char *package) {
    dxl_config *c = dxl_xmalloc(sizeof *c);
    memset(c, 0, sizeof *c);

    file_load(&c->base, leaf_path(system_dir, "", package));
    if (!c->base.ini) {
        seed(&c->base, system_dir, "Default");
        c->seeded = c->base.ini != NULL;
    }
    if (!c->base.ini) c->base.ini = dxl_ini_new();

    file_load(&c->user, leaf_path(system_dir, "", "User"));
    if (!c->user.ini) seed(&c->user, system_dir, "DefUser");
    return c;
}

void dxl_config_free(dxl_config *c) {
    if (!c) return;
    file_free(&c->base);
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

static void file_reload(ini_file *f) {
    if (!f->path) return;
    dxl_ini *fresh = dxl_ini_load(f->path, NULL);
    if (!fresh) return;
    dxl_ini_free(f->ini);
    f->ini = fresh;
    f->existed = 1;
}

void dxl_config_reload(dxl_config *c) {
    file_reload(&c->base);
    file_reload(&c->user);
}

int dxl_config_save(dxl_config *c, dxl_err *err) {
    if (file_save(&c->base, err) != 0) return -1;
    if (file_save(&c->user, err) != 0) return -1;
    return 0;
}

static int file_pending(const ini_file *f) {
    return f->ini && (dxl_ini_dirty(f->ini) || !f->existed);
}

int dxl_config_dirty(const dxl_config *c) {
    return file_pending(&c->base) || file_pending(&c->user);
}

const char *dxl_config_path(const dxl_config *c) { return c->base.path; }
dxl_ini *dxl_config_ini(dxl_config *c) { return c->base.ini; }
int dxl_config_seeded(const dxl_config *c) { return c->seeded; }

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
