/* A throwaway folder per test run, with files written into it -- for tests
 * that need a System/ folder of their own. Nothing of the game's goes in:
 * each test writes the few lines it needs. */
#ifndef DXL_TEST_SCRATCH_H
#define DXL_TEST_SCRATCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* /tmp/dxl-<tag>-<pid>; the same path for the same tag in one run. */
static inline const char *scratch_dir(const char *tag) {
    static char dir[256];
    snprintf(dir, sizeof dir, "/tmp/dxl-%s-%d", tag, (int)getpid());
    mkdir(dir, 0755);
    return dir;
}

/* <dir>/<rel>, creating the folders on the way. */
static inline char *scratch_path(const char *dir, const char *rel) {
    size_t n = strlen(dir) + strlen(rel) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", dir, rel);
    for (char *s = p + strlen(dir) + 1; *s; s++)
        if (*s == '/') { *s = '\0'; mkdir(p, 0755); *s = '/'; }
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
    int ok = access(p, F_OK) == 0;
    free(p);
    return ok;
}

/* rm -r, for a folder the test made. */
static inline void scratch_remove(const char *dir) {
    char cmd[512];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "could not remove %s\n", dir);
}

#endif
