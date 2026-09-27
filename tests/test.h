/* Minimal test scaffolding. No dependency, because the core has none and the
 * cross target should stay buildable without a test framework in the sysroot. */
#ifndef DXL_TEST_H
#define DXL_TEST_H

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
                        int main(int argc, char **argv) { (void)argc; (void)argv;
#define TEST_MAIN_END   if (dxl_test_failures) { \
                            fprintf(stderr, "%d check(s) failed\n", dxl_test_failures); \
                            return 1; \
                        } \
                        printf("ok\n"); \
                        return 0; }

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
