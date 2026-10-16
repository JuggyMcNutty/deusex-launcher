#include "core/localize.h"
#include "core/ini.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char    *package;   /* as asked for; matched case-insensitively */
    char    *ext;
    dxl_ini *ini;       /* NULL when the file is not there */
} loc_file;

struct dxl_loc {
    char     *system_dir;
    char     *language;
    loc_file *files;
    size_t    file_count, file_cap;
    /* Everything handed out -- unquoted values, placeholders -- so returned
     * pointers stay valid for the object's life. */
    char    **owned;
    size_t    owned_count, owned_cap;
};

dxl_loc *dxl_loc_open(const char *system_dir, const char *language) {
    dxl_loc *l = dxl_xmalloc(sizeof *l);
    memset(l, 0, sizeof *l);
    l->system_dir = dxl_xstrdup(system_dir ? system_dir : ".");
    l->language = dxl_xstrdup(language && *language ? language : "int");
    return l;
}

void dxl_loc_free(dxl_loc *l) {
    if (!l) return;
    for (size_t i = 0; i < l->file_count; i++) {
        free(l->files[i].package);
        free(l->files[i].ext);
        dxl_ini_free(l->files[i].ini);
    }
    free(l->files);
    for (size_t i = 0; i < l->owned_count; i++) free(l->owned[i]);
    free(l->owned);
    free(l->system_dir);
    free(l->language);
    free(l);
}

static const char *keep(dxl_loc *l, char *s) {
    if (l->owned_count == l->owned_cap) {
        l->owned_cap = l->owned_cap ? l->owned_cap * 2 : 32;
        l->owned = dxl_xrealloc(l->owned, l->owned_cap * sizeof *l->owned);
    }
    l->owned[l->owned_count++] = s;
    return s;
}

static dxl_ini *file_for(dxl_loc *l, const char *package, const char *ext) {
    for (size_t i = 0; i < l->file_count; i++)
        if (dxl_stricmp(l->files[i].package, package) == 0 &&
            dxl_stricmp(l->files[i].ext, ext) == 0)
            return l->files[i].ini;

    if (l->file_count == l->file_cap) {
        l->file_cap = l->file_cap ? l->file_cap * 2 : 8;
        l->files = dxl_xrealloc(l->files, l->file_cap * sizeof *l->files);
    }
    loc_file *f = &l->files[l->file_count++];
    f->package = dxl_xstrdup(package);
    f->ext = dxl_xstrdup(ext);
    size_t n = strlen(package) + strlen(ext) + 2;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, "%s.%s", package, ext);
    char *path = dxl_path_resolve_ci(l->system_dir, leaf);
    free(leaf);
    f->ini = path ? dxl_ini_load(path, NULL) : NULL;
    free(path);
    return f->ini;
}

/* UE1's reader drops a value's surrounding quotes, and only when both are
 * there (FConfigFile::Read). */
static const char *unquoted(dxl_loc *l, const char *v) {
    size_t n = strlen(v);
    if (n >= 2 && v[0] == '"' && v[n - 1] == '"')
        return keep(l, dxl_xstrndup(v + 1, n - 2));
    return v;
}

const char *dxl_loc_get(dxl_loc *l, const char *package, const char *section,
                        const char *key, int optional) {
    const char *exts[2] = { l->language, "int" };
    for (int i = 0; i < 2; i++) {
        if (i == 1 && dxl_stricmp(exts[0], "int") == 0) break;
        dxl_ini *ini = file_for(l, package, exts[i]);
        const char *v = ini ? dxl_ini_get(ini, section, key) : NULL;
        if (v) return unquoted(l, v);
    }
    if (optional) return "";
    size_t n = strlen(l->language) + strlen(package) + strlen(section) + strlen(key) + 16;
    char *s = dxl_xmalloc(n);
    snprintf(s, n, "<?%s?%s.%s.%s?>", l->language, package, section, key);
    return keep(l, s);
}

const char *dxl_loc_general(dxl_loc *l, const char *package, const char *key) {
    return dxl_loc_get(l, package, "General", key, 0);
}

char *dxl_loc_line_format(const char *s) {
    char *out = dxl_xmalloc(strlen(s ? s : "") + 1);
    char *o = out;
    for (const char *p = s ? s : ""; *p; ) {
        if (*p != '\\') { *o++ = *p++; continue; }
        p++;
        if (!*p) break;              /* a trailing backslash: nothing to keep */
        *o++ = (*p == 'n') ? '\n' : *p;
        p++;
    }
    *o = '\0';
    return out;
}
