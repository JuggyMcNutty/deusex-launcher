/* The settings the launcher offers, as data: each a row of its page, with
 * its label, help, kind, range or choices, default, and the places it is
 * kept -- up to four, in the profile's DeusEx.ini or User.ini, the
 * engine's Settings.json, launcher.ini or profile.ini -- each with a codec
 * that writes it as that place's reader takes it, and reads it strictly.
 *
 * A value is read from its first place that holds one, as the engine
 * reads it -- but in the system ini only from the generation the engine
 * reads: its own [Engine.DXEngine*] sections once [Engine.DXEngineAudioDevice]
 * has a key, the retail ones before (DXEngine's PackageManager.cpp); one
 * missing there is the engine's own default, the row's. It is written to
 * every place, so both generations agree. The codecs read as the engine
 * reads and write as it does: a value its converters cannot read (a rate
 * without "Hz") is no value, the row showing the engine's default, which
 * it takes for one. Values travel as text in one form: "1"/"0", a number with
 * "%.6g", a choice's own value, "WxH" for a size, text. The rows are
 * core/settings_table.c's; the presets are data too.
 */
#ifndef DXL_SETTINGS_H
#define DXL_SETTINGS_H

#include "core/common.h"
#include "core/engine_settings.h"
#include "core/ini.h"

typedef enum {
    DXL_SET_BOOL,
    DXL_SET_NUMBER,
    DXL_SET_CHOICE,
    DXL_SET_TEXT,
    DXL_SET_ACTION,     /* a button: the page does it */
    DXL_SET_HEADING     /* a group's heading, nothing kept */
} dxl_set_kind;

typedef enum {
    DXL_STORE_NONE,
    DXL_STORE_SYSTEM,   /* the profile's DeusEx.ini */
    DXL_STORE_USER,     /* its User.ini */
    DXL_STORE_ENGINE,   /* its engine folder's Settings.json */
    DXL_STORE_LAUNCHER, /* launcher.ini */
    DXL_STORE_PROFILE,  /* profile.ini */
    DXL_STORE_NEXT_RUN  /* this run's words alone, kept nowhere */
} dxl_store;

typedef enum {
    DXL_CODEC_BOOL,     /* True / False; reads true, yes or 1 and false, no or 0 in any case, as the engine;
                         * in User.ini, a class's config, true or 1 and false or 0 alone, as its import */
    DXL_CODEC_NOT,      /* the same, the value its opposite (NoDynamicLights for dynamic lights) */
    DXL_CODEC_INT,      /* "%d" of the value times scale, cut to a whole; reads the leading number */
    DXL_CODEC_BYTE,     /* the same, 0 to 255: the engine wraps what is past it */
    DXL_CODEC_FLOAT,    /* "%f", the value times scale */
    DXL_CODEC_HZ,       /* "<n>Hz"; read only with its Hz */
    DXL_CODEC_TEXT,     /* as it is */
    DXL_CODEC_WIDTH,    /* a size's width, "%d"; its height is the next place's */
    DXL_CODEC_HEIGHT,   /* a size's height */
    DXL_CODEC_JSON      /* a Settings.json member: its field's own kind */
} dxl_codec;

/* Which generation of the system ini a place is in. */
#define DXL_GEN_ANY     0
#define DXL_GEN_ENGINE  1   /* [Engine.DXEngine*] */
#define DXL_GEN_RETAIL  2   /* [WinDrv.WindowsClient], [Galaxy.GalaxyAudioSubsystem], the render device's */

typedef struct {
    dxl_store    store;
    const char  *section;   /* the ini's section; Settings.json: unused */
    const char  *key;       /* the ini's key; Settings.json: unused */
    dxl_codec    codec;
    double       scale;     /* the store's value is the setting's times this; 0 for 1 */
    dxl_es_field field;     /* DXL_CODEC_JSON's member */
    int          gen;       /* DXL_GEN_* */
} dxl_set_place;

typedef struct {
    const char *value;      /* as kept */
    const char *label;      /* as shown */
    const char *help;       /* what this choice does, under the row's own; may be NULL */
} dxl_set_choice;

#define DXL_SET_PLACES 4

/* Whether the engine reads its own [Engine.DXEngine*] sections of this
 * system ini: [Engine.DXEngineAudioDevice] has a key. */
int dxl_settings_engine_generation(const dxl_ini *system);

/* Row flags. */
#define DXL_SET_ADVANCED 0x01   /* after the page's others */
#define DXL_SET_PRESET   0x02   /* the graphics preset's: no place of its own */
#define DXL_SET_RENDERER 0x04   /* its choices the renderers this machine runs, the page's */

/* The anti-aliasing row, the one no_msaa holds at Off (dxl_stores). */
#define DXL_ANTIALIAS_ROW "graphics.antialias"

typedef struct {
    const char *id;             /* "display.fullscreen" */
    const char *page;           /* "Display" */
    const char *label;
    const char *help;           /* the help line under the page */
    dxl_set_kind kind;
    double min, max, step;      /* DXL_SET_NUMBER */
    const char *format;         /* its shown value: printf of the value times shown_scale */
    double shown_scale;
    const dxl_set_choice *choices;   /* DXL_SET_CHOICE: ends with a NULL value */
    /* What the engine or the game takes with no place holding a value (the
     * engine's built-in default, the class's, Settings.json's reader's,
     * the launcher's own); an empty text's. */
    const char *def;
    const char *chars;          /* DXL_SET_TEXT: what it keeps besides ASCII letters and digits; NULL, all */
    dxl_set_place at[DXL_SET_PLACES];   /* read in order, written to all */
    int flags;
} dxl_setting;

/* The pages, in the order the Settings screen shows them. */
size_t      dxl_settings_page_count(void);
const char *dxl_settings_page(size_t i);

size_t             dxl_settings_count(void);
const dxl_setting *dxl_setting_at(size_t i);
const dxl_setting *dxl_setting_find(const char *id);

/* The open stores a value is read from and written to; NULL for one the
 * launcher has not opened. next_run is this run's words, appended by
 * dxl_setting_set for DXL_STORE_NEXT_RUN rows. no_msaa: the GPU's
 * multisampling is broken (dxl_gpu_msaa_broken), and anti-aliasing takes
 * Off alone -- a preset's, a reset's or a row's value for it written as
 * Off, and a preset matched without it. */
typedef struct {
    dxl_ini             *system, *user, *launcher, *profile;
    dxl_engine_settings *engine;
    char                 next_run[512];
    int                  no_msaa;
} dxl_stores;

/* The value, from the first place that holds one, as text; the default
 * when none does, the renderer's the one the engine takes by the system
 * ini where Settings.json names no Type (dxl_es_names_type). Returns 1
 * when a place held it. */
int    dxl_setting_get(const dxl_stores *st, const dxl_setting *s, char *out, size_t size);
double dxl_setting_number(const dxl_stores *st, const dxl_setting *s);
int    dxl_setting_bool(const dxl_stores *st, const dxl_setting *s);
/* The index of the value among the choices; -1 for one not among them. */
int    dxl_setting_choice(const dxl_stores *st, const dxl_setting *s);

/* Written to every place: -1 for a value the row does not take (a choice
 * not among them, text for a number), with nothing written. A number is
 * clamped to the range; a text keeps the row's characters alone, and is
 * the default when none is left. */
int    dxl_setting_set(dxl_stores *st, const dxl_setting *s, const char *value);
/* Every row of a page (NULL: every page) back to its value in fresh, the
 * stores a profile starts with (dxl_profile_defaults, the engine's
 * defaults file), or to its default where they hold none or fresh is
 * NULL. */
void   dxl_settings_reset(dxl_stores *st, const dxl_stores *fresh, const char *page);

/* ---- presets ------------------------------------------------------------ */

typedef struct {
    const char *setting;   /* a row's id */
    const char *value;
} dxl_preset_value;

typedef struct {
    const char *id;        /* "as-in-2000" */
    const char *label;
    const char *help;
    const dxl_preset_value *values;   /* ends with a NULL setting */
} dxl_preset;

size_t            dxl_preset_count(void);
const dxl_preset *dxl_preset_at(size_t i);
void              dxl_preset_apply(dxl_stores *st, const dxl_preset *p);
/* The preset every value of which the stores hold, or NULL: "Custom". With
 * no_msaa, its anti-aliasing is not looked at. */
const dxl_preset *dxl_preset_matching(const dxl_stores *st);

#endif
