#define _GNU_SOURCE
#include "core/xdg.h"
#include "core/paths.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    if (h && h[0] == '/') return h;
    struct passwd *pw = getpwuid(getuid());
    return pw && pw->pw_dir && pw->pw_dir[0] == '/' ? pw->pw_dir : NULL;
}

/* $var if it is an absolute path, else <home>/<fallback>. */
static char *base_dir(const char *var, const char *home, const char *fallback) {
    const char *v = getenv(var);
    if (v && v[0] == '/') return dxl_xstrdup(v);
    return dxl_path_join(home, fallback);
}

int dxl_xdg_open(dxl_xdg *x, dxl_err *err) {
    memset(x, 0, sizeof *x);
    const char *scratch = getenv("DXL_HOME");
    char *config, *data, *state, *cache;
    if (scratch && scratch[0] == '/') {
        config = dxl_path_join(scratch, "config");
        data = dxl_path_join(scratch, "data");
        state = dxl_path_join(scratch, "state");
        cache = dxl_path_join(scratch, "cache");
    } else {
        const char *home = home_dir();
        if (!home) {
            dxl_err_set(err, "no home folder: HOME is not set and the user has none");
            return -1;
        }
        config = base_dir("XDG_CONFIG_HOME", home, ".config");
        data = base_dir("XDG_DATA_HOME", home, ".local/share");
        state = base_dir("XDG_STATE_HOME", home, ".local/state");
        cache = base_dir("XDG_CACHE_HOME", home, ".cache");
    }
    x->config = dxl_path_join(config, DXL_APP_DIR);
    x->data = dxl_path_join(data, DXL_APP_DIR);
    x->state = dxl_path_join(state, DXL_APP_DIR);
    x->cache = dxl_path_join(cache, DXL_APP_DIR);
    x->data_home = data;
    free(config);
    free(state);
    free(cache);
    return 0;
}

void dxl_xdg_free(dxl_xdg *x) {
    if (!x) return;
    free(x->config);
    free(x->data);
    free(x->state);
    free(x->cache);
    free(x->data_home);
    memset(x, 0, sizeof *x);
}

int dxl_xdg_make(const dxl_xdg *x, dxl_err *err) {
    const char *dirs[] = { x->config, x->data, x->state, x->cache };
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        if (dxl_path_mkdirs(dirs[i]) != 0) {
            dxl_err_set(err, "cannot create %s", dirs[i]);
            return -1;
        }
    }
    return 0;
}
