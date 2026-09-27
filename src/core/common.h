/* Shared primitives for the launcher core.
 *
 * The core is deliberately free of SDL, of Win32 concepts, and of any policy
 * about where files live: callers pass absolute paths in. That is what lets
 * the whole contract be unit-tested on a host with no display.
 */
#ifndef DXL_COMMON_H
#define DXL_COMMON_H

#include <stddef.h>
#include <stdarg.h>

#define DXL_ERR_LEN 256

/* Carries a human-readable failure reason back to the caller. Functions that
 * take one may be passed NULL when the reason is not wanted. */
typedef struct {
    char msg[DXL_ERR_LEN];
} dxl_err;

void  dxl_err_set(dxl_err *e, const char *fmt, ...);
const char *dxl_err_msg(const dxl_err *e);

/* Allocation that aborts rather than returning NULL. A launcher that cannot
 * allocate a few kilobytes has nothing useful left to do, and threading the
 * failure through every call site would bury the logic we care about. */
void *dxl_xmalloc(size_t n);
void *dxl_xrealloc(void *p, size_t n);
char *dxl_xstrdup(const char *s);
char *dxl_xstrndup(const char *s, size_t n);

/* ASCII case-insensitive compare. Deliberately not locale-aware: ini section
 * and key names are ASCII, and a Turkish locale must not change how
 * "GameRenderDevice" matches. */
int  dxl_stricmp(const char *a, const char *b);
int  dxl_strnicmp(const char *a, const char *b, size_t n);
/* Case-insensitive substring search -- the appStrfind of dx-reverse-info/cli-flags.md. */
const char *dxl_stristr(const char *hay, const char *needle);

/* In-place whitespace trim; returns a pointer into the original buffer. */
char *dxl_trim(char *s);

/* Growable string, used for building flag strings and file contents. */
typedef struct {
    char  *data;
    size_t len, cap;
} dxl_buf;

void dxl_buf_init(dxl_buf *b);
void dxl_buf_free(dxl_buf *b);
void dxl_buf_add(dxl_buf *b, const char *s, size_t n);
void dxl_buf_puts(dxl_buf *b, const char *s);
void dxl_buf_printf(dxl_buf *b, const char *fmt, ...);
/* Appends s, separated from existing content by a single space if non-empty. */
void dxl_buf_word(dxl_buf *b, const char *s);

#endif
