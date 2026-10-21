#define _GNU_SOURCE
#include "core/xdg.h"
#include "core/paths.h"
#include "platform/folders.h"
#include "platform/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dxl_xdg_open(dxl_xdg *x, dxl_err *err) {
    memset(x, 0, sizeof *x);
    const char *scratch = getenv("DXL_HOME");
    if (scratch && dxl_fs_is_absolute(scratch)) {
        char *config = dxl_path_join(scratch, "config");
        char *state = dxl_path_join(scratch, "state");
        char *cache = dxl_path_join(scratch, "cache");
        x->data_home = dxl_path_join(scratch, "data");
        x->config = dxl_path_join(config, DXL_APP_DIR);
        x->data = dxl_path_join(x->data_home, DXL_APP_DIR);
        x->state = dxl_path_join(state, DXL_APP_DIR);
        x->cache = dxl_path_join(cache, DXL_APP_DIR);
        free(config);
        free(state);
        free(cache);
        return 0;
    }
    dxl_app_folders f;
    if (dxl_app_folders_get(DXL_APP_DIR, &f, err) != 0) return -1;
    x->config = f.config;
    x->data = f.data;
    x->state = f.state;
    x->cache = f.cache;
    x->data_home = f.data_home;
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
