/* Surreal Engine's own settings: <HOME>/.config/SurrealEngine/Settings.json.
 *
 * This file, not DeusEx.ini, is where the engine takes its renderer from:
 * Surreal Engine overrides [Engine.Engine] GameRenderDevice with its own
 * device class (Engine.cpp, "Override the ini file for things that are
 * internal in Surreal Engine") and reads RenderDevice.Type from here instead.
 * So everything the launcher offers about rendering lands in this file.
 *
 * Every field is described by a table entry -- its JSON location, kind,
 * allowed values and range -- so the UI can build its rows from the table
 * instead of hard-coding each one. Values are the engine's own spellings
 * (LauncherSettings.cpp): "MSAA4x", "OneX", "XOpenGL", ...
 *
 * The engine parses the file inside a catch-all, so a file it cannot read
 * costs every setting at once, and a member it cannot find reads as empty,
 * false or 0. That is why a save always writes every member of every section
 * here, and why an unreadable file is replaced rather than left alone. A
 * member the file lacks takes the packaged default, so a field added later
 * reaches an existing install with the port's value.
 */
#ifndef DXL_ENGINE_SETTINGS_H
#define DXL_ENGINE_SETTINGS_H

#include "core/common.h"

typedef enum {
    /* RenderDevice */
    DXL_ES_RENDER_TYPE,
    DXL_ES_VSYNC,
    DXL_ES_ANTIALIAS,
    DXL_ES_LIGHT,
    DXL_ES_GAMMA,
    DXL_ES_BLOOM,
    DXL_ES_BLOOM_AMOUNT,
    DXL_ES_HDR,
    DXL_ES_HDR_SCALE,
    DXL_ES_GAMMA_SCREENSHOTS,
    DXL_ES_DEBUG_LAYER,
    /* Gamepad -- read by the fork's controller support */
    DXL_ES_PAD_ENABLED,
    DXL_ES_PAD_DEADZONE,
    DXL_ES_PAD_LOOK_X,
    DXL_ES_PAD_LOOK_Y,
    DXL_ES_PAD_INVERT_Y,
    DXL_ES_PAD_CURSOR_SPEED,
    DXL_ES_PAD_LAYOUT,          /* which preset the launcher last applied */
    /* Performance -- the fork's speed-for-fidelity choices */
    DXL_ES_AI_LOD,              /* pawns out of sight think every third frame (sixth when far) */
    DXL_ES_RENDER_SCALE,        /* the scene's size as a fraction of the window's */
    DXL_ES_FIELD_COUNT
} dxl_es_field;

typedef enum { DXL_ES_CHOICE, DXL_ES_BOOL, DXL_ES_NUMBER } dxl_es_kind;

typedef struct {
    const char  *section;           /* "RenderDevice", "Gamepad" or "Performance" */
    const char  *key;               /* member name inside it */
    dxl_es_kind  kind;
    const char  *const *choices;    /* DXL_ES_CHOICE: NULL-terminated */
    /* Built-in default, used when the packaged default file lacks the field. */
    const char  *def_choice;
    double       def_number;        /* bools use 0/1 */
    /* DXL_ES_NUMBER: the range the UI steps through. */
    double       min, max, step;
} dxl_es_info;

const dxl_es_info *dxl_es_describe(dxl_es_field f);

typedef struct dxl_engine_settings dxl_engine_settings;

/* Loads path. default_path (the packaged engine-settings.json.default) is
 * where "reset" values come from, and what a missing or unreadable settings
 * file is seeded with. Never fails: the worst case is the built-in table. */
dxl_engine_settings *dxl_es_open(const char *path, const char *default_path);
void                 dxl_es_free(dxl_engine_settings *s);

const char *dxl_es_path(const dxl_engine_settings *s);
/* Did the file exist, and could it be read? A file that existed but did not
 * parse is reported so the launcher can say it replaced it. */
int  dxl_es_existed(const dxl_engine_settings *s);
int  dxl_es_was_corrupt(const dxl_engine_settings *s);
int  dxl_es_dirty(const dxl_engine_settings *s);
/* Was the field in the file as loaded (with a usable value), rather than
 * filled in from the defaults? */
int  dxl_es_was_present(const dxl_engine_settings *s, dxl_es_field f);

/* Writes the file, creating its directory. Always emits every field. */
int  dxl_es_save(dxl_engine_settings *s, dxl_err *err);

const char *dxl_es_choice(const dxl_engine_settings *s, dxl_es_field f);
int         dxl_es_bool  (const dxl_engine_settings *s, dxl_es_field f);
double      dxl_es_number(const dxl_engine_settings *s, dxl_es_field f);

/* Setters validate: a choice outside the field's list is refused (returns
 * -1), a number is clamped into range. Return 0 when accepted. */
int  dxl_es_set_choice(dxl_engine_settings *s, dxl_es_field f, const char *v);
void dxl_es_set_bool  (dxl_engine_settings *s, dxl_es_field f, int v);
void dxl_es_set_number(dxl_engine_settings *s, dxl_es_field f, double v);

/* Render resolutions to offer on a panel panel_h lines tall: its own height,
 * then the usual heights below it down to 480 lines -- the least Deus Ex's
 * menus fit in (its resolution menu refuses anything under 640x480).
 * Tallest first; returns how many were written. Performance.RenderScale for
 * one is its height over panel_h. */
#define DXL_ES_MIN_RENDER_LINES 480
int dxl_es_render_heights(int panel_h, int *heights, int max);

/* One field, or every field in a section ("RenderDevice", "Gamepad",
 * "Performance"), back to its default. */
void dxl_es_reset(dxl_engine_settings *s, dxl_es_field f);
void dxl_es_reset_section(dxl_engine_settings *s, const char *section);

#endif
