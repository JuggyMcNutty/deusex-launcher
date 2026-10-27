/* DXEngine's own settings: Settings.json in its folder, the profile's
 * engine folder (--userdir), which the engine reads once, as its window
 * opens, and never writes (its LauncherSettings.cpp).
 *
 * The engine takes its renderer from here where the file names one
 * (RenderDevice.Type), else by the game's ini (GameRenderDevice,
 * core/renderers.h), and DXEngine's own choices: anti-aliasing, lighting,
 * the gamma curve, VSync, bloom, HDR, the pad, the render scale. The rest
 * of the game's display settings are the inis' (core/settings_table.c).
 *
 * Every field is described by a table entry -- its JSON location, kind,
 * allowed values and range -- so the UI can build its rows from the table
 * instead of hard-coding each one. Values are the engine's own spellings,
 * exact case (LauncherSettings.cpp): "MSAA4x", "OneX", "XOpenGL", ...
 *
 * The engine parses the file inside a catch-all, so a file it cannot read
 * costs every setting at once; a RenderDevice bool or number it cannot
 * find reads as false or 0, the rest as the engine's default. That is why
 * a save always writes every member of every section here, but a Type
 * none chose (dxl_es_names_type), and why a file neither can read is
 * replaced rather than left alone -- kept beside it, never written over.
 * A member the file lacks takes the packaged default, so a field added
 * later reaches an existing install with the port's value.
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
    DXL_ES_CONTRAST,            /* the present's colour correction, 1 each the picture unchanged */
    DXL_ES_SATURATION,
    DXL_ES_GAMMA_OFFSET,        /* added to the present's gamma, every channel's */
    DXL_ES_GAMMA_RED,           /* and each one's */
    DXL_ES_GAMMA_GREEN,
    DXL_ES_GAMMA_BLUE,
    DXL_ES_LOD_BIAS,            /* the textures' mip bias */
    /* Gamepad -- read by DXEngine's controller support */
    DXL_ES_PAD_ENABLED,
    DXL_ES_PAD_DEADZONE,
    DXL_ES_PAD_LOOK_X,
    DXL_ES_PAD_LOOK_Y,
    DXL_ES_PAD_INVERT_Y,
    DXL_ES_PAD_CURSOR_SPEED,
    /* Performance -- DXEngine's speed-for-fidelity choices */
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
 * parse is reported so the launcher can say it replaced it: the reader's
 * words for it, and where a save kept it (dxl_es_save), else "" and NULL. */
int  dxl_es_existed(const dxl_engine_settings *s);
int  dxl_es_was_corrupt(const dxl_engine_settings *s);
const char *dxl_es_problem(const dxl_engine_settings *s);
const char *dxl_es_kept(const dxl_engine_settings *s);
int  dxl_es_dirty(const dxl_engine_settings *s);
/* Was the field in the file as loaded (with a usable value), rather than
 * filled in from the defaults? */
int  dxl_es_was_present(const dxl_engine_settings *s, dxl_es_field f);

/* Whether the file names the renderer's Type: as read, or chosen since
 * (dxl_es_set_choice). With none the engine takes the game's ini's
 * GameRenderDevice, and a save writes none; a Type the engine cannot take
 * is dropped as the file is read. dxl_es_choice gives the defaults file's
 * for none. */
int  dxl_es_names_type(const dxl_engine_settings *s);

/* Writes the file, creating its directory. Every field but a Type none
 * named. A file there that could not be read is kept first, its path with
 * DXL_ES_KEPT after it, or nothing is written. */
#define DXL_ES_KEPT ".unreadable"
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

/* The anti-aliasing the engine takes from the Settings.json at path: the
 * file's, or the engine's own MSAA4x where it reads none there -- no file,
 * one it cannot parse, a member it does not know. */
void dxl_es_antialias_taken(const char *path, char *out, size_t size);
/* Antialias Off in the Settings.json at path, made from default_path when
 * it is missing or unreadable: for a GPU whose multisampling is broken.
 * 1 when the file changed, 2 when it replaced one that could not be read
 * (kept, dxl_es_save), 0 when the engine takes Off from it already, -1
 * when it could not be written. */
int  dxl_es_antialias_off(const char *path, const char *default_path, dxl_err *err);

#endif
