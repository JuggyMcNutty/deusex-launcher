#include "core/ini.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

typedef enum {
    LINE_BLANK,
    LINE_NONE,   /* neither a header nor a pair: read as nothing, kept */
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
    /* A line after the file's last, unterminated line gets its own line:
     * joined onto it, "[A]\r\nx=1" with y=2 appended read "x=1y=2". */
    if (index > 0 && ini->lines[index - 1].eol && ini->lines[index - 1].eol[0] == '\0') {
        free(ini->lines[index - 1].eol);
        ini->lines[index - 1].eol = dxl_xstrdup(ini->eol);
    }
    memmove(ini->lines + index + 1, ini->lines + index,
            (ini->count - index) * sizeof *ini->lines);
    ini->count++;
    memset(ini->lines + index, 0, sizeof *ini->lines);
    return ini->lines + index;
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

/* Classifies one line's text (terminator already stripped) as the
 * original's reader takes it (FConfigFile::Read, GOG DeusEx.exe 0x10916780;
 * core-dll.md, configuration): a header a line that begins [ and ends ],
 * else a key up to its first = and its value after it, nothing trimmed, a
 * value that begins and ends with " (a lone " too) without them. A line
 * with no =, one beginning ; or # among them, is none (a line of blanks a
 * blank line). */
static void classify(ini_line *l, const char *text, size_t len) {
    l->raw = dxl_xstrndup(text, len);
    l->raw_len = len;

    const char *t = l->raw;   /* up to a NUL */
    size_t n = strlen(t);
    const char *eq = strchr(t, '=');
    if (n >= 2 && t[0] == '[' && t[n - 1] == ']') {
        l->kind = LINE_SECTION;
        l->name = dxl_xstrndup(t + 1, n - 2);
    } else if (eq) {
        l->kind = LINE_PAIR;
        l->name = dxl_xstrndup(t, (size_t)(eq - t));
        const char *v = eq + 1;
        size_t vn = strlen(v);
        if (vn && v[0] == '"' && v[vn - 1] == '"')
            l->value = dxl_xstrndup(v + 1, vn > 1 ? vn - 2 : 0);
        else
            l->value = dxl_xstrdup(v);
    } else {
        char *scan = dxl_xstrdup(t);
        l->kind = *dxl_trim(scan) ? LINE_NONE : LINE_BLANK;
        free(scan);
    }
}

/* ---- the file's text ------------------------------------------------ */

/* A character onto b as UTF-8; a surrogate without its pair as three bytes
 * of its own, so it goes back as it came. */
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

/* The character of UTF-8 text at *i, *i moved past it: a byte that begins
 * no whole sequence is its Latin-1 character. */
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

/* A file's bytes as UTF-8 text, as the original's appLoadFileToString
 * (Core.dll 0x101488c0) reads them: a file of even size beginning FF FE
 * UTF-16 little-endian, FE FF big-endian, the mark dropped, a surrogate
 * pair one character; any other a byte a character, Latin-1 (a UTF-8 mark
 * too, as the original takes it). */
static char *text_of(const unsigned char *u, size_t len, size_t *out_len) {
    dxl_buf b;
    dxl_buf_init(&b);
    if (len >= 2 && len % 2 == 0 && ((u[0] == 0xff && u[1] == 0xfe) || (u[0] == 0xfe && u[1] == 0xff))) {
        int big = u[0] == 0xfe;
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
        for (size_t i = 0; i < len; i++) put_utf8(&b, u[i]);
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    *out_len = b.len;
    return b.data;
}

static void put_unit(dxl_buf *b, unsigned c) {
    char s[2] = { (char)(c & 0xff), (char)(c >> 8 & 0xff) };
    dxl_buf_add(b, s, 2);
}

/* UTF-8 text as the file's bytes, as the original's appSaveStringToFile
 * (Core.dll 0x10148c40) writes them, by the characters whatever the file
 * was read as: a byte a character when every one is below U+0100, else
 * UTF-16 little-endian after FF FE. */
static char *file_bytes(const char *text, size_t len, size_t *out_len) {
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

dxl_ini *dxl_ini_parse(const char *bytes, size_t len) {
    dxl_ini *ini = dxl_ini_new();
    int saw_crlf = 0, saw_lf = 0;
    /* The file's text as the original reads it, UTF-8 in memory (bugs.md
     * MAIN.9): a UTF-16 ini is no longer bytes with NULs among them. */
    char *text = text_of((const unsigned char *)bytes, len, &len);

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
    /* Written as the original's appSaveStringToFile writes it (MAIN.9). */
    size_t n;
    char *bytes = file_bytes(b.data, b.len, &n);
    dxl_buf_free(&b);
    if (out_len) *out_len = n;
    return bytes;   /* ownership passes to the caller */
}

int dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err) {
    /* The fresh file takes the ini's place: a linked ini's target, not
     * the link (which became a plain file, its target left stale), and
     * never a read-only one, a UE1 player's way of keeping settings, which
     * the original's write in place refuses. */
    char *target = NULL;
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) target = realpath(path, NULL);
        if (access(target ? target : path, W_OK) != 0) {
            dxl_err_set(err, "%s is read-only", path);
            free(target);
            return -1;
        }
    }
    const char *dest = target ? target : path;

    /* Write-and-rename, so an interrupted save cannot leave a truncated
     * config behind -- this file is the engine's only persistent state: on
     * a name of its own beside it, so two launchers saving at once never
     * write one file, its bytes on the disk before the rename, and the
     * ini's mode kept (a new one's as fopen would make it). */
    size_t len;
    char *text = dxl_ini_render(ini, &len);
    char *tmp = dxl_xmalloc(strlen(dest) + 8);
    sprintf(tmp, "%s.XXXXXX", dest);
    int fd = mkstemp(tmp);
    if (fd < 0) {
        dxl_err_set(err, "cannot write beside %s", dest);
        free(text);
        free(tmp);
        free(target);
        return -1;
    }
    size_t wrote = 0;
    while (wrote < len) {
        ssize_t w = write(fd, text + wrote, len - wrote);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) break;
        wrote += (size_t)w;
    }
    mode_t mask = umask(0);
    umask(mask);
    struct stat old;
    mode_t mode = stat(dest, &old) == 0 ? (old.st_mode & 07777) : (0666 & ~mask);
    /* A filesystem with no modes to keep (exFAT) refusing them is no failure */
    int moded = fchmod(fd, mode) == 0 || errno == EPERM || errno == EOPNOTSUPP;
    int bad = wrote != len || !moded || fsync(fd) != 0;
    bad = close(fd) != 0 || bad;
    free(text);
    if (bad) {
        remove(tmp);
        dxl_err_set(err, "short write on %s", tmp);
        free(tmp);
        free(target);
        return -1;
    }
    if (rename(tmp, dest) != 0) {
        remove(tmp);
        dxl_err_set(err, "cannot replace %s", dest);
        free(target);
        free(tmp);
        return -1;
    }
    free(target);
    free(tmp);
    return 0;
}

/* ---- lookup --------------------------------------------------------- */

/* A section written twice is one section to the original, its blocks read
 * in order, and a key given twice is read at its last value (core-dll.md;
 * the engine alike), so they are to these lookups (bugs.md MAIN.10). */

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

/* The line of the key's value the original reads, the last of the key in
 * the section's blocks, or (size_t)-1. */
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
    l->raw_len = n - 1;
}

/* Ensures the section exists, returning the header index of its last block,
 * the one a new key joins. */
static size_t ensure_section(dxl_ini *ini, const char *section) {
    size_t header = last_block(ini, section);
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

int dxl_ini_dirty(const dxl_ini *ini) { return ini->dirty; }
