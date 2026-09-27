#include "core/common.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dxl_err_set(dxl_err *e, const char *fmt, ...) {
    if (!e) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);
}

const char *dxl_err_msg(const dxl_err *e) {
    return (e && e->msg[0]) ? e->msg : "unknown error";
}

static void oom(size_t n) {
    fprintf(stderr, "deusex-launcher: out of memory allocating %zu bytes\n", n);
    abort();
}

void *dxl_xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) oom(n);
    return p;
}

void *dxl_xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) oom(n);
    return q;
}

char *dxl_xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = dxl_xmalloc(n);
    memcpy(p, s, n);
    return p;
}

char *dxl_xstrndup(const char *s, size_t n) {
    char *p = dxl_xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

int dxl_stricmp(const char *a, const char *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    for (;; a++, b++) {
        int d = lower((unsigned char)*a) - lower((unsigned char)*b);
        if (d || !*a) return d;
    }
}

int dxl_strnicmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int d = lower((unsigned char)a[i]) - lower((unsigned char)b[i]);
        if (d || !a[i]) return d;
    }
    return 0;
}

const char *dxl_stristr(const char *hay, const char *needle) {
    if (!hay || !needle) return NULL;
    if (!*needle) return hay;
    size_t n = strlen(needle);
    for (const char *p = hay; *p; p++)
        if (dxl_strnicmp(p, needle, n) == 0) return p;
    return NULL;
}

char *dxl_trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return s;
}

void dxl_buf_init(dxl_buf *b) { b->data = NULL; b->len = b->cap = 0; }

void dxl_buf_free(dxl_buf *b) { free(b->data); dxl_buf_init(b); }

static void buf_reserve(dxl_buf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return;
    size_t cap = b->cap ? b->cap : 64;
    while (cap < b->len + extra + 1) cap *= 2;
    b->data = dxl_xrealloc(b->data, cap);
    b->cap = cap;
}

void dxl_buf_add(dxl_buf *b, const char *s, size_t n) {
    if (!n) { buf_reserve(b, 0); b->data[b->len] = '\0'; return; }
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

void dxl_buf_puts(dxl_buf *b, const char *s) {
    if (s) dxl_buf_add(b, s, strlen(s));
}

void dxl_buf_printf(dxl_buf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char tmp[512];
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n < sizeof tmp) { dxl_buf_add(b, tmp, (size_t)n); return; }
    char *big = dxl_xmalloc((size_t)n + 1);
    va_start(ap, fmt);
    vsnprintf(big, (size_t)n + 1, fmt, ap);
    va_end(ap);
    dxl_buf_add(b, big, (size_t)n);
    free(big);
}

void dxl_buf_word(dxl_buf *b, const char *s) {
    if (!s || !*s) return;
    if (b->len) dxl_buf_add(b, " ", 1);
    dxl_buf_puts(b, s);
}
