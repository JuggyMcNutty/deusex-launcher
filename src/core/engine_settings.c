#include "core/engine_settings.h"
#include "core/json.h"
#include "core/paths.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Types the engine knows (LauncherSettings.cpp), plus GLES, which the fork's
 * GLES backend adds. An unknown Type makes the engine keep its default
 * (Vulkan), so writing one would be a silent no-op. */
static const char *const render_types[] = { "Vulkan", "OpenGL", "GLES", "D3D11", "D3D12", NULL };
static const char *const aa_modes[]     = { "Off", "MSAA2x", "MSAA4x", NULL };
static const char *const light_modes[]  = { "Normal", "OneX", "BrighterActors", NULL };
static const char *const gamma_modes[]  = { "D3D9", "XOpenGL", NULL };
/* Preset ids from bindings.c, plus "custom" for hand-edited bindings. */
static const char *const pad_layouts[]  = { "modern", "southpaw", "classic", "custom", NULL };

#define RD "RenderDevice"
#define GP "Gamepad"
#define PF "Performance"

/* Defaults are the port's, not the engine's: Antialias is Off because the
 * PowerVR GE8300 resolves MSAA to speckle, and VSync is off because the game
 * runs below the panel's 60 Hz and vsync would quantise it to 30 or 20. */
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
    [DXL_ES_PAD_LAYOUT]        = { GP, "Layout",            DXL_ES_CHOICE, pad_layouts, "modern", 0, 0, 0, 0 },

    /* Off unless the port's packaged default turns it on: a desktop has the CPU. */
    [DXL_ES_AI_LOD]            = { PF, "AiLevelOfDetail",   DXL_ES_BOOL,   NULL, NULL, 0,    0, 1, 1 },
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
        s->doc = dxl_json_load(path, NULL);
        if (!s->doc || dxl_json_kind(s->doc) != DXL_JSON_OBJECT) {
            dxl_json_free(s->doc);
            s->doc = NULL;
            s->corrupt = 1;
        }
    }
    if (!s->doc) {
        /* Missing or unreadable: start from nothing and let the defaults fill
         * every field. Marked dirty so that saving replaces what the engine
         * would otherwise have silently ignored. */
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
            if (!usable) dxl_json_set_string(sec, in->key, default_choice(s, f));
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
    free(s);
}

const char *dxl_es_path(const dxl_engine_settings *s) { return s->path; }
int dxl_es_existed(const dxl_engine_settings *s)      { return s->existed; }
int dxl_es_was_corrupt(const dxl_engine_settings *s)  { return s->corrupt; }
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
    return dxl_json_get_string(section_of(s, f), fields[f].key, fields[f].def_choice);
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
    if (strcmp(dxl_es_choice(s, f), v) == 0) return 0;
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
