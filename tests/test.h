/* Minimal test scaffolding. No dependency, because the core has none and the
 * cross target should stay buildable without a test framework in the sysroot. */
#ifndef DXL_TEST_H
#define DXL_TEST_H

#include "platform/fs.h"
#include "platform/os.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int dxl_test_failures;
extern const char *dxl_test_current;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL %s:%d in %s: %s\n", \
                __FILE__, __LINE__, dxl_test_current, #cond); \
        dxl_test_failures++; \
    } \
} while (0)

#define CHECK_STR(got, want) do { \
    const char *g_ = (got), *w_ = (want); \
    if (!g_ || !w_ || strcmp(g_, w_) != 0) { \
        fprintf(stderr, "  FAIL %s:%d in %s: got \"%s\" want \"%s\"\n", \
                __FILE__, __LINE__, dxl_test_current, g_ ? g_ : "(null)", \
                w_ ? w_ : "(null)"); \
        dxl_test_failures++; \
    } \
} while (0)

#define CHECK_INT(got, want) do { \
    long g_ = (long)(got), w_ = (long)(want); \
    if (g_ != w_) { \
        fprintf(stderr, "  FAIL %s:%d in %s: got %ld want %ld\n", \
                __FILE__, __LINE__, dxl_test_current, g_, w_); \
        dxl_test_failures++; \
    } \
} while (0)

#define RUN(fn) do { dxl_test_current = #fn; fn(); } while (0)

#define TEST_MAIN_BEGIN int dxl_test_failures = 0; \
                        const char *dxl_test_current = "(none)"; \
                        int main(int argc, char **argv) { (void)argc; (void)argv; \
                            dxl_test_runtime();
#define TEST_MAIN_END   if (dxl_test_failures) { \
                            fprintf(stderr, "%d check(s) failed\n", dxl_test_failures); \
                            return 1; \
                        } \
                        printf("ok\n"); \
                        return 0; }

/* The system's folder for temporary files, where the tests make theirs. */
static inline const char *dxl_test_tmp(void) {
    static char *tmp;
    if (!tmp) tmp = dxl_fs_temp_dir();
    return tmp;
}

#ifndef _WIN32
/* The test's runtime folder and the process that made it: a child forked
 * from a test leaves it be. */
static inline char *dxl_test_runtime_dir(void) {
    static char dir[300];
    return dir;
}

static inline void dxl_test_runtime_leave(void) {
    char *dir = dxl_test_runtime_dir(), mine[32];
    snprintf(mine, sizeof mine, "-%ld", dxl_os_pid());
    size_t n = strlen(dir), m = strlen(mine);
    if (n > m && !strcmp(dir + n - m, mine)) dxl_fs_remove_tree(dir);
}
#endif

/* The runtime folder ($XDG_RUNTIME_DIR), where a launcher keeps its lock
 * and its channels (platform/posix/instance.c): the test's own, made at
 * its start and removed as it ends, so a run leaves no file in the user's
 * (bugs.md X.18). The desktop's own sockets in the user's stay found, each
 * by its full path, for a launcher a test starts with a window: Wayland's,
 * the session bus, PipeWire's, PulseAudio's. Nothing on Windows, whose
 * marks are no files. */
static inline void dxl_test_runtime(void) {
#ifndef _WIN32
    char real[256] = "", path[300];
    const char *was = getenv("XDG_RUNTIME_DIR"), *wayland = getenv("WAYLAND_DISPLAY");
    if (was && strlen(was) < sizeof real) strcpy(real, was);
    if (real[0]) {
        if (wayland && *wayland && wayland[0] != '/') {
            snprintf(path, sizeof path, "%s/%.40s", real, wayland);
            dxl_env_set("WAYLAND_DISPLAY", path);
        }
        if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
            snprintf(path, sizeof path, "unix:path=%s/bus", real);
            dxl_env_set("DBUS_SESSION_BUS_ADDRESS", path);
        }
        if (!getenv("PIPEWIRE_RUNTIME_DIR")) dxl_env_set("PIPEWIRE_RUNTIME_DIR", real);
        if (!getenv("PULSE_SERVER")) {
            snprintf(path, sizeof path, "unix:%s/pulse/native", real);
            dxl_env_set("PULSE_SERVER", path);
        }
    }
    char *dir = dxl_test_runtime_dir();
    snprintf(dir, 300, "%s/dxl-run-%ld", dxl_test_tmp(), dxl_os_pid());
    dxl_fs_mkdir(dir);
    dxl_env_set("XDG_RUNTIME_DIR", dir);
    atexit(dxl_test_runtime_leave);
#endif
}

/* Absolute path to tests/fixtures, passed in by CMake. */
#ifndef DXL_FIXTURES
#define DXL_FIXTURES "tests/fixtures"
#endif
static inline char *fixture(const char *name) {
    size_t n = strlen(DXL_FIXTURES) + strlen(name) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", DXL_FIXTURES, name);
    return p;
}
static inline char *slurp(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    if (len) *len = got;
    return buf;
}

#endif
