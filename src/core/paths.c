#define _GNU_SOURCE
#include "core/paths.h"
#include "platform/fs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void dxl_path_normalize(char *p) {
    if (!p) return;
    for (; *p; p++) if (*p == '\\') *p = '/';
}

char *dxl_path_from_ini(const char *value) {
    char *c = dxl_xstrdup(value ? value : "");
    dxl_path_normalize(c);
    return c;
}

char *dxl_path_join(const char *base, const char *leaf) {
    if (!base || !*base) return dxl_xstrdup(leaf ? leaf : "");
    if (!leaf || !*leaf) return dxl_xstrdup(base);

    size_t bl = strlen(base);
    int sep = !(base[bl - 1] == '/' || base[bl - 1] == '\\');
    while (*leaf == '/' || *leaf == '\\') leaf++;

    size_t n = bl + (size_t)sep + strlen(leaf) + 1;
    char *out = dxl_xmalloc(n);
    snprintf(out, n, "%s%s%s", base, sep ? "/" : "", leaf);
    return out;
}

char *dxl_path_dirname(const char *p) {
    if (!p || !*p) return dxl_xstrdup(".");
    const char *slash = NULL;
    for (const char *s = p; *s; s++) if (*s == '/' || *s == '\\') slash = s;
    if (!slash) return dxl_xstrdup(".");
    /* A root's folder is the root itself: "/", or Windows' "C:/". */
    size_t root = dxl_fs_root_len(p);
    if ((size_t)(slash - p) < root) return dxl_xstrndup(p, root);
    return dxl_xstrndup(p, (size_t)(slash - p));
}

const char *dxl_path_basename(const char *p) {
    if (!p) return "";
    const char *out = p;
    for (const char *s = p; *s; s++) if (*s == '/' || *s == '\\') out = s + 1;
    return out;
}

int dxl_path_exists(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0;
}

int dxl_path_is_dir(const char *p) {
    struct stat st;
    return p && stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

long dxl_path_size(const char *p) {
    struct stat st;
    if (!p || stat(p, &st) != 0 || !S_ISREG(st.st_mode)) return -1;
    return (long)st.st_size;
}

char *dxl_path_resolve_ci(const char *dir, const char *leaf) {
    if (!dir || !leaf) return NULL;

    char *exact = dxl_fs_stored(dir, leaf);
    if (exact) return exact;

    /* The leaf may itself contain separators ("System/DeusEx.u"); resolve one
     * component at a time so each gets the same treatment. */
    char *rest = dxl_xstrdup(leaf);
    dxl_path_normalize(rest);
    if (strchr(rest, '/')) {
        char *cur = dxl_xstrdup(dir);
        char *save = NULL;
        for (char *tok = strtok_r(rest, "/", &save); tok;
             tok = strtok_r(NULL, "/", &save)) {
            char *next = dxl_path_resolve_ci(cur, tok);
            free(cur);
            if (!next) { free(rest); return NULL; }
            cur = next;
        }
        free(rest);
        return cur;
    }
    free(rest);

    DIR *d = opendir(dir);
    if (!d) return NULL;
    struct dirent *e;
    char *found = NULL;
    while ((e = readdir(d)) != NULL) {
        if (dxl_stricmp(e->d_name, leaf) == 0) {
            found = dxl_path_join(dir, e->d_name);
            break;
        }
    }
    closedir(d);
    return found;
}

char *dxl_path_self(void) { return dxl_fs_self(); }

int dxl_path_mkdirs(const char *p) {
    if (!p || !*p) return -1;
    char *buf = dxl_xstrdup(p);
    int rc = 0;
    size_t root = dxl_fs_root_len(p);
    /* Create each prefix in turn, after the root; EEXIST on the way down is
     * expected. */
    for (char *s = buf + (root ? root : 1); ; s++) {
        if (*s == '/' || *s == '\0') {
            char c = *s;
            *s = '\0';
            if (dxl_fs_mkdir(buf) != 0 && !dxl_path_is_dir(buf)) { rc = -1; break; }
            *s = c;
            if (!c) break;
        }
    }
    free(buf);
    return rc;
}

int dxl_path_copy_dir(const char *from, const char *to, int *copied, dxl_err *err) {
    DIR *d = opendir(from);
    if (!d) {
        dxl_err_set(err, "cannot read %s", from);
        return -1;
    }
    if (dxl_path_mkdirs(to) != 0) {
        closedir(d);
        dxl_err_set(err, "cannot create %s", to);
        return -1;
    }
    int rc = 0;
    struct dirent *e;
    while (rc == 0 && (e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char *src = dxl_path_join(from, e->d_name);
        char *dst = dxl_path_join(to, e->d_name);
        if (dxl_path_is_dir(src)) {
            rc = dxl_path_copy_dir(src, dst, copied, err);
        } else {
            rc = dxl_path_copy(src, dst, err);
            if (rc == 0 && copied) (*copied)++;
        }
        free(src);
        free(dst);
    }
    closedir(d);
    return rc;
}

long dxl_path_mtime(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 ? (long)st.st_mtime : -1;
}

int dxl_path_write_atomic(const char *path, const void *data, size_t len, dxl_err *err) {
    return dxl_fs_write_atomic(path, data, len, err);
}

int dxl_path_copy(const char *from, const char *to, dxl_err *err) {
    FILE *in = fopen(from, "rb");
    if (!in) {
        dxl_err_set(err, "cannot open %s", from);
        return -1;
    }
    FILE *out = fopen(to, "wb");
    if (!out) {
        fclose(in);
        dxl_err_set(err, "cannot write %s", to);
        return -1;
    }
    char chunk[16384];
    size_t n;
    int ok = 1;
    while ((n = fread(chunk, 1, sizeof chunk, in)) > 0)
        if (fwrite(chunk, 1, n, out) != n) { ok = 0; break; }
    ok = !ferror(in) && ok;
    fclose(in);
    ok = (fclose(out) == 0) && ok;
    if (!ok) {
        dxl_err_set(err, "copying %s to %s failed", from, to);
        remove(to);
        return -1;
    }
    return 0;
}
