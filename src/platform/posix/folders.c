#define _GNU_SOURCE
#include "platform/folders.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <stdlib.h>
#include <string.h>

/* $var if it is an absolute path, else <home>/<fallback>: a variable that
 * is empty or not absolute counts as unset, as the spec has it. */
static char *base_dir(const char *var, const char *home, const char *fallback) {
    const char *v = getenv(var);
    if (v && dxl_fs_is_absolute(v)) return dxl_xstrdup(v);
    return dxl_path_join(home, fallback);
}

int dxl_app_folders_get(const char *app, dxl_app_folders *f, dxl_err *err) {
    memset(f, 0, sizeof *f);
    const char *home = dxl_fs_home();
    if (!home) {
        dxl_err_set(err, "no home folder: HOME is not set and the user has none");
        return -1;
    }
    char *config = base_dir("XDG_CONFIG_HOME", home, ".config");
    char *state = base_dir("XDG_STATE_HOME", home, ".local/state");
    char *cache = base_dir("XDG_CACHE_HOME", home, ".cache");
    f->data_home = base_dir("XDG_DATA_HOME", home, ".local/share");
    f->config = dxl_path_join(config, app);
    f->data = dxl_path_join(f->data_home, app);
    f->state = dxl_path_join(state, app);
    f->cache = dxl_path_join(cache, app);
    free(config);
    free(state);
    free(cache);
    return 0;
}

void dxl_app_folders_free(dxl_app_folders *f) {
    if (!f) return;
    free(f->config);
    free(f->data);
    free(f->state);
    free(f->cache);
    free(f->data_home);
    memset(f, 0, sizeof *f);
}
