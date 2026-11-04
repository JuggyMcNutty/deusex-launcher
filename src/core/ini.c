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
    int       game;  /* the game's: read and written as the engine does; else the launcher's own UTF-8 */
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

static dxl_ini *ini_new(int game) {
    dxl_ini *ini = dxl_xmalloc(sizeof *ini);
    memset(ini, 0, sizeof *ini);
    ini->eol = dxl_xstrdup("\r\n");   /* the engine's own convention */
    ini->game = game;
    return ini;
}

dxl_ini *dxl_ini_new(void) { return ini_new(1); }
dxl_ini *dxl_ini_new_utf8(void) { return ini_new(0); }

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

/* ---- the file's text ------------------------------------------------ */

/* A character onto b as UTF-8; a surrogate without its pair as three bytes
 * of its own, as DXEngine's ScriptToUTF8 gives one, so it goes back as it
 * came. */
static void put_utf8(dxl_buf *b, unsigned c) {
    char s[4];
    size_t n = 1;
    if (c < 0x80) {
        s[0] = (char)c;
    } else if (c < 0x800) {
        s[0] = (char)(0xc0 | c >> 6);
        s[1] = (char)(0x80 | (c & 0x3f));
        n = 2;
    } else if (c < 0x10000) {
        s[0] = (char)(0xe0 | c >> 12);
        s[1] = (char)(0x80 | (c >> 6 & 0x3f));
        s[2] = (char)(0x80 | (c & 0x3f));
        n = 3;
    } else {
        s[0] = (char)(0xf0 | c >> 18);
        s[1] = (char)(0x80 | (c >> 12 & 0x3f));
        s[2] = (char)(0x80 | (c >> 6 & 0x3f));
        s[3] = (char)(0x80 | (c & 0x3f));
        n = 4;
    }
    dxl_buf_add(b, s, n);
}

/* The character of UTF-8 text at *i, *i moved past it, as DXEngine's
 * UTF8ToScript takes one: a byte that begins no whole sequence is its
 * Latin-1 character. */
static unsigned next_char(const unsigned char *s, size_t len, size_t *i) {
    unsigned c = s[*i];
    int extra = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
    int whole = extra > 0 && *i + (size_t)extra < len;
    unsigned cp = whole ? c & (0x3fu >> extra) : c;
    for (int k = 1; whole && k <= extra; k++) {
        unsigned next = s[*i + (size_t)k];
        whole = (next & 0xc0) == 0x80;
        cp = cp << 6 | (next & 0x3f);
    }
    if (!whole) {
        (*i)++;
        return c;
    }
    *i += (size_t)extra + 1;
    return cp;
}

/* A file's bytes as UTF-8 text; *how 2 for UTF-16, 1 after a UTF-8 mark,
 * else 0. UTF-16 by its mark, the file of even size -- little-endian after
 * FF FE, big-endian after FE FF, the mark dropped, a surrogate pair one
 * character -- as the original's appLoadFileToString (Core.dll 0x101488c0)
 * and DXEngine's IniFile read it. Any other file a byte a character,
 * Latin-1, as they read it, the game's (game); the launcher's own as it is,
 * UTF-8. A UTF-8 mark (an editor's) is passed over, the text after it read
 * the same way. */
static char *text_of(const unsigned char *u, size_t len, int game, int *how, size_t *out_len) {
    dxl_buf b;
    dxl_buf_init(&b);
    *how = 0;
    if (len >= 2 && len % 2 == 0 && ((u[0] == 0xff && u[1] == 0xfe) || (u[0] == 0xfe && u[1] == 0xff))) {
        int big = u[0] == 0xfe;
        *how = 2;
        for (size_t i = 2; i + 1 < len; i += 2) {
            unsigned c = big ? (unsigned)(u[i] << 8 | u[i + 1]) : (unsigned)(u[i + 1] << 8 | u[i]);
            if (c >= 0xd800 && c < 0xdc00 && i + 3 < len) {
                unsigned low = big ? (unsigned)(u[i + 2] << 8 | u[i + 3]) : (unsigned)(u[i + 3] << 8 | u[i + 2]);
                if (low >= 0xdc00 && low < 0xe000) {
                    c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                    i += 2;
                }
            }
            put_utf8(&b, c);
        }
    } else {
        size_t from = 0;
        if (len >= 3 && u[0] == 0xef && u[1] == 0xbb && u[2] == 0xbf) {
            *how = 1;
            from = 3;
        }
        if (game)
            for (size_t i = from; i < len; i++) put_utf8(&b, u[i]);
        else
            dxl_buf_add(&b, (const char *)u + from, len - from);
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    *out_len = b.len;
    return b.data;
}

static void put_unit(dxl_buf *b, unsigned c) {
    char s[2] = { (char)(c & 0xff), (char)(c >> 8 & 0xff) };
    dxl_buf_add(b, s, 2);
}

/* UTF-8 text as the game's file, by its characters whatever it was read
 * as: a byte a character when every one is below U+0100, else UTF-16
 * little-endian after FF FE -- the original's appSaveStringToFile
 * (Core.dll 0x10148c40) and DXEngine's IniFile. */
static char *game_bytes(const char *text, size_t len, size_t *out_len) {
    const unsigned char *s = (const unsigned char *)text;
    int wide = 0;
    for (size_t i = 0; i < len && !wide;) wide = next_char(s, len, &i) > 0xff;
    dxl_buf b;
    dxl_buf_init(&b);
    if (wide) dxl_buf_add(&b, "\xff\xfe", 2);
    for (size_t i = 0; i < len;) {
        unsigned c = next_char(s, len, &i);
        if (!wide) {
            char byte = (char)c;
            dxl_buf_add(&b, &byte, 1);
        } else if (c >= 0x10000) {
            put_unit(&b, 0xd800 + ((c - 0x10000) >> 10));
            put_unit(&b, 0xdc00 + ((c - 0x10000) & 0x3ff));
        } else {
            put_unit(&b, c);
        }
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    *out_len = b.len;
    return b.data;
}

static dxl_ini *parse(const char *bytes, size_t len, int game) {
    dxl_ini *ini = ini_new(game);
    int saw_crlf = 0, saw_lf = 0;

    /* UTF-8 in memory. A game ini's UTF-16 is kept, the engine reading it,
     * written as the engine writes one (bugs.md LCORE.8); the launcher's
     * own is written UTF-8 at the next save, which the file is marked for.
     * A UTF-8 byte-order mark (an editor's) is gone at the next save, which
     * the file is marked for: the engine's reader takes it as text, the
     * section after it lost to it. */
    int how;
    char *text = text_of((const unsigned char *)bytes, len, game, &how, &len);
    if (how == 1 || (how == 2 && !game)) ini->dirty = 1;

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
    free(text);
    return ini;
}

dxl_ini *dxl_ini_parse(const char *bytes, size_t len) { return parse(bytes, len, 1); }
dxl_ini *dxl_ini_parse_utf8(const char *text, size_t len) { return parse(text, len, 0); }

static dxl_ini *load(const char *path, int game, dxl_err *err) {
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

    dxl_ini *ini = parse(b.data ? b.data : "", b.len, game);
    dxl_buf_free(&b);
    return ini;
}

dxl_ini *dxl_ini_load(const char *path, dxl_err *err) { return load(path, 1, err); }
dxl_ini *dxl_ini_load_utf8(const char *path, dxl_err *err) { return load(path, 0, err); }

char *dxl_ini_render(const dxl_ini *ini, size_t *out_len) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (size_t i = 0; i < ini->count; i++) {
        dxl_buf_add(&b, ini->lines[i].raw, ini->lines[i].raw_len);
        dxl_buf_puts(&b, ini->lines[i].eol);
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    if (ini->game) {
        size_t n;
        char *bytes = game_bytes(b.data, b.len, &n);
        dxl_buf_free(&b);
        if (out_len) *out_len = n;
        return bytes;
    }
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

/* The index of the key's first line in the section's blocks, or
 * (size_t)-1. */
static size_t first_pair(const dxl_ini *ini, const char *section, const char *key) {
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t end = section_end(ini, h);
        for (size_t i = h + 1; i < end; i++)
            if (ini->lines[i].kind == LINE_PAIR && dxl_stricmp(ini->lines[i].name, key) == 0) return i;
    }
    return (size_t)-1;
}

/* The key's lines in the section made the n values, in their order: in the
 * place of its first line, its others removed, or appended to the
 * section's last block where it has none. */
static void set_all(dxl_ini *ini, const char *section, const char *key, const char **values, size_t n) {
    size_t have = dxl_ini_get_all(ini, section, key, NULL, 0);
    if (have == n) {
        const char **now = dxl_xmalloc(n * sizeof *now);
        dxl_ini_get_all(ini, section, key, now, n);
        size_t same = 0;
        while (same < n && strcmp(now[same], values[same]) == 0) same++;
        free(now);
        if (same == n) return;
    }
    size_t first = first_pair(ini, section, key);
    if (first == (size_t)-1) {
        for (size_t k = 0; k < n; k++) dxl_ini_append(ini, section, key, values[k]);
        return;
    }
    /* The first is the file's earliest of them: every other comes after. */
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t i = h + 1;
        while (i < ini->count && ini->lines[i].kind != LINE_SECTION) {
            if (i != first && ini->lines[i].kind == LINE_PAIR && dxl_stricmp(ini->lines[i].name, key) == 0)
                line_remove(ini, i);
            else
                i++;
        }
    }
    pair_write(ini->lines + first, key, values[0], ini->eol);
    for (size_t k = 1; k < n; k++) pair_write(line_insert(ini, first + k), key, values[k], ini->eol);
    ini->dirty = 1;
}

/* A value's characters as bytes, when each is below U+0100 and they make
 * UTF-8 past ASCII: the UTF-8 an earlier launcher wrote, which the engine
 * reads a byte a character. Else NULL. */
static char *utf8_read_as_latin1(const char *v) {
    const unsigned char *s = (const unsigned char *)v;
    size_t len = strlen(v), n = 0;
    char *bytes = dxl_xmalloc(len + 1);
    int past_ascii = 0;
    for (size_t i = 0; i < len;) {
        unsigned c = next_char(s, len, &i);
        if (c > 0xff) {
            free(bytes);
            return NULL;
        }
        past_ascii |= c >= 0x80;
        bytes[n++] = (char)c;
    }
    bytes[n] = '\0';
    for (size_t i = 0; past_ascii && i < n;) {
        size_t k = dxl_utf8_char(bytes + i);
        if (!k) past_ascii = 0;
        i += k;
    }
    if (past_ascii) return bytes;
    free(bytes);
    return NULL;
}

size_t dxl_ini_undo_utf8(dxl_ini *ini, const char *section, const char *key) {
    size_t done = 0;
    if (!ini->game) return 0;
    for (size_t h = find_section(ini, section); h != (size_t)-1; h = next_block_after(ini, section, h)) {
        size_t end = section_end(ini, h);
        for (size_t i = h + 1; i < end; i++) {
            ini_line *l = ini->lines + i;
            if (l->kind != LINE_PAIR || dxl_stricmp(l->name, key) != 0) continue;
            char *was = utf8_read_as_latin1(l->value);
            if (!was) continue;
            pair_write(l, l->name, was, ini->eol);   /* copies both first */
            free(was);
            done++;
        }
    }
    if (done) ini->dirty = 1;
    return done;
}

void dxl_ini_overlay(dxl_ini *dst, const dxl_ini *src) {
    const char *section = NULL;
    for (size_t i = 0; i < src->count; i++) {
        const ini_line *l = src->lines + i;
        if (l->kind == LINE_SECTION) section = l->name;
        /* Each key once, at its first line; none above the first header,
         * which the engine reads in no section. */
        if (l->kind != LINE_PAIR || !section || first_pair(src, section, l->name) != i) continue;
        size_t n = dxl_ini_get_all(src, section, l->name, NULL, 0);
        const char **values = dxl_xmalloc(n * sizeof *values);
        dxl_ini_get_all(src, section, l->name, values, n);
        set_all(dst, section, l->name, values, n);
        free(values);
    }
}
