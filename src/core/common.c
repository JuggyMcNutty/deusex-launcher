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

/* Its first token, as the engine's import takes one (the original's token
 * reader, Core.dll 0x10164cb0): a quoted string's text, else the run of
 * letters, digits, _ and - it begins with. */
int dxl_text_bool(const char *v) {
    if (!v) return -1;
    size_t n = 0;
    if (*v == '"') {
        const char *end = strchr(++v, '"');
        if (!end) return -1;
        n = (size_t)(end - v);
    } else {
        while (isalnum((unsigned char)v[n]) || v[n] == '_' || v[n] == '-') n++;
    }
    char t[8];
    if (n >= sizeof t) return -1;
    memcpy(t, v, n);
    t[n] = '\0';
    if (!dxl_stricmp(t, "True") || !dxl_stricmp(t, "Yes") || !strcmp(t, "1")) return 1;
    if (!dxl_stricmp(t, "False") || !dxl_stricmp(t, "No") || !strcmp(t, "0")) return 0;
    return -1;
}

/* The length of the UTF-8 character s begins, or 0 for one that is not. */
static size_t utf8_char(const unsigned char *s) {
    if (s[0] < 0x80) return 1;
    size_t n = s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : s[0] >= 0xe0 && s[0] <= 0xef ? 3 : s[0] >= 0xc2 && s[0] <= 0xdf ? 2 : 0;
    if (!n) return 0;
    for (size_t i = 1; i < n; i++)
        if ((s[i] & 0xc0) != 0x80) return 0;
    if (n == 3 && s[0] == 0xe0 && s[1] < 0xa0) return 0;   /* overlong */
    if (n == 3 && s[0] == 0xed && s[1] >= 0xa0) return 0;  /* a surrogate */
    if (n == 4 && s[0] == 0xf0 && s[1] < 0x90) return 0;   /* overlong */
    if (n == 4 && s[0] == 0xf4 && s[1] >= 0x90) return 0;  /* past U+10FFFF */
    return n;
}

size_t dxl_utf8_char(const char *s) {
    return utf8_char((const unsigned char *)s);
}

void dxl_text_utf8(const char *in, char *out, size_t size) {
    if (!size) return;
    const unsigned char *s = (const unsigned char *)(in ? in : "");
    int utf8 = 1;
    for (const unsigned char *p = s; *p && utf8;) {
        size_t n = utf8_char(p);
        if (!n) utf8 = 0;
        else p += n;
    }
    size_t o = 0;
    for (const unsigned char *p = s; *p;) {
        unsigned char c[4];
        size_t n;
        if (utf8) {
            n = utf8_char(p);
            memcpy(c, p, n);
            p += n;
        } else if (*p < 0x80) {
            c[0] = *p++;
            n = 1;
        } else {
            c[0] = (unsigned char)(0xc0 | (*p >> 6));
            c[1] = (unsigned char)(0x80 | (*p & 0x3f));
            p++;
            n = 2;
        }
        if (o + n >= size) break;
        memcpy(out + o, c, n);
        o += n;
    }
    out[o] = '\0';
}

void dxl_text_drop_cut(char *s) {
    size_t len = strlen(s), p = len;
    while (p > 0 && len - p < 3 && ((unsigned char)s[p - 1] & 0xc0) == 0x80) p--;
    if (p == 0) return;
    unsigned char c = (unsigned char)s[p - 1];
    size_t need = c >= 0xf0 && c <= 0xf4 ? 4 : c >= 0xe0 && c <= 0xef ? 3 : c >= 0xc2 && c <= 0xdf ? 2 : 0;
    if (need > len - (p - 1)) s[p - 1] = '\0';
}
