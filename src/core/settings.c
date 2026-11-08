#include "core/settings.h"
#include "core/engine_sections.h"
#include "core/gamespy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* core/settings_table.c's. */
extern const dxl_setting dxl_settings_table[];
extern const size_t dxl_settings_table_count;
extern const char *const dxl_settings_pages[];
extern const size_t dxl_settings_pages_count;
extern const dxl_preset dxl_presets[];
extern const size_t dxl_presets_count;

size_t dxl_settings_page_count(void) { return dxl_settings_pages_count; }
const char *dxl_settings_page(size_t i) { return i < dxl_settings_pages_count ? dxl_settings_pages[i] : NULL; }
size_t dxl_settings_count(void) { return dxl_settings_table_count; }
const dxl_setting *dxl_setting_at(size_t i) { return i < dxl_settings_table_count ? &dxl_settings_table[i] : NULL; }

const dxl_setting *dxl_setting_find(const char *id) {
    for (size_t i = 0; i < dxl_settings_table_count; i++)
        if (strcmp(dxl_settings_table[i].id, id) == 0) return &dxl_settings_table[i];
    return NULL;
}

static dxl_ini *ini_of(const dxl_stores *st, dxl_store s) {
    switch (s) {
    case DXL_STORE_SYSTEM: return st->system;
    case DXL_STORE_USER: return st->user;
    case DXL_STORE_LAUNCHER: return st->launcher;
    case DXL_STORE_PROFILE: return st->profile;
    default: return NULL;
    }
}

static double scale_of(const dxl_set_place *p) { return p->scale != 0 ? p->scale : 1; }

static int keeps(dxl_set_kind k) {
    return k == DXL_SET_BOOL || k == DXL_SET_NUMBER || k == DXL_SET_CHOICE || k == DXL_SET_TEXT;
}

/* ---- this run's words ---------------------------------------------------- */

static int has_word(const char *line, const char *word) {
    size_t n = strlen(word);
    for (const char *p = line; (p = dxl_stristr(p, word)); p += n)
        if ((p == line || p[-1] == ' ') && (p[n] == '\0' || p[n] == ' ')) return 1;
    return 0;
}

/* word on or off in line; a word may be two ("-nohard -noddraw"). */
static void set_word(char *line, size_t size, const char *word, int on) {
    if (on == has_word(line, word)) return;
    if (on) {
        size_t len = strlen(line);
        snprintf(line + len, size - len, "%s%s", len ? " " : "", word);
        return;
    }
    size_t n = strlen(word);
    for (char *p = line; (p = (char *)dxl_stristr(p, word));) {
        int starts = p == line || p[-1] == ' ', ends = p[n] == '\0' || p[n] == ' ';
        if (!starts || !ends) {
            p += n;
            continue;
        }
        char *after = p + n;
        while (*after == ' ') after++;
        if (p > line && !*after) p--;   /* the space before a last word */
        memmove(p, after, strlen(after) + 1);
    }
}

/* ---- codecs ------------------------------------------------------------- */

int dxl_settings_engine_generation(const dxl_ini *system) {
    return system && dxl_ini_pairs(system, "Engine.DXEngineAudioDevice", NULL, NULL, 0) > 0;
}

/* The leading whole number, as the engine's std::stoi reads it: blanks, a
 * sign, digits; the rest ignored. 0 when there is none. */
static int leading_int(const char *v, long *out) {
    char *end;
    while (*v == ' ' || *v == '\t') v++;
    *out = strtol(v, &end, 10);
    return end != v;
}

static int leading_float(const char *v, double *out) {
    char *end;
    while (*v == ' ' || *v == '\t') v++;
    *out = strtod(v, &end);
    return end != v;
}

static int full_number(const char *v, double *out) {
    char *end;
    while (*v == ' ') v++;
    if (!*v) return 0;
    *out = strtod(v, &end);
    while (*end == ' ') end++;
    return *end == '\0';
}

/* A place's text as the setting's value; 0 for text the engine would not
 * take either (it stops on such a value at start). */
static int decode(const dxl_set_place *p, const char *v, char *out, size_t size) {
    long i;
    double n;
    switch (p->codec) {
    case DXL_CODEC_BOOL:
    case DXL_CODEC_NOT: {
        /* The game's inis as the engine and a class's import read them,
         * with no yes or no; the launcher's own as it reads them. */
        int game = p->store == DXL_STORE_SYSTEM || p->store == DXL_STORE_USER;
        int b = game ? dxl_class_bool(v) : dxl_text_bool(v);
        if (b < 0) return 0;
        snprintf(out, size, "%d", p->codec == DXL_CODEC_NOT ? !b : b);
        return 1;
    }
    case DXL_CODEC_HZ:
        if (!(i = dxl_engine_rate(v))) return 0;
        snprintf(out, size, "%.6g", i / scale_of(p));
        return 1;
    case DXL_CODEC_INT:
    case DXL_CODEC_WIDTH:
    case DXL_CODEC_HEIGHT:
        if (!leading_int(v, &i)) return 0;
        snprintf(out, size, "%.6g", i / scale_of(p));
        return 1;
    case DXL_CODEC_BYTE:
        if (!leading_int(v, &i)) return 0;
        snprintf(out, size, "%.6g", (unsigned char)i / scale_of(p));
        return 1;
    case DXL_CODEC_FLOAT:
        /* The game's inis as the engine and a class's import read a float,
         * any text by strtod, an empty one 0 (bugs.md ENG.11); the
         * launcher's own with a number. */
        if (p->store == DXL_STORE_SYSTEM || p->store == DXL_STORE_USER) n = strtod(v, NULL);
        else if (!leading_float(v, &n)) return 0;
        snprintf(out, size, "%.6g", n / scale_of(p));
        return 1;
    case DXL_CODEC_TEXT:
        snprintf(out, size, "%s", v);
        return 1;
    case DXL_CODEC_HOST:
        snprintf(out, size, "%s", v);
        return *v != '\0';
    case DXL_CODEC_PORT:
        /* UIntProperty::ImportText's: a '+', a blank or no text leaves the
         * class's default. */
        if (*v != '-' && (*v < '0' || *v > '9')) return 0;
        snprintf(out, size, "%d", (int)strtol(v, NULL, 10));
        return 1;
    default:
        return 0;
    }
}

/* Written only as the player changes it: an untouched value keeps its own
 * text, so a whole is the nearest. */
static void encode(const dxl_set_place *p, const char *value, char *out, size_t size) {
    double n = atof(value) * scale_of(p);
    long whole = lround(n);
    switch (p->codec) {
    case DXL_CODEC_BOOL: snprintf(out, size, "%s", strcmp(value, "1") == 0 ? "True" : "False"); break;
    case DXL_CODEC_NOT: snprintf(out, size, "%s", strcmp(value, "1") == 0 ? "False" : "True"); break;
    case DXL_CODEC_INT:
    case DXL_CODEC_WIDTH:
    case DXL_CODEC_HEIGHT: snprintf(out, size, "%ld", whole); break;
    case DXL_CODEC_BYTE: snprintf(out, size, "%ld", whole < 0 ? 0 : whole > 255 ? 255 : whole); break;
    case DXL_CODEC_FLOAT: snprintf(out, size, "%f", n); break;
    case DXL_CODEC_HZ: snprintf(out, size, "%ldHz", whole); break;
    default: snprintf(out, size, "%s", value); break;
    }
}

/* Whether the place is in the generation the engine reads. */
static int live_place(const dxl_stores *st, const dxl_set_place *p) {
    if (p->store != DXL_STORE_SYSTEM || p->gen == DXL_GEN_ANY) return 1;
    int engine = dxl_settings_engine_generation(st->system);
    return p->gen == DXL_GEN_ENGINE ? engine : !engine;
}

static int read_place(const dxl_stores *st, const dxl_set_place *p, char *out, size_t size) {
    if (!live_place(st, p)) return 0;
    if (p->store == DXL_STORE_NEXT_RUN) {
        snprintf(out, size, "%d", has_word(st->next_run, p->key));
        return 1;
    }
    if (p->store == DXL_STORE_ENGINE) {
        if (!st->engine) return 0;
        const dxl_es_info *info = dxl_es_describe(p->field);
        switch (info->kind) {
        case DXL_ES_BOOL: snprintf(out, size, "%d", dxl_es_bool(st->engine, p->field) ? 1 : 0); return 1;
        case DXL_ES_NUMBER: snprintf(out, size, "%.6g", dxl_es_number(st->engine, p->field) / scale_of(p)); return 1;
        case DXL_ES_CHOICE:
            if (p->field == DXL_ES_RENDER_TYPE && !dxl_es_names_type(st->engine)) return 0;
            snprintf(out, size, "%s", dxl_es_choice(st->engine, p->field));
            return 1;
        }
        return 0;
    }
    dxl_ini *ini = ini_of(st, p->store);
    const char *v = ini ? dxl_ini_get(ini, p->section, p->key) : NULL;
    return v ? decode(p, v, out, size) : 0;
}

/* -1 when the place refuses the value (a choice Settings.json's reader
 * does not know). */
static int write_place(dxl_stores *st, const dxl_set_place *p, const char *value) {
    if (p->store == DXL_STORE_NEXT_RUN) {
        set_word(st->next_run, sizeof st->next_run, p->key, strcmp(value, "1") == 0);
        return 0;
    }
    if (p->store == DXL_STORE_ENGINE) {
        if (!st->engine) return 0;
        const dxl_es_info *info = dxl_es_describe(p->field);
        switch (info->kind) {
        case DXL_ES_BOOL: dxl_es_set_bool(st->engine, p->field, strcmp(value, "1") == 0); break;
        case DXL_ES_NUMBER: dxl_es_set_number(st->engine, p->field, atof(value) * scale_of(p)); break;
        case DXL_ES_CHOICE: return dxl_es_set_choice(st->engine, p->field, value) == 0 ? 0 : -1;
        }
        return 0;
    }
    dxl_ini *ini = ini_of(st, p->store);
    if (!ini) return 0;
    char text[512];
    encode(p, value, text, sizeof text);
    dxl_ini_set(ini, p->section, p->key, text);
    return 0;
}

/* ---- values -------------------------------------------------------------- */

/* The renderer the engine takes where Settings.json names no Type, as its
 * Engine::ChooseRenderDevice: the software one for the system ini's
 * [Engine.Engine] GameRenderDevice=SoftDrv.SoftwareRenderDevice, else
 * Vulkan (core/renderers.h). */
static const char *ini_renderer(const dxl_ini *system) {
    const char *device = system ? dxl_ini_get(system, "Engine.Engine", "GameRenderDevice") : NULL;
    return device && !dxl_stricmp(device, "SoftDrv.SoftwareRenderDevice") ? "Software" : "Vulkan";
}

int dxl_setting_get(const dxl_stores *st, const dxl_setting *s, char *out, size_t size) {
    /* The renderer: its Type, or the ini's where none is named (this port
     * has no Mesa CPU-driver rows, linux-x86_64's second place). */
    if (s->flags & DXL_SET_RENDERER) {
        int named = read_place(st, &s->at[0], out, size);
        if (!named) snprintf(out, size, "%s", ini_renderer(st->system));
        return named;
    }
    for (int i = 0; i < DXL_SET_PLACES && s->at[i].store != DXL_STORE_NONE; i++) {
        /* A size: its width's place and its height's, the next. */
        if (s->at[i].codec == DXL_CODEC_WIDTH && i + 1 < DXL_SET_PLACES && s->at[i + 1].codec == DXL_CODEC_HEIGHT) {
            char w[32], h[32];
            if (read_place(st, &s->at[i], w, sizeof w) && read_place(st, &s->at[i + 1], h, sizeof h)) {
                snprintf(out, size, "%sx%s", w, h);
                return 1;
            }
            i++;
            continue;
        }
        /* A master server: its address's place and its port's, the next,
         * the class's port where that holds none. */
        if (s->at[i].codec == DXL_CODEC_HOST && i + 1 < DXL_SET_PLACES && s->at[i + 1].codec == DXL_CODEC_PORT) {
            char host[400], port[32];
            if (read_place(st, &s->at[i], host, sizeof host)) {
                if (!read_place(st, &s->at[i + 1], port, sizeof port)) snprintf(port, sizeof port, "%d", DXL_MASTER_PORT);
                snprintf(out, size, "%s:%s", host, port);
                return 1;
            }
            i++;
            continue;
        }
        if (read_place(st, &s->at[i], out, size)) return 1;
    }
    snprintf(out, size, "%s", s->def ? s->def : "");
    return 0;
}

double dxl_setting_number(const dxl_stores *st, const dxl_setting *s) {
    char v[64];
    dxl_setting_get(st, s, v, sizeof v);
    return atof(v);
}

int dxl_setting_bool(const dxl_stores *st, const dxl_setting *s) {
    char v[16];
    dxl_setting_get(st, s, v, sizeof v);
    return strcmp(v, "1") == 0;
}

int dxl_setting_choice(const dxl_stores *st, const dxl_setting *s) {
    char v[128];
    dxl_setting_get(st, s, v, sizeof v);
    for (int i = 0; s->choices && s->choices[i].value; i++)
        if (dxl_stricmp(s->choices[i].value, v) == 0) return i;
    return -1;
}

/* The value as the row keeps it, or 0 when it takes no such value. */
static int normalise(const dxl_setting *s, const char *in, char *out, size_t size) {
    double n;
    switch (s->kind) {
    case DXL_SET_BOOL:
        if (!strcmp(in, "1") || !dxl_stricmp(in, "True")) snprintf(out, size, "1");
        else if (!strcmp(in, "0") || !dxl_stricmp(in, "False")) snprintf(out, size, "0");
        else return 0;
        return 1;
    case DXL_SET_NUMBER:
        /* Clamped, not stepped: a value the game's files hold off the
         * slider's steps (a volume of 160) is kept as it is. */
        if (!full_number(in, &n)) return 0;
        if (n < s->min) n = s->min;
        if (n > s->max) n = s->max;
        snprintf(out, size, "%.6g", n);
        return 1;
    case DXL_SET_CHOICE:
        /* The renderers', and a size's, are the page's to offer. */
        if (!s->choices || (s->flags & DXL_SET_RENDERER) || s->at[0].codec == DXL_CODEC_WIDTH) {
            if (s->at[0].codec == DXL_CODEC_WIDTH) {
                int w, h;
                if (sscanf(in, "%dx%d", &w, &h) != 2 || w < 320 || h < 240) return 0;
            }
            snprintf(out, size, "%s", in);
            return 1;
        }
        for (int i = 0; s->choices[i].value; i++)
            if (dxl_stricmp(s->choices[i].value, in) == 0) {
                snprintf(out, size, "%s", s->choices[i].value);
                return 1;
            }
        return 0;
    case DXL_SET_TEXT: {
        size_t len = 0;
        for (const char *c = in; *c && len + 1 < size; c++) {
            int plain = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9');
            if (!s->chars || plain || strchr(s->chars, *c)) out[len++] = *c;
        }
        out[len] = '\0';
        if (!len && s->def) snprintf(out, size, "%s", s->def);
        return 1;
    }
    default:
        return 0;
    }
}

int dxl_setting_set(dxl_stores *st, const dxl_setting *s, const char *value) {
    char v[512];
    if (!value || !normalise(s, value, v, sizeof v)) return -1;
    if (st->no_msaa && !strcmp(s->id, DXL_ANTIALIAS_ROW)) snprintf(v, sizeof v, "Off");
    int rc = 0;
    for (int i = 0; i < DXL_SET_PLACES && s->at[i].store != DXL_STORE_NONE; i++) {
        const char *part = v;
        char half[sizeof v];
        int w = 0, h = 0;
        if (s->at[i].codec == DXL_CODEC_WIDTH || s->at[i].codec == DXL_CODEC_HEIGHT) {
            sscanf(v, "%dx%d", &w, &h);
            snprintf(half, sizeof half, "%d", s->at[i].codec == DXL_CODEC_WIDTH ? w : h);
            part = half;
        } else if (s->at[i].codec == DXL_CODEC_HOST || s->at[i].codec == DXL_CODEC_PORT) {
            /* "<address>:<port>", the port the digits after the last ':';
             * with none, the address all of it at the class's port. */
            const char *colon = strrchr(v, ':');
            int ported = colon && colon[1] && strspn(colon + 1, "0123456789") == strlen(colon + 1);
            if (s->at[i].codec == DXL_CODEC_HOST)
                snprintf(half, sizeof half, "%.*s", ported ? (int)(colon - v) : (int)strlen(v), v);
            else if (ported)
                snprintf(half, sizeof half, "%s", colon + 1);
            else
                snprintf(half, sizeof half, "%d", DXL_MASTER_PORT);
            part = half;
        }
        if (write_place(st, &s->at[i], part) != 0) rc = -1;
    }
    return rc;
}

void dxl_settings_reset(dxl_stores *st, const dxl_stores *fresh, const char *page) {
    for (size_t i = 0; i < dxl_settings_table_count; i++) {
        const dxl_setting *s = &dxl_settings_table[i];
        if (!keeps(s->kind) || !s->def) continue;
        if (page && strcmp(page, s->page) != 0) continue;
        char v[512];
        if (fresh) dxl_setting_get(fresh, s, v, sizeof v);
        else snprintf(v, sizeof v, "%s", s->def);
        dxl_setting_set(st, s, v);
    }
}

/* ---- presets ------------------------------------------------------------ */

size_t dxl_preset_count(void) { return dxl_presets_count; }
const dxl_preset *dxl_preset_at(size_t i) { return i < dxl_presets_count ? &dxl_presets[i] : NULL; }

void dxl_preset_apply(dxl_stores *st, const dxl_preset *p) {
    for (const dxl_preset_value *v = p->values; v->setting; v++) {
        const dxl_setting *s = dxl_setting_find(v->setting);
        if (s) dxl_setting_set(st, s, v->value);
    }
}

static int same(const dxl_setting *s, const char *a, const char *b) {
    if (s->kind == DXL_SET_NUMBER) {
        double x = atof(a), y = atof(b);
        return fabs(x - y) <= 1e-6 * (fabs(x) > 1 ? fabs(x) : 1);
    }
    return dxl_stricmp(a, b) == 0;
}

const dxl_preset *dxl_preset_matching(const dxl_stores *st) {
    for (size_t i = 0; i < dxl_presets_count; i++) {
        int all = 1;
        for (const dxl_preset_value *v = dxl_presets[i].values; v->setting && all; v++) {
            const dxl_setting *s = dxl_setting_find(v->setting);
            char now[512], want[512];
            if (s && st->no_msaa && !strcmp(s->id, DXL_ANTIALIAS_ROW)) continue;
            if (!s || !normalise(s, v->value, want, sizeof want)) {
                all = 0;
                break;
            }
            dxl_setting_get(st, s, now, sizeof now);
            all = same(s, now, want);
        }
        if (all) return &dxl_presets[i];
    }
    return NULL;
}
