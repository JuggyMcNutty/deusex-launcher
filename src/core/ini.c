#include "core/ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    LINE_BLANK,
    LINE_COMMENT,
    LINE_SECTION,
    LINE_PAIR
} line_kind;

typedef struct {
    line_kind kind;
    char *raw;   /* the line as it will be written, without its terminator */
    char *eol;   /* "\r\n", "\n", or "" for a final line with no terminator */
    char *name;  /* LINE_SECTION: section name; LINE_PAIR: key */
    char *value; /* LINE_PAIR only */
} ini_line;

struct dxl_ini {
    ini_line *lines;
    size_t    count, cap;
    char     *eol;   /* what new lines get; inferred from the file */
    int       dirty;
};

static void lines_reserve(dxl_ini *ini, size_t need) {
    if (need <= ini->cap) return;
    size_t cap = ini->cap ? ini->cap : 32;
    while (cap < need) cap *= 2;
    ini->lines = dxl_xrealloc(ini->lines, cap * sizeof *ini->lines);
    ini->cap = cap;
}

static void line_clear(ini_line *l) {
    free(l->raw); free(l->eol); free(l->name); free(l->value);
    memset(l, 0, sizeof *l);
}

/* Inserts a zeroed line at index, shifting the rest down. */
static ini_line *line_insert(dxl_ini *ini, size_t index) {
    lines_reserve(ini, ini->count + 1);
    if (index > ini->count) index = ini->count;
    memmove(ini->lines + index + 1, ini->lines + index,
            (ini->count - index) * sizeof *ini->lines);
    ini->count++;
    memset(ini->lines + index, 0, sizeof *ini->lines);
    return ini->lines + index;
}

static void line_remove(dxl_ini *ini, size_t index) {
    if (index >= ini->count) return;
    line_clear(ini->lines + index);
    memmove(ini->lines + index, ini->lines + index + 1,
            (ini->count - index - 1) * sizeof *ini->lines);
    ini->count--;
}

dxl_ini *dxl_ini_new(void) {
    dxl_ini *ini = dxl_xmalloc(sizeof *ini);
    memset(ini, 0, sizeof *ini);
    ini->eol = dxl_xstrdup("\r\n");   /* the engine's own convention */
    return ini;
}

void dxl_ini_free(dxl_ini *ini) {
    if (!ini) return;
    for (size_t i = 0; i < ini->count; i++) line_clear(ini->lines + i);
    free(ini->lines);
    free(ini->eol);
    free(ini);
}

/* Classifies one line's text (terminator already stripped). */
static void classify(ini_line *l, const char *text, size_t len) {
    l->raw = dxl_xstrndup(text, len);

    char *scan = dxl_xstrndup(text, len);
    char *t = dxl_trim(scan);

    if (*t == '\0') {
        l->kind = LINE_BLANK;
    } else if (*t == ';' || *t == '#') {
        l->kind = LINE_COMMENT;
    } else if (*t == '[') {
        char *close = strrchr(t, ']');
        if (close) {
            l->kind = LINE_SECTION;
            l->name = dxl_xstrndup(t + 1, (size_t)(close - t - 1));
            char *n = dxl_trim(l->name);
            if (n != l->name) memmove(l->name, n, strlen(n) + 1);
        } else {
            l->kind = LINE_COMMENT;   /* malformed: preserve, don't interpret */
        }
    } else {
        char *eq = strchr(t, '=');
        if (eq) {
            l->kind  = LINE_PAIR;
            l->name  = dxl_xstrndup(t, (size_t)(eq - t));
            char *k = dxl_trim(l->name);
            if (k != l->name) memmove(l->name, k, strlen(k) + 1);
            /* The value keeps interior and trailing spaces the engine wrote,
             * but not the padding around '='. Trailing whitespace is already
             * gone because we trimmed the whole line. */
            l->value = dxl_xstrdup(eq + 1);
            char *v = l->value;
            while (*v == ' ' || *v == '\t') v++;
            if (v != l->value) memmove(l->value, v, strlen(v) + 1);
        } else {
            l->kind = LINE_COMMENT;   /* not a pair; preserve verbatim */
        }
    }
    free(scan);
}

dxl_ini *dxl_ini_parse(const char *text, size_t len) {
    dxl_ini *ini = dxl_ini_new();
    int saw_crlf = 0, saw_lf = 0;

    size_t i = 0;
    while (i < len) {
        size_t start = i;
        while (i < len && text[i] != '\n' && text[i] != '\r') i++;
        size_t end = i;

        const char *eol = "";
        if (i < len) {
            if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n') {
                eol = "\r\n"; i += 2; saw_crlf = 1;
            } else if (text[i] == '\r') {
                eol = "\r"; i += 1;
            } else {
                eol = "\n"; i += 1; saw_lf = 1;
            }
        }

        lines_reserve(ini, ini->count + 1);
        ini_line *l = ini->lines + ini->count++;
        memset(l, 0, sizeof *l);
        classify(l, text + start, end - start);
        l->eol = dxl_xstrdup(eol);
    }

    /* New lines follow whatever the file predominantly uses. */
    free(ini->eol);
    ini->eol = dxl_xstrdup((saw_lf && !saw_crlf) ? "\n" : "\r\n");
    return ini;
}

dxl_ini *dxl_ini_load(const char *path, dxl_err *err) {
    FILE *f = fopen(path, "rb");
    if (!f) { dxl_err_set(err, "cannot open %s", path); return NULL; }

    dxl_buf b;
    dxl_buf_init(&b);
    char chunk[8192];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&b, chunk, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) { dxl_buf_free(&b); dxl_err_set(err, "read error on %s", path); return NULL; }

    dxl_ini *ini = dxl_ini_parse(b.data ? b.data : "", b.len);
    dxl_buf_free(&b);
    return ini;
}

char *dxl_ini_render(const dxl_ini *ini, size_t *out_len) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (size_t i = 0; i < ini->count; i++) {
        dxl_buf_puts(&b, ini->lines[i].raw);
        dxl_buf_puts(&b, ini->lines[i].eol);
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    if (out_len) *out_len = b.len;
    return b.data;   /* ownership passes to the caller */
}

int dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err) {
    size_t len;
    char *text = dxl_ini_render(ini, &len);

    /* Write-and-rename, so an interrupted save cannot leave a truncated
     * config behind -- this file is the engine's only persistent state. */
    char *tmp = dxl_xmalloc(strlen(path) + 8);
    sprintf(tmp, "%s.tmpXXX", path);

    FILE *f = fopen(tmp, "wb");
    if (!f) { dxl_err_set(err, "cannot write %s", tmp); free(text); free(tmp); return -1; }
    size_t wrote = len ? fwrite(text, 1, len, f) : 0;
    int bad = (wrote != len) || fflush(f) != 0;
    fclose(f);
    free(text);

    if (bad) { remove(tmp); dxl_err_set(err, "short write on %s", tmp); free(tmp); return -1; }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        dxl_err_set(err, "cannot replace %s", path);
        free(tmp);
        return -1;
    }
    free(tmp);
    return 0;
}

/* ---- lookup --------------------------------------------------------- */

/* Index of the section header line, or (size_t)-1. */
static size_t find_section(const dxl_ini *ini, const char *section) {
    for (size_t i = 0; i < ini->count; i++)
        if (ini->lines[i].kind == LINE_SECTION &&
            dxl_stricmp(ini->lines[i].name, section) == 0)
            return i;
    return (size_t)-1;
}

/* One past the last line belonging to the section starting at header. */
static size_t section_end(const dxl_ini *ini, size_t header) {
    size_t i = header + 1;
    while (i < ini->count && ini->lines[i].kind != LINE_SECTION) i++;
    return i;
}

/* Insertion point for a new key: after the section's last pair line, so new
 * keys land with their fellows rather than after trailing blank lines. */
static size_t section_insert_at(const dxl_ini *ini, size_t header) {
    size_t end = section_end(ini, header), at = header + 1;
    for (size_t i = header + 1; i < end; i++)
        if (ini->lines[i].kind == LINE_PAIR) at = i + 1;
    return at;
}

static size_t find_pair(const dxl_ini *ini, const char *section, const char *key,
                        size_t from) {
    size_t header = find_section(ini, section);
    if (header == (size_t)-1) return (size_t)-1;
    size_t end = section_end(ini, header);
    for (size_t i = (from > header) ? from : header + 1; i < end; i++)
        if (ini->lines[i].kind == LINE_PAIR &&
            dxl_stricmp(ini->lines[i].name, key) == 0)
            return i;
    return (size_t)-1;
}

const char *dxl_ini_get(const dxl_ini *ini, const char *section, const char *key) {
    size_t i = find_pair(ini, section, key, 0);
    return (i == (size_t)-1) ? NULL : ini->lines[i].value;
}

size_t dxl_ini_get_all(const dxl_ini *ini, const char *section, const char *key,
                       const char **out, size_t max) {
    size_t header = find_section(ini, section);
    if (header == (size_t)-1) return 0;
    size_t end = section_end(ini, header), n = 0;
    for (size_t i = header + 1; i < end; i++)
        if (ini->lines[i].kind == LINE_PAIR &&
            dxl_stricmp(ini->lines[i].name, key) == 0) {
            if (out && n < max) out[n] = ini->lines[i].value;
            n++;
        }
    return n;
}

int dxl_ini_get_int(const dxl_ini *ini, const char *section, const char *key,
                    int fallback) {
    const char *v = dxl_ini_get(ini, section, key);
    if (!v || !*v) return fallback;
    char *end;
    long n = strtol(v, &end, 10);
    return (end == v) ? fallback : (int)n;
}

int dxl_ini_get_bool(const dxl_ini *ini, const char *section, const char *key,
                     int fallback) {
    const char *v = dxl_ini_get(ini, section, key);
    if (!v || !*v) return fallback;
    if (dxl_stricmp(v, "True") == 0 || dxl_stricmp(v, "1") == 0) return 1;
    if (dxl_stricmp(v, "False") == 0 || dxl_stricmp(v, "0") == 0) return 0;
    return fallback;
}

/* ---- mutation ------------------------------------------------------- */

/* Rewrites a line as key=value. eol may be NULL to keep whatever the line
 * already had; callers that are regenerating a line pass the file's dominant
 * terminator, so a line we touch ends up looking like the rest of the file
 * rather than keeping a stray ending some other tool left behind. */
static void pair_write(ini_line *l, const char *key, const char *value,
                       const char *eol) {
    free(l->raw); free(l->name); free(l->value);
    if (eol) { free(l->eol); l->eol = dxl_xstrdup(eol); }
    l->kind  = LINE_PAIR;
    l->name  = dxl_xstrdup(key);
    l->value = dxl_xstrdup(value ? value : "");
    size_t n = strlen(key) + 1 + strlen(l->value) + 1;
    l->raw = dxl_xmalloc(n);
    snprintf(l->raw, n, "%s=%s", key, l->value);
}

/* Ensures the section exists, returning its header index. */
static size_t ensure_section(dxl_ini *ini, const char *section) {
    size_t header = find_section(ini, section);
    if (header != (size_t)-1) return header;

    /* A final line with no terminator would run into whatever we append, so
     * terminate it first -- before adding the separator, or the separator is
     * what gets swallowed. */
    if (ini->count && ini->lines[ini->count - 1].eol[0] == '\0') {
        free(ini->lines[ini->count - 1].eol);
        ini->lines[ini->count - 1].eol = dxl_xstrdup(ini->eol);
    }
    /* Separate from whatever precedes, matching the file's own style. */
    if (ini->count && ini->lines[ini->count - 1].kind != LINE_BLANK) {
        ini_line *blank = line_insert(ini, ini->count);
        blank->kind = LINE_BLANK;
        blank->raw  = dxl_xstrdup("");
        blank->eol  = dxl_xstrdup(ini->eol);
    }

    header = ini->count;
    ini_line *l = line_insert(ini, header);
    l->kind = LINE_SECTION;
    l->name = dxl_xstrdup(section);
    size_t n = strlen(section) + 3;
    l->raw = dxl_xmalloc(n);
    snprintf(l->raw, n, "[%s]", section);
    l->eol = dxl_xstrdup(ini->eol);
    return header;
}

void dxl_ini_set(dxl_ini *ini, const char *section, const char *key,
                 const char *value) {
    size_t i = find_pair(ini, section, key, 0);
    if (i != (size_t)-1) {
        if (strcmp(ini->lines[i].value, value ? value : "") == 0) return;
        pair_write(ini->lines + i, key, value, ini->eol);
        ini->dirty = 1;
        return;
    }
    dxl_ini_append(ini, section, key, value);
}

void dxl_ini_append(dxl_ini *ini, const char *section, const char *key,
                    const char *value) {
    size_t header = ensure_section(ini, section);
    ini_line *l = line_insert(ini, section_insert_at(ini, header));
    pair_write(l, key, value, ini->eol);
    ini->dirty = 1;
}

void dxl_ini_set_int(dxl_ini *ini, const char *section, const char *key, int value) {
    char tmp[32];
    snprintf(tmp, sizeof tmp, "%d", value);
    dxl_ini_set(ini, section, key, tmp);
}

void dxl_ini_set_bool(dxl_ini *ini, const char *section, const char *key, int value) {
    dxl_ini_set(ini, section, key, value ? "True" : "False");
}

int dxl_ini_has_section(const dxl_ini *ini, const char *section) {
    return find_section(ini, section) != (size_t)-1;
}

void dxl_ini_empty_section(dxl_ini *ini, const char *section) {
    size_t header = find_section(ini, section);
    if (header == (size_t)-1) return;
    size_t i = header + 1;
    while (i < ini->count && ini->lines[i].kind != LINE_SECTION) {
        if (ini->lines[i].kind == LINE_PAIR) { line_remove(ini, i); ini->dirty = 1; }
        else i++;
    }
}

int dxl_ini_dirty(const dxl_ini *ini) { return ini->dirty; }

void dxl_ini_overlay(dxl_ini *dst, const dxl_ini *src) {
    const char *section = "";
    for (size_t i = 0; i < src->count; i++) {
        const ini_line *l = src->lines + i;
        if (l->kind == LINE_SECTION) section = l->name;
        else if (l->kind == LINE_PAIR) dxl_ini_set(dst, section, l->name, l->value);
    }
}
