#define _GNU_SOURCE
#include "core/json.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct dxl_json {
    dxl_json_type type;
    int     b;
    double  num;
    char   *text;       /* string value, or a number's original spelling */

    /* Arrays and objects. keys[i] is NULL for array elements. */
    dxl_json **items;
    char     **keys;
    size_t     count, cap;
};

static dxl_json *node(dxl_json_type t) {
    dxl_json *v = dxl_xmalloc(sizeof *v);
    memset(v, 0, sizeof *v);
    v->type = t;
    return v;
}

void dxl_json_free(dxl_json *v) {
    if (!v) return;
    for (size_t i = 0; i < v->count; i++) {
        dxl_json_free(v->items[i]);
        free(v->keys[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->text);
    free(v);
}

static void push(dxl_json *parent, char *key, dxl_json *child) {
    if (parent->count == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 8;
        parent->items = dxl_xrealloc(parent->items, parent->cap * sizeof *parent->items);
        parent->keys  = dxl_xrealloc(parent->keys,  parent->cap * sizeof *parent->keys);
    }
    parent->items[parent->count] = child;
    parent->keys[parent->count]  = key;
    parent->count++;
}

/* ---- parser ---------------------------------------------------------- */

typedef struct {
    const char *p, *end;
    dxl_err    *err;
    int         failed;
    int         depth;
} reader;

static void fail(reader *r, const char *what) {
    if (!r->failed) {
        dxl_err_set(r->err, "JSON: %s", what);
        r->failed = 1;
    }
}

static void skip_ws(reader *r) {
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r'))
        r->p++;
}

static int literal(reader *r, const char *word) {
    size_t n = strlen(word);
    if ((size_t)(r->end - r->p) >= n && memcmp(r->p, word, n) == 0) {
        r->p += n;
        return 1;
    }
    return 0;
}

static void utf8_put(dxl_buf *b, unsigned cp) {
    char out[4];
    size_t n;
    if (cp < 0x80)        { out[0] = (char)cp; n = 1; }
    else if (cp < 0x800)  { out[0] = (char)(0xC0 | (cp >> 6));
                            out[1] = (char)(0x80 | (cp & 0x3F)); n = 2; }
    else if (cp < 0x10000){ out[0] = (char)(0xE0 | (cp >> 12));
                            out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            out[2] = (char)(0x80 | (cp & 0x3F)); n = 3; }
    else                  { out[0] = (char)(0xF0 | (cp >> 18));
                            out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                            out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            out[3] = (char)(0x80 | (cp & 0x3F)); n = 4; }
    dxl_buf_add(b, out, n);
}

static int hex4(reader *r, unsigned *out) {
    if (r->end - r->p < 4) return 0;
    unsigned v = 0;
    for (int i = 0; i < 4; i++) {
        char c = r->p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return 0;
    }
    r->p += 4;
    *out = v;
    return 1;
}

/* Called with r->p on the opening quote. Returns a malloc'd string. */
static char *read_string(reader *r) {
    r->p++;
    dxl_buf b;
    dxl_buf_init(&b);
    while (r->p < r->end && *r->p != '"') {
        unsigned char c = (unsigned char)*r->p;
        if (c < 0x20) { fail(r, "control character in string"); break; }
        if (c != '\\') { dxl_buf_add(&b, r->p, 1); r->p++; continue; }

        r->p++;
        if (r->p >= r->end) break;
        char e = *r->p++;
        switch (e) {
        case '"':  dxl_buf_add(&b, "\"", 1); break;
        case '\\': dxl_buf_add(&b, "\\", 1); break;
        case '/':  dxl_buf_add(&b, "/", 1);  break;
        case 'b':  dxl_buf_add(&b, "\b", 1); break;
        case 'f':  dxl_buf_add(&b, "\f", 1); break;
        case 'n':  dxl_buf_add(&b, "\n", 1); break;
        case 'r':  dxl_buf_add(&b, "\r", 1); break;
        case 't':  dxl_buf_add(&b, "\t", 1); break;
        case 'u': {
            unsigned cp;
            if (!hex4(r, &cp)) { fail(r, "bad \\u escape"); break; }
            /* A surrogate pair spells one code point above the BMP. */
            if (cp >= 0xD800 && cp <= 0xDBFF && r->end - r->p >= 6 &&
                r->p[0] == '\\' && r->p[1] == 'u') {
                const char *save = r->p;
                unsigned lo;
                r->p += 2;
                if (hex4(r, &lo) && lo >= 0xDC00 && lo <= 0xDFFF)
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                else
                    r->p = save;
            }
            utf8_put(&b, cp);
            break;
        }
        default:
            fail(r, "unknown escape");
        }
        if (r->failed) break;
    }
    if (!r->failed && (r->p >= r->end || *r->p != '"')) fail(r, "unterminated string");
    if (r->failed) { dxl_buf_free(&b); return NULL; }
    r->p++;
    if (!b.data) dxl_buf_puts(&b, "");
    return b.data;
}

static dxl_json *read_value(reader *r);

static dxl_json *read_number(reader *r) {
    const char *s = r->p;
    if (r->p < r->end && *r->p == '-') r->p++;
    if (r->p >= r->end || *r->p < '0' || *r->p > '9') { fail(r, "bad number"); return NULL; }
    while (r->p < r->end && ((*r->p >= '0' && *r->p <= '9') || *r->p == '.' ||
                             *r->p == 'e' || *r->p == 'E' || *r->p == '+' || *r->p == '-'))
        r->p++;
    char *spelling = dxl_xstrndup(s, (size_t)(r->p - s));
    char *endp = NULL;
    double d = strtod(spelling, &endp);
    if (!endp || *endp) { free(spelling); fail(r, "bad number"); return NULL; }
    dxl_json *v = node(DXL_JSON_NUMBER);
    v->num = d;
    v->text = spelling;
    return v;
}

static dxl_json *read_container(reader *r, int is_object) {
    if (++r->depth > 64) { fail(r, "nested too deeply"); return NULL; }
    dxl_json *v = node(is_object ? DXL_JSON_OBJECT : DXL_JSON_ARRAY);
    char close = is_object ? '}' : ']';
    r->p++;
    skip_ws(r);
    if (r->p < r->end && *r->p == close) { r->p++; r->depth--; return v; }

    for (;;) {
        char *key = NULL;
        if (is_object) {
            skip_ws(r);
            if (r->p >= r->end || *r->p != '"') { fail(r, "expected member name"); break; }
            key = read_string(r);
            if (!key) break;
            skip_ws(r);
            if (r->p >= r->end || *r->p != ':') { free(key); fail(r, "expected ':'"); break; }
            r->p++;
        }
        dxl_json *child = read_value(r);
        if (!child) { free(key); break; }
        push(v, key, child);

        skip_ws(r);
        if (r->p < r->end && *r->p == ',') { r->p++; continue; }
        if (r->p < r->end && *r->p == close) { r->p++; r->depth--; return v; }
        fail(r, is_object ? "expected ',' or '}'" : "expected ',' or ']'");
        break;
    }
    dxl_json_free(v);
    return NULL;
}

static dxl_json *read_value(reader *r) {
    skip_ws(r);
    if (r->p >= r->end) { fail(r, "unexpected end of data"); return NULL; }
    switch (*r->p) {
    case '{': return read_container(r, 1);
    case '[': return read_container(r, 0);
    case '"': {
        char *s = read_string(r);
        if (!s) return NULL;
        dxl_json *v = node(DXL_JSON_STRING);
        v->text = s;
        return v;
    }
    default:
        break;
    }
    if (literal(r, "true"))  { dxl_json *v = node(DXL_JSON_BOOL); v->b = 1; return v; }
    if (literal(r, "false")) { return node(DXL_JSON_BOOL); }
    if (literal(r, "null"))  { return node(DXL_JSON_NULL); }
    return read_number(r);
}

dxl_json *dxl_json_parse(const char *text, size_t len, dxl_err *err) {
    reader r = { text, text + len, err, 0, 0 };
    /* Tolerate a UTF-8 byte-order mark; editors on the host add one. */
    if (len >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        r.p += 3;
    dxl_json *v = read_value(&r);
    if (!v) return NULL;
    skip_ws(&r);
    if (r.p != r.end) {
        fail(&r, "trailing data after the document");
        dxl_json_free(v);
        return NULL;
    }
    return v;
}

dxl_json *dxl_json_load(const char *path, dxl_err *err) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot open %s: %s", path, strerror(errno));
        return NULL;
    }
    dxl_buf b;
    dxl_buf_init(&b);
    char chunk[4096];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&b, chunk, n);
    fclose(f);
    dxl_json *v = dxl_json_parse(b.data ? b.data : "", b.len, err);
    dxl_buf_free(&b);
    return v;
}

/* ---- writer ---------------------------------------------------------- */

static void write_string(dxl_buf *b, const char *s) {
    dxl_buf_add(b, "\"", 1);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  dxl_buf_puts(b, "\\\""); break;
        case '\\': dxl_buf_puts(b, "\\\\"); break;
        case '\n': dxl_buf_puts(b, "\\n");  break;
        case '\r': dxl_buf_puts(b, "\\r");  break;
        case '\t': dxl_buf_puts(b, "\\t");  break;
        case '\b': dxl_buf_puts(b, "\\b");  break;
        case '\f': dxl_buf_puts(b, "\\f");  break;
        default:
            if (*p < 0x20) dxl_buf_printf(b, "\\u%04x", *p);
            else dxl_buf_add(b, (const char *)p, 1);
        }
    }
    dxl_buf_add(b, "\"", 1);
}

static void indent(dxl_buf *b, int depth) {
    dxl_buf_add(b, "\n", 1);
    for (int i = 0; i < depth; i++) dxl_buf_add(b, "  ", 2);
}

static void write_value(dxl_buf *b, const dxl_json *v, int depth) {
    switch (v->type) {
    case DXL_JSON_NULL:   dxl_buf_puts(b, "null"); break;
    case DXL_JSON_BOOL:   dxl_buf_puts(b, v->b ? "true" : "false"); break;
    case DXL_JSON_NUMBER: dxl_buf_puts(b, v->text); break;
    case DXL_JSON_STRING: write_string(b, v->text); break;
    case DXL_JSON_ARRAY:
    case DXL_JSON_OBJECT: {
        int obj = v->type == DXL_JSON_OBJECT;
        dxl_buf_add(b, obj ? "{" : "[", 1);
        for (size_t i = 0; i < v->count; i++) {
            if (i) dxl_buf_add(b, ",", 1);
            indent(b, depth + 1);
            if (obj) {
                write_string(b, v->keys[i]);
                dxl_buf_puts(b, ": ");
            }
            write_value(b, v->items[i], depth + 1);
        }
        if (v->count) indent(b, depth);
        dxl_buf_add(b, obj ? "}" : "]", 1);
        break;
    }
    }
}

char *dxl_json_render(const dxl_json *v) {
    dxl_buf b;
    dxl_buf_init(&b);
    write_value(&b, v, 0);
    dxl_buf_add(&b, "\n", 1);
    return b.data;
}

int dxl_json_save(const dxl_json *v, const char *path, dxl_err *err) {
    char *text = dxl_json_render(v);
    size_t n = strlen(path) + 8;
    char *tmp = dxl_xmalloc(n);
    snprintf(tmp, n, "%s.tmp", path);

    int rc = -1;
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        dxl_err_set(err, "cannot write %s: %s", tmp, strerror(errno));
    } else {
        size_t len = strlen(text);
        int ok = fwrite(text, 1, len, f) == len;
        ok = (fflush(f) == 0) && ok;
        /* exFAT on the device: without the sync a power-off after rename can
         * still leave the new name pointing at unwritten blocks. */
        ok = (fsync(fileno(f)) == 0) && ok;
        ok = (fclose(f) == 0) && ok;
        if (!ok) {
            dxl_err_set(err, "cannot write %s: %s", tmp, strerror(errno));
            remove(tmp);
        } else if (rename(tmp, path) != 0) {
            dxl_err_set(err, "cannot replace %s: %s", path, strerror(errno));
            remove(tmp);
        } else {
            rc = 0;
        }
    }
    free(tmp);
    free(text);
    return rc;
}

/* ---- access ---------------------------------------------------------- */

dxl_json *dxl_json_new_object(void) { return node(DXL_JSON_OBJECT); }
dxl_json_type dxl_json_kind(const dxl_json *v) { return v ? v->type : DXL_JSON_NULL; }

static ssize_t find(const dxl_json *obj, const char *key) {
    if (!obj || obj->type != DXL_JSON_OBJECT) return -1;
    for (size_t i = 0; i < obj->count; i++)
        if (strcmp(obj->keys[i], key) == 0) return (ssize_t)i;
    return -1;
}

dxl_json *dxl_json_get(const dxl_json *obj, const char *key) {
    ssize_t i = find(obj, key);
    return i < 0 ? NULL : obj->items[i];
}

/* Puts v at key, freeing whatever was there, keeping the member's slot. */
static void put(dxl_json *obj, const char *key, dxl_json *v) {
    if (!obj || obj->type != DXL_JSON_OBJECT) { dxl_json_free(v); return; }
    ssize_t i = find(obj, key);
    if (i >= 0) {
        dxl_json_free(obj->items[i]);
        obj->items[i] = v;
    } else {
        push(obj, dxl_xstrdup(key), v);
    }
}

dxl_json *dxl_json_child(dxl_json *obj, const char *key) {
    dxl_json *c = dxl_json_get(obj, key);
    if (c && c->type == DXL_JSON_OBJECT) return c;
    c = node(DXL_JSON_OBJECT);
    put(obj, key, c);
    return c;
}

const char *dxl_json_get_string(const dxl_json *obj, const char *key, const char *fallback) {
    const dxl_json *v = dxl_json_get(obj, key);
    return v && v->type == DXL_JSON_STRING ? v->text : fallback;
}

int dxl_json_get_bool(const dxl_json *obj, const char *key, int fallback) {
    const dxl_json *v = dxl_json_get(obj, key);
    return v && v->type == DXL_JSON_BOOL ? v->b : fallback;
}

double dxl_json_get_number(const dxl_json *obj, const char *key, double fallback) {
    const dxl_json *v = dxl_json_get(obj, key);
    return v && v->type == DXL_JSON_NUMBER ? v->num : fallback;
}

void dxl_json_set_string(dxl_json *obj, const char *key, const char *value) {
    dxl_json *v = node(DXL_JSON_STRING);
    v->text = dxl_xstrdup(value ? value : "");
    put(obj, key, v);
}

void dxl_json_set_bool(dxl_json *obj, const char *key, int value) {
    dxl_json *v = node(DXL_JSON_BOOL);
    v->b = value ? 1 : 0;
    put(obj, key, v);
}

void dxl_json_set_number(dxl_json *obj, const char *key, double value) {
    /* An unchanged value keeps its spelling (the engine writes "0.150000"
     * for 0.15), so a save does not churn values nobody touched. */
    dxl_json *old = dxl_json_get(obj, key);
    if (old && old->type == DXL_JSON_NUMBER && old->num == value) return;

    char spelling[64];
    if (!isfinite(value)) value = 0;
    if (value == (double)(long long)value && fabs(value) < 1e15) {
        snprintf(spelling, sizeof spelling, "%lld", (long long)value);
    } else {
        /* Shortest spelling that reads back as the same double. */
        for (int prec = 6; prec <= 17; prec++) {
            snprintf(spelling, sizeof spelling, "%.*g", prec, value);
            if (strtod(spelling, NULL) == value) break;
        }
    }
    dxl_json *v = node(DXL_JSON_NUMBER);
    v->num = value;
    v->text = dxl_xstrdup(spelling);
    put(obj, key, v);
}
