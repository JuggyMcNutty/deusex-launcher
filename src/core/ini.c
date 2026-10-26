#include "core/ini.h"
#include "core/paths.h"

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
    size_t raw_len;   /* its bytes, a NUL among them kept: read up to one, written whole */
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
    l->raw_len = len;

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

/* The game's 8-bit text of UTF-16 bytes that begin with their byte-order
 * mark: a unit below 256 is its byte, any other character a '?' (one for a
 * surrogate pair), an odd last byte dropped. */
static char *from_utf16(const char *text, size_t len, size_t *out_len) {
    const unsigned char *u = (const unsigned char *)text;
    int big = u[0] == 0xfe;
    char *out = dxl_xmalloc(len / 2 + 1);
    size_t n = 0;
    for (size_t i = 2; i + 1 < len; i += 2) {
        unsigned unit = big ? (unsigned)((u[i] << 8) | u[i + 1]) : (unsigned)((u[i + 1] << 8) | u[i]);
        if (unit >= 0xd800 && unit < 0xdc00 && i + 3 < len) i += 2;   /* the pair's second unit */
        out[n++] = unit < 0x100 ? (char)unit : '?';
    }
    out[n] = '\0';
    *out_len = n;
    return out;
}

dxl_ini *dxl_ini_parse(const char *text, size_t len) {
    dxl_ini *ini = dxl_ini_new();
    int saw_crlf = 0, saw_lf = 0;

    /* Notepad's "Unicode" (UTF-16 by its byte-order mark) is read as the
     * original reads it (Core's appLoadFileToString takes either byte
     * order), its text made the game's 8-bit one, and written so at the
     * next save, which the file is marked for. */
    char *narrow = NULL;
    const unsigned char *u = (const unsigned char *)text;
    if (len >= 2 && ((u[0] == 0xff && u[1] == 0xfe) || (u[0] == 0xfe && u[1] == 0xff))) {
        narrow = from_utf16(text, len, &len);
        text = narrow;
        ini->dirty = 1;
    }

    /* A UTF-8 byte-order mark (an editor's) is passed over, and gone at the
     * next save, which the file is marked for: the engine's reader takes
     * none, the section after it lost to it. */
    if (len >= 3 && (unsigned char)text[0] == 0xef && (unsigned char)text[1] == 0xbb &&
        (unsigned char)text[2] == 0xbf) {
        text += 3;
        len -= 3;
        ini->dirty = 1;
    }

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
    free(narrow);
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
        dxl_buf_add(&b, ini->lines[i].raw, ini->lines[i].raw_len);
        dxl_buf_puts(&b, ini->lines[i].eol);
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    if (out_len) *out_len = b.len;
    return b.data;   /* ownership passes to the caller */
}

int dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err) {
    size_t len;
    char *text = dxl_ini_render(ini, &len);
    /* Whole or not at all: this file is the engine's only persistent state. */
    int rc = dxl_path_write_atomic(path, text, len, err);
    free(text);
    return rc;
}

void dxl_ini_set_eol(dxl_ini *ini, const char *eol) {
    for (size_t i = 0; i < ini->count; i++) {
        ini_line *l = ini->lines + i;
        if (!l->eol[0] && i + 1 == ini->count) continue;   /* an unterminated last line stays so */
        if (strcmp(l->eol, eol) == 0) continue;
        free(l->eol);
        l->eol = dxl_xstrdup(eol);
        ini->dirty = 1;
    }
    free(ini->eol);
    ini->eol = dxl_xstrdup(eol);
}

/* ---- lookup --------------------------------------------------------- */

/* A section written twice is one section to the engine, its blocks read in
 * order, and a key given twice is read at its last value (as the original
 * reads them: dx-reverse-info's core-dll.md), so they are to these lookups. */

/* Index of the section's next header line at or after from, or (size_t)-1. */
static size_t next_block(const dxl_ini *ini, const char *section, size_t from) {
    for (size_t i = from; i < ini->count; i++)
        if (ini->lines[i].kind == LINE_SECTION &&
            dxl_stricmp(ini->lines[i].name, section) == 0)
            return i;
    return (size_t)-1;
}

/* Index of the section's first header line, or (size_t)-1. */
static size_t find_section(const dxl_ini *ini, const char *section) {
    return next_block(ini, section, 0);
}

/* One past the last line belonging to the block starting at header. */
static size_t section_end(const dxl_ini *ini, size_t header) {
    size_t i = header + 1;
    while (i < ini->count && ini->lines[i].kind != LINE_SECTION) i++;
    return i;
}

/* The section's header after the block at header, or (size_t)-1: the step
 * of a loop over a section's blocks. */
static size_t next_block_after(const dxl_ini *ini, const char *section, size_t header) {
    return next_block(ini, section, section_end(ini, header));
}

/* Index of the section's last header line, or (size_t)-1. */
static size_t last_block(const dxl_ini *ini, const char *section) {
    size_t last = (size_t)-1;
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h))
        last = h;
    return last;
}

/* Insertion point for a new key: after the section's last pair line, so new
 * keys land with their fellows rather than after trailing blank lines. */
static size_t section_insert_at(const dxl_ini *ini, size_t header) {
    size_t end = section_end(ini, header), at = header + 1;
    for (size_t i = header + 1; i < end; i++)
        if (ini->lines[i].kind == LINE_PAIR) at = i + 1;
    return at;
}

/* The line of the key's value the engine reads, the last of the key in the
 * section's blocks, or (size_t)-1. */
static size_t find_pair(const dxl_ini *ini, const char *section, const char *key) {
    size_t found = (size_t)-1;
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t end = section_end(ini, h);
        for (size_t i = h + 1; i < end; i++)
            if (ini->lines[i].kind == LINE_PAIR &&
                dxl_stricmp(ini->lines[i].name, key) == 0)
                found = i;
    }
    return found;
}

const char *dxl_ini_get(const dxl_ini *ini, const char *section, const char *key) {
    size_t i = find_pair(ini, section, key);
    return (i == (size_t)-1) ? NULL : ini->lines[i].value;
}

size_t dxl_ini_get_all(const dxl_ini *ini, const char *section, const char *key,
                       const char **out, size_t max) {
    size_t n = 0;
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t end = section_end(ini, h);
        for (size_t i = h + 1; i < end; i++)
            if (ini->lines[i].kind == LINE_PAIR &&
                dxl_stricmp(ini->lines[i].name, key) == 0) {
                if (out && n < max) out[n] = ini->lines[i].value;
                n++;
            }
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
    int b = dxl_text_bool(v);
    return b < 0 ? fallback : b;
}

/* ---- mutation ------------------------------------------------------- */

/* Rewrites a line as key=value. eol may be NULL to keep whatever the line
 * already had; callers that are regenerating a line pass the file's dominant
 * terminator, so a line we touch ends up looking like the rest of the file
 * rather than keeping a stray ending some other tool left behind. */
static void pair_write(ini_line *l, const char *key, const char *value,
                       const char *eol) {
    /* key and value may be the line's own strings (a name dxl_ini_pairs
     * gave): copied before the line's go. */
    char *name = dxl_xstrdup(key), *text = dxl_xstrdup(value ? value : "");
    free(l->raw); free(l->name); free(l->value);
    if (eol) { free(l->eol); l->eol = dxl_xstrdup(eol); }
    l->kind  = LINE_PAIR;
    l->name  = name;
    l->value = text;
    size_t n = strlen(name) + 1 + strlen(text) + 1;
    l->raw = dxl_xmalloc(n);
    snprintf(l->raw, n, "%s=%s", name, text);
    l->raw_len = n - 1;
}

/* A line inserted at index after the file's unterminated last line would
 * run onto it: that line is terminated first -- before a separator is
 * added, or the separator is what gets swallowed. */
static void terminate_before(dxl_ini *ini, size_t at) {
    if (at && at == ini->count && ini->lines[at - 1].eol[0] == '\0') {
        free(ini->lines[at - 1].eol);
        ini->lines[at - 1].eol = dxl_xstrdup(ini->eol);
    }
}

/* Ensures the section exists, returning the header index of its last block,
 * the one a new key joins. */
static size_t ensure_section(dxl_ini *ini, const char *section) {
    size_t header = last_block(ini, section);
    if (header != (size_t)-1) return header;

    terminate_before(ini, ini->count);
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
    l->raw_len = n - 1;
    l->eol = dxl_xstrdup(ini->eol);
    return header;
}

void dxl_ini_set(dxl_ini *ini, const char *section, const char *key,
                 const char *value) {
    size_t i = find_pair(ini, section, key);
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
    size_t at = section_insert_at(ini, header);
    terminate_before(ini, at);
    ini_line *l = line_insert(ini, at);
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

void dxl_ini_copy_section(dxl_ini *dst, const dxl_ini *src, const char *section) {
    if (find_section(src, section) == (size_t)-1) return;
    ensure_section(dst, section);
    dst->dirty = 1;
    for (size_t h = find_section(src, section); h != (size_t)-1; h = next_block_after(src, section, h)) {
        size_t end = section_end(src, h);
        for (size_t i = h + 1; i < end; i++)
            if (src->lines[i].kind == LINE_PAIR)
                dxl_ini_append(dst, section, src->lines[i].name, src->lines[i].value);
    }
}

int dxl_ini_has_section(const dxl_ini *ini, const char *section) {
    return find_section(ini, section) != (size_t)-1;
}

size_t dxl_ini_pairs(const dxl_ini *ini, const char *section, const char **keys, const char **values, size_t max) {
    size_t n = 0;
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t end = section_end(ini, h);
        for (size_t i = h + 1; i < end; i++) {
            if (ini->lines[i].kind != LINE_PAIR) continue;
            if (n < max) {
                if (keys) keys[n] = ini->lines[i].name;
                if (values) values[n] = ini->lines[i].value;
            }
            n++;
        }
    }
    return n;
}

size_t dxl_ini_sections(const dxl_ini *ini, const char *prefix, const char **out, size_t max) {
    size_t n = 0, plen = strlen(prefix);
    for (size_t i = 0; i < ini->count; i++) {
        const ini_line *l = &ini->lines[i];
        if (l->kind != LINE_SECTION || dxl_strnicmp(l->name, prefix, plen) != 0) continue;
        /* A section written twice is one section to its lookups. */
        if (find_section(ini, l->name) != i) continue;
        if (out && n < max) out[n] = l->name;
        n++;
    }
    return n;
}

void dxl_ini_empty_section(dxl_ini *ini, const char *section) {
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block(ini, section, h + 1)) {
        size_t i = h + 1;
        while (i < ini->count && ini->lines[i].kind != LINE_SECTION) {
            if (ini->lines[i].kind == LINE_PAIR) { line_remove(ini, i); ini->dirty = 1; }
            else i++;
        }
    }
}

void dxl_ini_remove(dxl_ini *ini, const char *section, const char *key) {
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block(ini, section, h + 1)) {
        size_t i = h + 1;
        while (i < ini->count && ini->lines[i].kind != LINE_SECTION) {
            if (ini->lines[i].kind == LINE_PAIR && dxl_stricmp(ini->lines[i].name, key) == 0) {
                line_remove(ini, i);
                ini->dirty = 1;
            } else {
                i++;
            }
        }
    }
}

int dxl_ini_dirty(const dxl_ini *ini) { return ini->dirty; }
void dxl_ini_clean(dxl_ini *ini) { ini->dirty = 0; }

void dxl_ini_overlay(dxl_ini *dst, const dxl_ini *src) {
    const char *section = "";
    for (size_t i = 0; i < src->count; i++) {
        const ini_line *l = src->lines + i;
        if (l->kind == LINE_SECTION) section = l->name;
        else if (l->kind == LINE_PAIR) dxl_ini_set(dst, section, l->name, l->value);
    }
}
