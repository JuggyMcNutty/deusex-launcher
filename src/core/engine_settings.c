#include "core/engine_settings.h"
#include "core/json.h"
#include "core/paths.h"
#include "platform/fs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The types the engine has on Linux (LauncherSettings.cpp; D3D11 there
 * stops it: "Render API not available on this platform"). Another Type is
 * dropped: with none the engine takes the ini's (dxl_es_names_type). */
static const char *const render_types[] = { "Vulkan", "OpenGL", "GLES", "Software", NULL };
static const char *const aa_modes[]     = { "Off", "MSAA2x", "MSAA4x", NULL };
static const char *const light_modes[]  = { "Normal", "OneX", "BrighterActors", NULL };
static const char *const gamma_modes[]  = { "D3D9", "XOpenGL", NULL };
/* Preset ids from bindings.c, plus "custom" for hand-edited bindings. */

#define RD "RenderDevice"
#define GP "Gamepad"
#define PF "Performance"

/* The defaults where the packaged engine-settings.json.default has none
 * (it has the engine's own for every member but RenderScale). These are
 * the Smart Pro's, where they were made: Antialias Off, as the PowerVR
 * GE8300 resolves MSAA to speckle, and VSync off. */
static const dxl_es_info fields[DXL_ES_FIELD_COUNT] = {
    [DXL_ES_RENDER_TYPE]       = { RD, "Type",                    DXL_ES_CHOICE, render_types, "Vulkan", 0, 0, 0, 0 },
    [DXL_ES_VSYNC]             = { RD, "UseVSync",                DXL_ES_BOOL,   NULL, NULL, 0, 0, 1, 1 },
    [DXL_ES_ANTIALIAS]         = { RD, "Antialias",               DXL_ES_CHOICE, aa_modes, "Off", 0, 0, 0, 0 },
    [DXL_ES_LIGHT]             = { RD, "Light",                   DXL_ES_CHOICE, light_modes, "Normal", 0, 0, 0, 0 },
    [DXL_ES_GAMMA]             = { RD, "Gamma",                   DXL_ES_CHOICE, gamma_modes, "D3D9", 0, 0, 0, 0 },
    [DXL_ES_BLOOM]             = { RD, "Bloom",                   DXL_ES_BOOL,   NULL, NULL, 0, 0, 1, 1 },
    [DXL_ES_BLOOM_AMOUNT]      = { RD, "BloomAmount",             DXL_ES_NUMBER, NULL, NULL, 128, 16, 255, 16 },
    [DXL_ES_HDR]               = { RD, "Hdr",                     DXL_ES_BOOL,   NULL, NULL, 0, 0, 1, 1 },
    [DXL_ES_HDR_SCALE]         = { RD, "HdrScale",                DXL_ES_NUMBER, NULL, NULL, 128, 16, 255, 16 },
    [DXL_ES_GAMMA_SCREENSHOTS] = { RD, "GammaCorrectScreenshots", DXL_ES_BOOL,   NULL, NULL, 0, 0, 1, 1 },
    [DXL_ES_DEBUG_LAYER]       = { RD, "UseDebugLayer",           DXL_ES_BOOL,   NULL, NULL, 0, 0, 1, 1 },

    [DXL_ES_PAD_ENABLED]       = { GP, "Enabled",           DXL_ES_BOOL,   NULL, NULL, 1,    0, 1, 1 },
    [DXL_ES_PAD_DEADZONE]      = { GP, "DeadZone",          DXL_ES_NUMBER, NULL, NULL, 0.20, 0.05, 0.50, 0.05 },
    [DXL_ES_PAD_LOOK_X]        = { GP, "LookSensitivityX",  DXL_ES_NUMBER, NULL, NULL, 1.00, 0.25, 3.00, 0.25 },
    [DXL_ES_PAD_LOOK_Y]        = { GP, "LookSensitivityY",  DXL_ES_NUMBER, NULL, NULL, 1.00, 0.25, 3.00, 0.25 },
    [DXL_ES_PAD_INVERT_Y]      = { GP, "InvertY",           DXL_ES_BOOL,   NULL, NULL, 0,    0, 1, 1 },
    [DXL_ES_PAD_CURSOR_SPEED]  = { GP, "CursorSpeed",       DXL_ES_NUMBER, NULL, NULL, 1.00, 0.25, 3.00, 0.25 },

    /* The engine clamps to 0.25..1; the UI offers dxl_es_render_heights. No
     * step: a height's exact fraction of the panel is kept, off any grid. */
    [DXL_ES_RENDER_SCALE]      = { PF, "RenderScale",       DXL_ES_NUMBER, NULL, NULL, 1.00, 0.25, 1.00, 0    },
};

int dxl_es_render_heights(int panel_h, int *heights, int max) {
    static const int usual[] = { 2160, 1440, 1080, 900, 720, 540, DXL_ES_MIN_RENDER_LINES };
    int n = 0;
    if (max < 1 || panel_h < 1) return 0;
    heights[n++] = panel_h;
    for (size_t i = 0; i < sizeof usual / sizeof *usual && n < max; i++)
        if (usual[i] < panel_h) heights[n++] = usual[i];
    return n;
}

const dxl_es_info *dxl_es_describe(dxl_es_field f) {
    return (f >= 0 && f < DXL_ES_FIELD_COUNT) ? &fields[f] : NULL;
}

struct dxl_engine_settings {
    char     *path;
    dxl_json *doc;
    dxl_json *defaults;    /* the packaged default file, or NULL */
    int       existed, corrupt, dirty;
    unsigned char present[DXL_ES_FIELD_COUNT];
    char      problem[DXL_ERR_LEN];   /* why the file could not be read */
    char     *kept;                   /* where a save kept it */
};

static int choice_valid(const dxl_es_info *in, const char *v) {
    if (!v) return 0;
    for (const char *const *c = in->choices; *c; c++)
        if (strcmp(*c, v) == 0) return 1;
    return 0;
}

static double clamp_step(const dxl_es_info *in, double v) {
    if (!isfinite(v)) v = in->def_number;
    if (v < in->min) v = in->min;
    if (v > in->max) v = in->max;
    return v;
}

/* The default for a field: the packaged file's value when it has a usable
 * one, the table's otherwise. */
static const char *default_choice(const dxl_engine_settings *s, dxl_es_field f) {
    const dxl_es_info *in = &fields[f];
    const char *v = dxl_json_get_string(dxl_json_get(s->defaults, in->section), in->key, NULL);
    return choice_valid(in, v) ? v : in->def_choice;
}

static double default_number(const dxl_engine_settings *s, dxl_es_field f) {
    const dxl_es_info *in = &fields[f];
    const dxl_json *sec = dxl_json_get(s->defaults, in->section);
    if (in->kind == DXL_ES_BOOL)
        return dxl_json_get_bool(sec, in->key, (int)in->def_number);
    return clamp_step(in, dxl_json_get_number(sec, in->key, in->def_number));
}

dxl_engine_settings *dxl_es_open(const char *path, const char *default_path) {
    dxl_engine_settings *s = dxl_xmalloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->path = dxl_xstrdup(path);

    if (default_path) {
        s->defaults = dxl_json_load(default_path, NULL);
        if (s->defaults && dxl_json_kind(s->defaults) != DXL_JSON_OBJECT) {
            dxl_json_free(s->defaults);
            s->defaults = NULL;
        }
    }

    if (dxl_path_exists(path)) {
        s->existed = 1;
        dxl_err e;
        dxl_err_set(&e, "not a JSON object");
        s->doc = dxl_json_load(path, &e);
        if (!s->doc || dxl_json_kind(s->doc) != DXL_JSON_OBJECT) {
            dxl_json_free(s->doc);
            s->doc = NULL;
            s->corrupt = 1;
            snprintf(s->problem, sizeof s->problem, "%s", dxl_err_msg(&e));
        }
    }
    if (!s->doc) {
        /* Missing or unreadable: start from nothing and let the defaults fill
         * every field but the Type. Marked dirty so that saving replaces what
         * the engine would otherwise have silently ignored. */
        s->doc = dxl_json_new_object();
        s->dirty = 1;
    }

    /* Fill whatever is absent or mistyped now, so every accessor below can
     * assume the member exists. Only the fields we own are touched. */
    for (int f = 0; f < DXL_ES_FIELD_COUNT; f++) {
        const dxl_es_info *in = &fields[f];
        dxl_json *sec = dxl_json_child(s->doc, in->section);
        dxl_json *v = dxl_json_get(sec, in->key);
        int usable = 0;
        switch (in->kind) {
        case DXL_ES_CHOICE:
            usable = v && dxl_json_kind(v) == DXL_JSON_STRING &&
                     choice_valid(in, dxl_json_get_string(sec, in->key, NULL));
            if (usable) break;
            if (f == DXL_ES_RENDER_TYPE) {
                /* A Type only as one is chosen (dxl_es_names_type): one
                 * the engine cannot take is dropped, not replaced. */
                if (dxl_json_remove(sec, in->key)) s->dirty = 1;
                continue;
            }
            dxl_json_set_string(sec, in->key, default_choice(s, f));
            break;
        case DXL_ES_BOOL:
            usable = v && dxl_json_kind(v) == DXL_JSON_BOOL;
            if (!usable) dxl_json_set_bool(sec, in->key, (int)default_number(s, f));
            break;
        case DXL_ES_NUMBER:
            usable = v && dxl_json_kind(v) == DXL_JSON_NUMBER;
            if (!usable) dxl_json_set_number(sec, in->key, default_number(s, f));
            break;
        }
        s->present[f] = (unsigned char)usable;
        if (!usable) s->dirty = 1;
    }
    return s;
}

void dxl_es_free(dxl_engine_settings *s) {
    if (!s) return;
    dxl_json_free(s->doc);
    dxl_json_free(s->defaults);
    free(s->path);
    free(s->kept);
    free(s);
}

const char *dxl_es_path(const dxl_engine_settings *s) { return s->path; }
int dxl_es_existed(const dxl_engine_settings *s)      { return s->existed; }
int dxl_es_was_corrupt(const dxl_engine_settings *s)  { return s->corrupt; }
const char *dxl_es_problem(const dxl_engine_settings *s) { return s->problem; }
const char *dxl_es_kept(const dxl_engine_settings *s)    { return s->kept; }
int dxl_es_dirty(const dxl_engine_settings *s)        { return s->dirty; }
int dxl_es_was_present(const dxl_engine_settings *s, dxl_es_field f) {
    return f >= 0 && f < DXL_ES_FIELD_COUNT && s->present[f];
}

int dxl_es_save(dxl_engine_settings *s, dxl_err *err) {
    char *dir = dxl_path_dirname(s->path);
    if (dxl_path_mkdirs(dir) != 0) {
        dxl_err_set(err, "cannot create %s", dir);
        free(dir);
        return -1;
    }
    free(dir);
    /* The file it could not read, kept beside it: never written over. */
    if (s->corrupt && dxl_path_exists(s->path)) {
        size_t n = strlen(s->path) + sizeof DXL_ES_KEPT;
        char *kept = dxl_xmalloc(n);
        snprintf(kept, n, "%s" DXL_ES_KEPT, s->path);
        if (dxl_fs_replace(s->path, kept) != 0) {
            dxl_err_set(err, "cannot keep %s, which could not be read, as %s", s->path, kept);
            free(kept);
            return -1;
        }
        free(s->kept);
        s->kept = kept;
    }
    if (dxl_json_save(s->doc, s->path, err) != 0) return -1;
    s->dirty = 0;
    s->existed = 1;
    s->corrupt = 0;
    return 0;
}

static dxl_json *section_of(const dxl_engine_settings *s, dxl_es_field f) {
    return dxl_json_get(s->doc, fields[f].section);
}

const char *dxl_es_choice(const dxl_engine_settings *s, dxl_es_field f) {
    const char *v = dxl_json_get_string(section_of(s, f), fields[f].key, NULL);
    return v ? v : default_choice(s, f);
}

int dxl_es_names_type(const dxl_engine_settings *s) {
    return dxl_json_get(section_of(s, DXL_ES_RENDER_TYPE), fields[DXL_ES_RENDER_TYPE].key) != NULL;
}

int dxl_es_bool(const dxl_engine_settings *s, dxl_es_field f) {
    return dxl_json_get_bool(section_of(s, f), fields[f].key, (int)fields[f].def_number);
}

double dxl_es_number(const dxl_engine_settings *s, dxl_es_field f) {
    return dxl_json_get_number(section_of(s, f), fields[f].key, fields[f].def_number);
}

int dxl_es_set_choice(dxl_engine_settings *s, dxl_es_field f, const char *v) {
    const dxl_es_info *in = &fields[f];
    if (in->kind != DXL_ES_CHOICE || !choice_valid(in, v)) return -1;
    /* A Type none named is written even as the defaults file's: chosen. */
    const char *now = dxl_json_get_string(section_of(s, f), in->key, NULL);
    if (now && strcmp(now, v) == 0) return 0;
    dxl_json_set_string(dxl_json_child(s->doc, in->section), in->key, v);
    s->dirty = 1;
    return 0;
}

void dxl_es_set_bool(dxl_engine_settings *s, dxl_es_field f, int v) {
    const dxl_es_info *in = &fields[f];
    if (in->kind != DXL_ES_BOOL) return;
    v = v ? 1 : 0;
    if (dxl_es_bool(s, f) == v) return;
    dxl_json_set_bool(dxl_json_child(s->doc, in->section), in->key, v);
    s->dirty = 1;
}

void dxl_es_set_number(dxl_engine_settings *s, dxl_es_field f, double v) {
    const dxl_es_info *in = &fields[f];
    if (in->kind != DXL_ES_NUMBER) return;
    v = clamp_step(in, v);
    /* Stepping by 0.05 accumulates binary error; snap to the step grid so the
     * file says 0.25, not 0.25000000000000006. */
    if (in->step > 0) v = in->min + round((v - in->min) / in->step) * in->step;
    v = clamp_step(in, round(v * 1e6) / 1e6);   /* the grid may overshoot max */
    if (dxl_es_number(s, f) == v) return;
    dxl_json_set_number(dxl_json_child(s->doc, in->section), in->key, v);
    s->dirty = 1;
}

void dxl_es_reset(dxl_engine_settings *s, dxl_es_field f) {
    switch (fields[f].kind) {
    case DXL_ES_CHOICE: dxl_es_set_choice(s, f, default_choice(s, f)); break;
    case DXL_ES_BOOL:   dxl_es_set_bool(s, f, (int)default_number(s, f)); break;
    case DXL_ES_NUMBER: dxl_es_set_number(s, f, default_number(s, f)); break;
    }
}

void dxl_es_reset_section(dxl_engine_settings *s, const char *section) {
    for (int f = 0; f < DXL_ES_FIELD_COUNT; f++)
        if (strcmp(fields[f].section, section) == 0) dxl_es_reset(s, (dxl_es_field)f);
}

/* LauncherSettings.cpp's, with no Settings.json to read. */
#define ENGINE_OWN_ANTIALIAS "MSAA4x"

void dxl_es_antialias_taken(const char *path, char *out, size_t size) {
    dxl_engine_settings *s = dxl_es_open(path, NULL);
    int read = s->existed && !s->corrupt && s->present[DXL_ES_ANTIALIAS];
    snprintf(out, size, "%s", read ? dxl_es_choice(s, DXL_ES_ANTIALIAS) : ENGINE_OWN_ANTIALIAS);
    dxl_es_free(s);
}

int dxl_es_antialias_off(const char *path, const char *default_path, dxl_err *err) {
    char now[16];
    dxl_es_antialias_taken(path, now, sizeof now);
    if (!strcmp(now, "Off")) return 0;
    dxl_engine_settings *s = dxl_es_open(path, default_path);
    dxl_es_set_choice(s, DXL_ES_ANTIALIAS, "Off");
    int rc = dxl_es_save(s, err) != 0 ? -1 : dxl_es_kept(s) ? 2 : 1;
    dxl_es_free(s);
    return rc;
}
