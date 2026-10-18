#define _GNU_SOURCE
#include "core/shotlist.h"
#include "core/paths.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int dxl_shot_name_ok(const char *name) {
    size_t n = strlen(name);
    if (n < 5 || name[0] == '.') return 0;
    return !dxl_stricmp(name + n - 4, ".bmp") || !dxl_stricmp(name + n - 4, ".png");
}

/* Newest first; of one time, the later name first (Shot0012, Shot0011). */
static int newest_first(const void *a, const void *b) {
    const dxl_shot *x = a, *y = b;
    if (x->mtime != y->mtime) return x->mtime > y->mtime ? -1 : 1;
    return -strcmp(x->name, y->name);
}

int dxl_shots_list(const char *dir, dxl_shots *out, dxl_err *err) {
    memset(out, 0, sizeof *out);
    DIR *d = dir ? opendir(dir) : NULL;
    if (!d) {
        if (!dir || errno == ENOENT || errno == ENOTDIR) return 0;
        dxl_err_set(err, "cannot read %s: %s", dir, strerror(errno));
        return -1;
    }
    int cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!dxl_shot_name_ok(e->d_name) || strlen(e->d_name) >= sizeof out->items[0].name) continue;
        char *path = dxl_path_join(dir, e->d_name);
        long size = dxl_path_size(path);
        if (dxl_path_is_dir(path) || size <= 0) {
            free(path);
            continue;
        }
        if (out->count == cap) {
            cap = cap ? cap * 2 : 32;
            out->items = dxl_xrealloc(out->items, (size_t)cap * sizeof *out->items);
        }
        dxl_shot *s = &out->items[out->count++];
        s->path = path;
        snprintf(s->name, sizeof s->name, "%s", e->d_name);
        s->mtime = dxl_path_mtime(path);
        s->size = size;
    }
    closedir(d);
    if (out->count > 1) qsort(out->items, (size_t)out->count, sizeof *out->items, newest_first);
    return 0;
}

void dxl_shots_free(dxl_shots *s) {
    for (int i = 0; i < s->count; i++) free(s->items[i].path);
    free(s->items);
    memset(s, 0, sizeof *s);
}

void dxl_shot_when(const dxl_shot *s, char *out, size_t size) {
    time_t t = (time_t)s->mtime;
    struct tm tm;
    out[0] = '\0';
    if (s->mtime < 0 || !localtime_r(&t, &tm) || !strftime(out, size, "%e %B %Y, %H:%M", &tm)) return;
    if (out[0] == ' ') memmove(out, out + 1, strlen(out));
}
