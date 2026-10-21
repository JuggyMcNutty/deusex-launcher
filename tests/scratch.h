/* A throwaway folder per test run, with files written into it -- for tests
 * that need a System/ folder of their own. Nothing of the game's goes in:
 * each test writes the few lines it needs. */
#ifndef DXL_TEST_SCRATCH_H
#define DXL_TEST_SCRATCH_H

#include "test.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "platform/os.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* <the temporary folder>/dxl-<tag>-<pid>; the same path for the same tag
 * in one run. */
static inline const char *scratch_dir(const char *tag) {
    static char dir[256];
    snprintf(dir, sizeof dir, "%s/dxl-%s-%ld", dxl_test_tmp(), tag, dxl_os_pid());
    dxl_fs_mkdir(dir);
    return dir;
}

/* <dir>/<rel>, creating the folders on the way. */
static inline char *scratch_path(const char *dir, const char *rel) {
    size_t n = strlen(dir) + strlen(rel) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", dir, rel);
    for (char *s = p + strlen(dir) + 1; *s; s++)
        if (*s == '/') { *s = '\0'; dxl_fs_mkdir(p); *s = '/'; }
    return p;
}

static inline void scratch_write(const char *dir, const char *rel, const char *text) {
    char *p = scratch_path(dir, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fputs(text, f); fclose(f); }
    free(p);
}

static inline int scratch_exists(const char *dir, const char *rel) {
    char *p = scratch_path(dir, rel);
    int ok = dxl_path_exists(p);
    free(p);
    return ok;
}

/* A folder and all in it made read-only (on == 0) or writable again: on
 * POSIX by its modes, so a write into it fails; Windows' folders take no
 * such flag, and a listing compared after says what was written. 0, or -1. */
static inline int scratch_writable(const char *dir, int on) {
#ifdef _WIN32
    (void)dir; (void)on;
    return 0;
#else
    char cmd[600];
    snprintf(cmd, sizeof cmd, "chmod -R %s '%s'", on ? "u+w" : "a-w", dir);
    return system(cmd) == 0 ? 0 : -1;
#endif
}

static int scratch_by_name(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Every file and folder under dir, its size and time, a line each in name
 * order: to compare after. Caller frees. */
static inline void scratch_list_into(const char *dir, const char *rel, char ***lines, size_t *n, size_t *cap) {
    char *path = rel[0] ? dxl_path_join(dir, rel) : dxl_xstrdup(dir);
    DIR *d = opendir(path);
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char *child = rel[0] ? dxl_path_join(rel, e->d_name) : dxl_xstrdup(e->d_name);
        char *full = dxl_path_join(dir, child);
        struct stat st;
        if (stat(full, &st) == 0) {
            if (*n == *cap) *lines = realloc(*lines, (*cap = *cap ? *cap * 2 : 64) * sizeof **lines);
            size_t len = strlen(child) + 48;
            (*lines)[*n] = malloc(len);
            snprintf((*lines)[(*n)++], len, "%s %lld %lld\n", child, (long long)st.st_size, (long long)st.st_mtime);
            if (S_ISDIR(st.st_mode)) scratch_list_into(dir, child, lines, n, cap);
        }
        free(full);
        free(child);
    }
    if (d) closedir(d);
    free(path);
}

static inline char *scratch_listing(const char *dir) {
    char **lines = NULL;
    size_t n = 0, cap = 0, len = 1;
    scratch_list_into(dir, "", &lines, &n, &cap);
    if (n) qsort(lines, n, sizeof *lines, scratch_by_name);
    for (size_t i = 0; i < n; i++) len += strlen(lines[i]);
    char *text = malloc(len);
    text[0] = '\0';
    for (size_t i = 0; i < n; i++) {
        strcat(text, lines[i]);
        free(lines[i]);
    }
    free(lines);
    return text;
}

/* rm -r, for a folder the test made. */
static inline void scratch_remove(const char *dir) {
    if (dxl_fs_remove_tree(dir) != 0) fprintf(stderr, "could not remove %s\n", dir);
}

#endif
