#include "core/padmap.h"
#include "core/paths.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const dxl_padctl_names[DXL_PADCTL_COUNT] = {
    "a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick", "leftshoulder",
    "rightshoulder", "dpup", "dpdown", "dpleft", "dpright", "leftx", "lefty", "rightx", "righty",
    "lefttrigger", "righttrigger",
};

void dxl_padsrc_format(const dxl_padsrc *s, char *out, size_t size) {
    switch (s->kind) {
    case DXL_PADSRC_BUTTON: snprintf(out, size, "b%d", s->index); break;
    case DXL_PADSRC_HAT: snprintf(out, size, "h%d.%d", s->index, s->hat_mask); break;
    case DXL_PADSRC_AXIS:
        snprintf(out, size, "%sa%d%s", s->half > 0 ? "+" : s->half < 0 ? "-" : "", s->index, s->invert ? "~" : "");
        break;
    default: out[0] = '\0'; break;
    }
}

void dxl_padmap_format(const dxl_padmap *m, const char *platform, char *out, size_t size) {
    char name[sizeof m->name];
    size_t n = 0;
    for (const char *c = m->name; *c && n + 1 < sizeof name; c++)
        if (*c != ',') name[n++] = *c;
    name[n] = '\0';
    dxl_buf b;
    dxl_buf_init(&b);
    dxl_buf_printf(&b, "%s,%s,", m->guid, name);
    for (int i = 0; i < DXL_PADCTL_COUNT; i++) {
        if (m->src[i].kind == DXL_PADSRC_NONE) continue;
        char s[32];
        dxl_padsrc_format(&m->src[i], s, sizeof s);
        dxl_buf_printf(&b, "%s:%s,", dxl_padctl_names[i], s);
    }
    dxl_buf_printf(&b, "platform:%s,", platform);
    snprintf(out, size, "%s", b.data);
    dxl_buf_free(&b);
}

int dxl_padmap_controls(const dxl_padmap *m) {
    int n = 0;
    for (int i = 0; i < DXL_PADCTL_COUNT; i++) n += m->src[i].kind != DXL_PADSRC_NONE;
    return n;
}

int dxl_padmap_same(const dxl_padmap *a, const dxl_padmap *b) {
    for (int i = 0; i < DXL_PADCTL_COUNT; i++) {
        const dxl_padsrc *x = &a->src[i], *y = &b->src[i];
        if (x->kind != y->kind) return 0;
        if (x->kind == DXL_PADSRC_NONE) continue;
        if (x->index != y->index || x->hat_mask != y->hat_mask || x->half != y->half || x->invert != y->invert)
            return 0;
    }
    return 1;
}

static int number(const char *t, const char **end) {
    if (!isdigit((unsigned char)*t)) return -1;
    char *e;
    long v = strtol(t, &e, 10);
    *end = e;
    return v > 255 ? -1 : (int)v;
}

static int src_parse(const char *t, dxl_padsrc *s) {
    memset(s, 0, sizeof *s);
    const char *end;
    if (*t == '+' || *t == '-') {
        s->half = *t == '+' ? 1 : -1;
        t++;
    }
    switch (*t) {
    case 'b':
        s->kind = DXL_PADSRC_BUTTON;
        if ((s->index = number(t + 1, &end)) < 0 || *end) return -1;
        return s->half ? -1 : 0;
    case 'h':
        s->kind = DXL_PADSRC_HAT;
        if ((s->index = number(t + 1, &end)) < 0 || *end != '.') return -1;
        if ((s->hat_mask = number(end + 1, &end)) < 0 || *end) return -1;
        return s->half ? -1 : 0;
    case 'a':
        s->kind = DXL_PADSRC_AXIS;
        if ((s->index = number(t + 1, &end)) < 0) return -1;
        if (*end == '~') {
            s->invert = 1;
            end++;
        }
        return *end ? -1 : 0;
    default:
        return -1;
    }
}

int dxl_padmap_parse(const char *text, dxl_padmap *m) {
    memset(m, 0, sizeof *m);
    char *copy = dxl_xstrdup(text), *p = copy;
    int field = 0, rc = 0;
    while (p && rc == 0) {
        char *comma = strchr(p, ',');
        if (comma) *comma = '\0';
        char *f = dxl_trim(p);
        if (field == 0) {
            if (strlen(f) != 32) rc = -1;
            else snprintf(m->guid, sizeof m->guid, "%s", f);
        } else if (field == 1) {
            snprintf(m->name, sizeof m->name, "%s", f);
        } else if (*f) {
            char *colon = strchr(f, ':');
            if (!colon) {
                rc = -1;
                break;
            }
            *colon = '\0';
            for (int i = 0; i < DXL_PADCTL_COUNT; i++)
                if (!strcmp(f, dxl_padctl_names[i]) && src_parse(colon + 1, &m->src[i]) != 0) rc = -1;
        }
        field++;
        p = comma ? comma + 1 : NULL;
    }
    free(copy);
    return field >= 2 ? rc : -1;
}

int dxl_padmap_guid_no_crc(const char *guid, char out[33]) {
    snprintf(out, 33, "%s", guid);
    if (strlen(guid) != 32 || !strncmp(guid + 4, "0000", 4)) return 0;
    memcpy(out + 4, "0000", 4);
    return 1;
}

int dxl_padmap_guid_crc(const char *guid, char out[5]) {
    out[0] = '\0';
    if (strlen(guid) != 32 || !strncmp(guid + 4, "0000", 4)) return 0;
    /* Bytes 2 and 3 are the CRC's low byte and its high one. */
    snprintf(out, 5, "%c%c%c%c", tolower((unsigned char)guid[6]), tolower((unsigned char)guid[7]),
             tolower((unsigned char)guid[4]), tolower((unsigned char)guid[5]));
    return 1;
}

/* The file's text, "" for none. */
static char *read_all(const char *path) {
    FILE *f = path ? fopen(path, "rb") : NULL;
    dxl_buf b;
    dxl_buf_init(&b);
    char chunk[4096];
    size_t n;
    while (f && (n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&b, chunk, n);
    if (f) fclose(f);
    if (!b.data) dxl_buf_add(&b, "", 0);
    return b.data;
}

/* A mapping line's GUID, up to its first comma. */
static int same_guid(const char *line, const char *mapping) {
    const char *a = strchr(line, ','), *b = strchr(mapping, ',');
    if (!a || !b || a - line != b - mapping) return 0;
    return !dxl_strnicmp(line, mapping, (size_t)(a - line));
}

char *dxl_padmap_db_read(const char *path) { return read_all(path); }

int dxl_padmap_db_put(const char *path, const char *mapping, dxl_err *err) {
    char *text = read_all(path);
    dxl_buf out;
    dxl_buf_init(&out);
    int placed = 0;
    for (char *line = text; *line;) {
        char *nl = strchr(line, '\n');
        size_t n = nl ? (size_t)(nl - line) : strlen(line);
        char *l = dxl_xstrndup(line, n);
        char *t = dxl_trim(l);
        if (*t && *t != '#' && same_guid(t, mapping)) {
            if (!placed) dxl_buf_printf(&out, "%s\n", mapping);
            placed = 1;
        } else if (*t) {
            dxl_buf_printf(&out, "%s\n", t);
        }
        free(l);
        line += n + (nl ? 1 : 0);
    }
    if (!placed) dxl_buf_printf(&out, "%s\n", mapping);
    char *dir = dxl_path_dirname(path);
    dxl_path_mkdirs(dir);
    free(dir);
    int rc = dxl_path_write_atomic(path, out.data, out.len, err);
    dxl_buf_free(&out);
    free(text);
    return rc;
}

/* Each line of text, trimmed, that is not blank or a comment. */
typedef void (*line_fn)(const char *line, void *ctx);
static void each_line(const char *text, line_fn fn, void *ctx) {
    for (const char *line = text; *line;) {
        const char *nl = strchr(line, '\n');
        size_t n = nl ? (size_t)(nl - line) : strlen(line);
        char *l = dxl_xstrndup(line, n);
        char *t = dxl_trim(l);
        if (*t && *t != '#') fn(t, ctx);
        free(l);
        line += n + (nl ? 1 : 0);
    }
}

typedef struct {
    const char *ours;   /* the file's text */
    dxl_buf    *out;
} without_ctx;

/* A line of the file's whose GUID is the line's. */
static int ours_has(const char *ours, const char *line) {
    for (const char *p = ours; *p;) {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        while (n && (*p == ' ' || *p == '\t')) p++, n--;
        if (n && *p != '#' && same_guid(p, line)) return 1;
        p += n + (nl ? 1 : 0);
    }
    return 0;
}

int dxl_padmap_db_has(const char *text, const char *guid) {
    char line[40];
    snprintf(line, sizeof line, "%.32s,", guid);
    return text && strlen(guid) == 32 && ours_has(text, line);
}

static void keep_unless_ours(const char *line, void *ctx) {
    without_ctx *w = ctx;
    if (!ours_has(w->ours, line)) dxl_buf_printf(w->out, "%s\n", line);
}

char *dxl_padmap_without_ours(const char *steam, const char *db_path) {
    if (!steam || !*steam) return NULL;
    char *ours = read_all(db_path);
    dxl_buf b;
    dxl_buf_init(&b);
    without_ctx w = { ours, &b };
    each_line(steam, keep_unless_ours, &w);
    free(ours);
    if (!b.data) dxl_buf_add(&b, "", 0);
    return b.data;
}

/* A mapping of the file's, and its copy for an SDL2 before 2.26. */
static void ours_and_copy(const char *line, void *ctx) {
    dxl_buf *b = ctx;
    dxl_padmap m;
    if (dxl_padmap_parse(line, &m) != 0) return;
    dxl_buf_printf(b, "%s\n", line);
    char bare[33], crc[5];
    if (!dxl_padmap_guid_no_crc(m.guid, bare) || !dxl_padmap_guid_crc(m.guid, crc)) return;
    size_t n = strlen(line);
    dxl_buf_printf(b, "%s%s%scrc:%s,\n", bare, line + 32, n && line[n - 1] != ',' ? "," : "", crc);
}

char *dxl_padmap_engine_config(const char *steam, const char *db_path) {
    dxl_buf b;
    dxl_buf_init(&b);
    char *theirs = dxl_padmap_without_ours(steam, db_path);
    if (theirs) dxl_buf_puts(&b, theirs);
    free(theirs);
    char *text = read_all(db_path);
    each_line(text, ours_and_copy, &b);
    free(text);
    if (!b.data || !b.len) {
        dxl_buf_free(&b);
        return NULL;
    }
    return b.data;
}
