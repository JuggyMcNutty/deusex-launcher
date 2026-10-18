#define _GNU_SOURCE
#include "core/vdf.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Deeper than any of Steam's files nest, shallow enough for the stack. */
#define DEPTH_MAX 32
/* More than any of Steam's files hold. */
#define FILE_MAX (4L << 20)

struct dxl_vdf {
    char     *key;
    char     *value;      /* NULL for a block */
    dxl_vdf **items;
    size_t    count, cap;
};

static dxl_vdf *node(char *key) {
    dxl_vdf *v = dxl_xmalloc(sizeof *v);
    memset(v, 0, sizeof *v);
    v->key = key;
    return v;
}

void dxl_vdf_free(dxl_vdf *v) {
    if (!v) return;
    for (size_t i = 0; i < v->count; i++) dxl_vdf_free(v->items[i]);
    free(v->items);
    free(v->key);
    free(v->value);
    free(v);
}

static void push(dxl_vdf *block, dxl_vdf *child) {
    if (block->count == block->cap) {
        block->cap = block->cap ? block->cap * 2 : 8;
        block->items = dxl_xrealloc(block->items, block->cap * sizeof *block->items);
    }
    block->items[block->count++] = child;
}

/* ---- reading --------------------------------------------------------- */

typedef struct {
    const char *p, *end;
    int         line;
    dxl_err    *err;
} reader;

typedef enum { T_END, T_STRING, T_OPEN, T_CLOSE, T_BAD } token;

/* Past spaces, line ends, comments, and a NUL byte, which no file of
 * Steam's has. */
static void skip(reader *r) {
    while (r->p < r->end) {
        char c = *r->p;
        if (c == '\n') {
            r->line++;
            r->p++;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\0') {
            r->p++;
        } else if (c == '/' && r->p + 1 < r->end && r->p[1] == '/') {
            while (r->p < r->end && *r->p != '\n') r->p++;
        } else {
            return;
        }
    }
}

/* The next token; a string's text into *text, the caller's to free. */
static token next(reader *r, char **text) {
    *text = NULL;
    for (;;) {
        skip(r);
        if (r->p >= r->end) return T_END;
        /* A condition, [$WIN32]: not the file's on this system or any. */
        if (*r->p != '[') break;
        while (r->p < r->end && *r->p != ']' && *r->p != '\n') r->p++;
        if (r->p < r->end && *r->p == ']') r->p++;
    }
    char c = *r->p;
    if (c == '{') { r->p++; return T_OPEN; }
    if (c == '}') { r->p++; return T_CLOSE; }
    dxl_buf b;
    dxl_buf_init(&b);
    if (c == '"') {
        r->p++;
        for (;;) {
            if (r->p >= r->end) {
                dxl_buf_free(&b);
                dxl_err_set(r->err, "a quote left open at line %d", r->line);
                return T_BAD;
            }
            char d = *r->p++;
            if (d == '"') break;
            if (d == '\n') r->line++;
            if (d == '\\' && r->p < r->end) {
                char e = *r->p++;
                d = e == 'n' ? '\n' : e == 't' ? '\t' : e;
            }
            dxl_buf_add(&b, &d, 1);
        }
    } else {
        while (r->p < r->end && *r->p && !strchr(" \t\r\n{}\"", *r->p)) dxl_buf_add(&b, r->p++, 1);
    }
    *text = b.data ? b.data : dxl_xstrdup("");
    return T_STRING;
}

/* A block's keys, to its closing brace (or the file's end, at the top). */
static int read_block(reader *r, dxl_vdf *block, int depth, int top) {
    for (;;) {
        char *key;
        token t = next(r, &key);
        if (t == T_BAD) return -1;
        if (t == T_END) {
            if (top) return 0;
            dxl_err_set(r->err, "a brace left open at the end");
            return -1;
        }
        if (t == T_CLOSE) {
            if (!top) return 0;
            dxl_err_set(r->err, "a brace closed with none open at line %d", r->line);
            return -1;
        }
        if (t == T_OPEN) {
            dxl_err_set(r->err, "a block with no key at line %d", r->line);
            return -1;
        }
        char *value;
        token u = next(r, &value);
        if (u == T_STRING) {
            dxl_vdf *v = node(key);
            v->value = value;
            push(block, v);
        } else if (u == T_OPEN) {
            if (depth >= DEPTH_MAX) {
                free(key);
                dxl_err_set(r->err, "blocks nested past %d deep at line %d", DEPTH_MAX, r->line);
                return -1;
            }
            dxl_vdf *v = node(key);
            push(block, v);
            if (read_block(r, v, depth + 1, 0) != 0) return -1;
        } else {
            free(key);
            if (u != T_BAD) dxl_err_set(r->err, "a key with nothing after it at line %d", r->line);
            return -1;
        }
    }
}

dxl_vdf *dxl_vdf_parse(const char *text, size_t len, dxl_err *err) {
    reader r = { text, text + len, 1, err };
    dxl_vdf *top = node(NULL);
    if (read_block(&r, top, 0, 1) != 0) {
        dxl_vdf_free(top);
        return NULL;
    }
    return top;
}

dxl_vdf *dxl_vdf_load(const char *path, dxl_err *err) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot open %s: %s", path, strerror(errno));
        return NULL;
    }
    dxl_buf b;
    dxl_buf_init(&b);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0 && b.len <= (size_t)FILE_MAX) dxl_buf_add(&b, chunk, n);
    fclose(f);
    dxl_vdf *v = NULL;
    if (b.len > (size_t)FILE_MAX) dxl_err_set(err, "%s is larger than Steam writes one", path);
    else v = dxl_vdf_parse(b.data ? b.data : "", b.len, err);
    dxl_buf_free(&b);
    return v;
}

/* ---- looking ----------------------------------------------------------- */

const char *dxl_vdf_key(const dxl_vdf *v) { return v ? v->key : NULL; }
const char *dxl_vdf_value(const dxl_vdf *v) { return v ? v->value : NULL; }
size_t dxl_vdf_count(const dxl_vdf *v) { return v && !v->value ? v->count : 0; }

const dxl_vdf *dxl_vdf_at(const dxl_vdf *v, size_t i) {
    return i < dxl_vdf_count(v) ? v->items[i] : NULL;
}

const dxl_vdf *dxl_vdf_get(const dxl_vdf *v, const char *key) {
    for (size_t i = 0; i < dxl_vdf_count(v); i++)
        if (v->items[i]->key && !dxl_stricmp(v->items[i]->key, key)) return v->items[i];
    return NULL;
}

const char *dxl_vdf_get_string(const dxl_vdf *v, const char *key) {
    return dxl_vdf_value(dxl_vdf_get(v, key));
}
