#include "core/sentinel.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dxl_sentinel_init(dxl_sentinel *s, const char *system_dir) {
    memset(s, 0, sizeof *s);
    s->path = dxl_path_join(system_dir, "Running.ini");
}

void dxl_sentinel_free(dxl_sentinel *s) {
    if (!s) return;
    free(s->path);
    s->path = NULL;
}

const char *dxl_sentinel_path(const dxl_sentinel *s) { return s->path; }

int dxl_sentinel_exists(const dxl_sentinel *s) {
    if (dxl_path_exists(s->path)) return 1;
    /* The original wrote this on a case-insensitive filesystem; a copied
     * install on ext4 may carry any case. Missing a stale sentinel would
     * silently disable crash detection. */
    char *dir = dxl_path_dirname(s->path);
    char *hit = dxl_path_resolve_ci(dir, "Running.ini");
    free(dir);
    if (hit) { free(hit); return 1; }
    return 0;
}

int dxl_sentinel_create(dxl_sentinel *s, dxl_err *err) {
    FILE *f = fopen(s->path, "wb");
    if (!f) {
        dxl_err_set(err, "cannot create %s", s->path);
        return -1;
    }
    /* The original writes an empty file; its existence is the whole signal.
     * A comment costs nothing and makes a stray copy self-explanatory. */
    fputs("; Deus Ex is running. Deleted on clean exit.\r\n", f);
    fclose(f);
    s->created = 1;
    return 0;
}

void dxl_sentinel_remove(dxl_sentinel *s) {
    if (!s || !s->path) return;
    if (remove(s->path) != 0) {
        char *dir = dxl_path_dirname(s->path);
        char *hit = dxl_path_resolve_ci(dir, "Running.ini");
        free(dir);
        if (hit) { remove(hit); free(hit); }
    }
    s->created = 0;
}
