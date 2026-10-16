#include "core/renderdev.h"
#include "core/paths.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One field of a registry entry, "(Name=...,Class=...,...)": the value after
 * "<key>=", unquoted, up to the next comma or closing parenthesis outside
 * quotes. NULL when the entry has no such field. Caller frees. */
static char *field(const char *entry, const char *key) {
    size_t kn = strlen(key);
    const char *p = entry;
    if (*p == '(') p++;
    while (*p) {
        const char *start = p;
        while (*p == ' ' || *p == '\t') p++;
        if (dxl_strnicmp(p, key, kn) == 0 && p[kn] == '=') {
            p += kn + 1;
            dxl_buf v;
            dxl_buf_init(&v);
            dxl_buf_add(&v, "", 0);
            int quoted = 0;
            for (; *p; p++) {
                if (*p == '"') { quoted = !quoted; continue; }
                if (!quoted && (*p == ',' || *p == ')')) break;
                dxl_buf_add(&v, p, 1);
            }
            return v.data;
        }
        /* Skip to the next field. */
        int quoted = 0;
        for (p = start; *p; p++) {
            if (*p == '"') quoted = !quoted;
            else if (!quoted && *p == ',') { p++; break; }
            else if (!quoted && *p == ')') return NULL;
        }
    }
    return NULL;
}

static int by_name(const void *a, const void *b) {
    return dxl_stricmp(*(char *const *)a, *(char *const *)b);
}

/* The .int files in name order, as FindFiles lists them on NTFS. */
static char **int_files(const char *system_dir, size_t *count) {
    char **names = NULL;
    size_t n = 0, cap = 0;
    DIR *d = opendir(system_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t len = strlen(e->d_name);
            if (len < 5 || dxl_stricmp(e->d_name + len - 4, ".int") != 0) continue;
            if (n == cap) {
                cap = cap ? cap * 2 : 32;
                names = dxl_xrealloc(names, cap * sizeof *names);
            }
            names[n++] = dxl_xstrdup(e->d_name);
        }
        closedir(d);
    }
    if (n) qsort(names, n, sizeof *names, by_name);
    *count = n;
    return names;
}

static int autodetected(const char *file, const char *const *dirs) {
    if (!file || !*file || !dirs) return 0;
    for (; *dirs; dirs++) {
        char *hit = dxl_path_resolve_ci(*dirs, file);
        if (hit) { free(hit); return 1; }
    }
    return 0;
}

static void add(dxl_renderdev_list *l, size_t *cap, const char *path, const char *autodetect,
                dxl_loc *loc, const dxl_ini *game_ini, const char *const *autodetect_dirs) {
    const char *dot = strchr(path, '.');
    if (!dot) return;                   /* the original skips a path it cannot split */
    if (l->count == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        l->items = dxl_xrealloc(l->items, *cap * sizeof *l->items);
    }
    dxl_renderdev *r = &l->items[l->count++];
    r->path = dxl_xstrdup(path);
    r->autodetect = dxl_xstrdup(autodetect ? autodetect : "");
    char *package = dxl_xstrndup(path, (size_t)(dot - path));
    r->caption = dxl_xstrdup(dxl_loc_get(loc, package, dot + 1, "ClassCaption", 0));
    free(package);
    r->desc_flags = game_ini ? dxl_ini_get_int(game_ini, path, "DescFlags", 0) : 0;
    if (autodetected(r->autodetect, autodetect_dirs))       r->priority = 3;
    else if (r->desc_flags & DXL_DESCF_CERTIFIED)            r->priority = 2;
    else if (dxl_stricmp(path, DXL_SOFTWARE_DEVICE) == 0)    r->priority = 1;
    else                                                      r->priority = 0;
}

void dxl_renderdev_load(dxl_renderdev_list *l, const char *system_dir, dxl_loc *loc,
                        const dxl_ini *game_ini, const char *const *autodetect_dirs) {
    memset(l, 0, sizeof *l);
    size_t cap = 0, nfiles = 0;
    char **files = int_files(system_dir, &nfiles);
    for (size_t i = 0; i < nfiles; i++) {
        char *path = dxl_path_join(system_dir, files[i]);
        dxl_ini *ini = dxl_ini_load(path, NULL);
        free(path);
        if (ini) {
            const char *entries[64];
            size_t n = dxl_ini_get_all(ini, "Public", "Object", entries, 64);
            if (n > 64) n = 64;
            for (size_t k = 0; k < n; k++) {
                char *name = field(entries[k], "Name");
                char *cls = field(entries[k], "Class");
                char *meta = field(entries[k], "MetaClass");
                char *autod = field(entries[k], "Autodetect");
                if (name && cls && meta && dxl_stricmp(cls, "Class") == 0 &&
                    dxl_stricmp(meta, "Engine.RenderDevice") == 0)
                    add(l, &cap, name, autod, loc, game_ini, autodetect_dirs);
                free(name); free(cls); free(meta); free(autod);
            }
            dxl_ini_free(ini);
        }
        free(files[i]);
    }
    free(files);
}

void dxl_renderdev_free(dxl_renderdev_list *l) {
    if (!l) return;
    for (size_t i = 0; i < l->count; i++) {
        free(l->items[i].path);
        free(l->items[i].caption);
        free(l->items[i].autodetect);
    }
    free(l->items);
    memset(l, 0, sizeof *l);
}

static const dxl_renderdev_list *sort_list;

static int by_caption(const void *a, const void *b) {
    size_t ia = *(const size_t *)a, ib = *(const size_t *)b;
    int d = dxl_stricmp(sort_list->items[ia].caption, sort_list->items[ib].caption);
    return d ? d : (ia < ib ? -1 : ia > ib);
}

size_t dxl_renderdev_shown(const dxl_renderdev_list *l, int all,
                           size_t *out, size_t max, int *selected) {
    size_t n = 0;
    int best = 0;
    const char *chosen = NULL;
    for (size_t i = 0; i < l->count && n < max; i++) {
        const dxl_renderdev *r = &l->items[i];
        if (!all && r->priority == 0) continue;
        out[n++] = i;
        if (r->priority >= best) { chosen = r->caption; best = r->priority; }
    }
    sort_list = l;
    qsort(out, n, sizeof *out, by_caption);
    *selected = -1;
    for (size_t k = 0; chosen && k < n; k++)
        if (strcmp(l->items[out[k]].caption, chosen) == 0) { *selected = (int)k; break; }
    return n;
}

const char *dxl_renderdev_note(dxl_loc *loc, const char *path) {
    return dxl_loc_get(loc, "Startup", "Descriptions", path, 1);
}

int dxl_renderdev_testrendev(dxl_config *c, const char *system_dir, const char *device,
                             dxl_err *err) {
    dxl_ini_set_int(dxl_config_ini(c), device, "DescFlags", DXL_DESCF_INCOMPATIBLE);
    if (dxl_config_save(c, err) != 0) return -1;
    /* Starting the device is where a Windows driver would write its own
     * flags; none can start here. */
    char *path = dxl_path_join(system_dir, "Detected.ini");
    FILE *f = fopen(path, "wb");
    if (!f) {
        dxl_err_set(err, "cannot write %s", path);
        free(path);
        return -1;
    }
    fclose(f);
    free(path);
    return 0;
}
