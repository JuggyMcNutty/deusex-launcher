#include "core/safemode.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dxl_safe_options dxl_safe_defaults(void) {
    dxl_safe_options o = {
        .no_sound = 1, .no_3d_sound = 1, .no_3d_video = 1, .window = 1,
        .res = 1, .reset_config = 0, .no_processor = 1, .no_joy = 1,
    };
    return o;
}

char *dxl_safe_flags(const dxl_safe_options *o) {
    dxl_buf b;
    dxl_buf_init(&b);
    dxl_buf_add(&b, "", 0);
    if (o->no_sound)     dxl_buf_puts(&b, " -nosound");
    if (o->no_3d_sound)  dxl_buf_puts(&b, " -no3dsound");
    if (o->no_3d_video)  dxl_buf_puts(&b, " -nohard");
    if (o->window)       dxl_buf_puts(&b, " -nohard -noddraw");
    if (o->res)          dxl_buf_puts(&b, " -defaultres");
    if (o->no_processor) dxl_buf_puts(&b, " -nommx -nokni -nok6");
    if (o->no_joy)       dxl_buf_puts(&b, " -nojoy");
    return b.data;
}

int dxl_safe_reset_config(const char *system_dir, const char *package, dxl_err *err) {
    size_t n = strlen(package) + 5;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, "%s.ini", package);
    char *path = dxl_path_resolve_ci(system_dir, leaf);
    free(leaf);
    if (!path) return 0;
    int rc = remove(path) == 0 ? 0 : -1;
    if (rc) dxl_err_set(err, "cannot delete %s", path);
    free(path);
    return rc;
}
