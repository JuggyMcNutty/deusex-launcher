#define _GNU_SOURCE
#include "core/saves.h"
#include "core/paths.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int dxl_save_slot_of(const char *folder, int *slot) {
    if (!strcmp(folder, "QuickSave")) {
        *slot = DXL_SAVE_QUICK;
        return 1;
    }
    if (strncmp(folder, "Save", 4) != 0 || folder[4] < '0' || folder[4] > '9') return 0;
    char *end;
    errno = 0;
    long n = strtol(folder + 4, &end, 10);
    if (*end || errno || n < 0 || n > 999999) return 0;
    /* Only the engine's own name for it: Save0010, not Save010. */
    char again[24];
    snprintf(again, sizeof again, "Save%04ld", n);
    if (strcmp(again, folder) != 0) return 0;
    *slot = (int)n;
    return 1;
}

void dxl_save_url(int slot, char *out, size_t size) { snprintf(out, size, "?loadgame=%d", slot); }

char *dxl_save_path(const char *dir, const dxl_save *s) { return dxl_path_join(dir, s->folder); }

static long long made(const dxl_save *s) {
    const dxl_saveinfo *i = &s->info;
    return ((((((long long)i->year * 12 + i->month) * 31 + i->day) * 24 + i->hour) * 60 + i->minute) * 60) + i->second;
}

/* Newest first; those that read before those that do not, which go by
 * their folders' names. */
static int newest_first(const void *a, const void *b) {
    const dxl_save *x = a, *y = b;
    if (x->readable != y->readable) return x->readable ? -1 : 1;
    if (!x->readable) return strcmp(x->folder, y->folder);
    long long mx = made(x), my = made(y);
    if (mx != my) return mx > my ? -1 : 1;
    return x->slot > y->slot ? -1 : x->slot < y->slot ? 1 : 0;
}

int dxl_saves_list(const char *dir, dxl_saves *out, dxl_err *err) {
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
        int slot;
        if (!dxl_save_slot_of(e->d_name, &slot)) continue;
        char *folder = dxl_path_join(dir, e->d_name);
        int is_dir = dxl_path_is_dir(folder);
        char *info = is_dir ? dxl_path_resolve_ci(folder, "SaveInfo.dxs") : NULL;
        free(folder);
        if (!is_dir) continue;
        if (out->count == cap) {
            cap = cap ? cap * 2 : 16;
            out->items = dxl_xrealloc(out->items, (size_t)cap * sizeof *out->items);
        }
        dxl_save *s = &out->items[out->count++];
        memset(s, 0, sizeof *s);
        s->slot = slot;
        snprintf(s->folder, sizeof s->folder, "%.23s", e->d_name);
        dxl_err e2;
        s->readable = info && dxl_saveinfo_read(info, 0, &s->info, &e2) == 0;
        if (!s->readable) memset(&s->info, 0, sizeof s->info);
        free(info);
    }
    closedir(d);
    if (out->count > 1) qsort(out->items, (size_t)out->count, sizeof *out->items, newest_first);
    return 0;
}

void dxl_saves_free(dxl_saves *s) {
    for (int i = 0; i < s->count; i++) dxl_saveinfo_free(&s->items[i].info);
    free(s->items);
    memset(s, 0, sizeof *s);
}

const dxl_save *dxl_saves_latest(const dxl_saves *s) {
    return s->count && s->items[0].readable ? &s->items[0] : NULL;
}

void dxl_save_when(const dxl_save *s, char *out, size_t size) {
    const dxl_saveinfo *i = &s->info;
    out[0] = '\0';
    if (!s->readable || i->year <= 0 || i->month < 1 || i->month > 12 || i->day < 1 || i->day > 31) return;
    struct tm tm;
    memset(&tm, 0, sizeof tm);
    tm.tm_year = i->year - 1900;
    tm.tm_mon = i->month - 1;
    tm.tm_mday = i->day;
    tm.tm_hour = i->hour;
    tm.tm_min = i->minute;
    tm.tm_sec = i->second;
    if (!strftime(out, size, "%e %B %Y, %H:%M", &tm)) out[0] = '\0';
    if (out[0] == ' ') memmove(out, out + 1, strlen(out));
}

void dxl_save_played(const dxl_save *s, char *out, size_t size) {
    int t = s->info.play_seconds > 0 ? s->info.play_seconds : 0;
    int h = t / 3600, m = t % 3600 / 60;
    if (t < 60) snprintf(out, size, "under a minute");
    else if (!h) snprintf(out, size, "%d min", m);
    else snprintf(out, size, "%d h %02d min", h, m);
}
