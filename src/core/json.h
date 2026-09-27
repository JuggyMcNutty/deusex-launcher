/* A small JSON document model, for Surreal Engine's Settings.json.
 *
 * The engine keeps its own settings -- renderer backend, VSync, lighting,
 * gamma, and (in our fork) the gamepad -- in a JSON file rather than in the
 * game's ini. It parses that file inside a catch-all: one malformed byte and
 * every setting silently reverts to the engine default, including the 4x MSAA
 * that turns the PowerVR's output to speckle. So this module is strict on
 * output and keeps whatever it does not understand.
 *
 * Objects keep their members in file order, and members this launcher does
 * not know about are carried through untouched. Numbers remember their
 * original spelling so a value that was not changed is written back exactly.
 */
#ifndef DXL_JSON_H
#define DXL_JSON_H

#include "core/common.h"

typedef enum {
    DXL_JSON_NULL,
    DXL_JSON_BOOL,
    DXL_JSON_NUMBER,
    DXL_JSON_STRING,
    DXL_JSON_ARRAY,
    DXL_JSON_OBJECT
} dxl_json_type;

typedef struct dxl_json dxl_json;

dxl_json *dxl_json_parse(const char *text, size_t len, dxl_err *err);
/* A missing file is an error, like dxl_ini_load. */
dxl_json *dxl_json_load(const char *path, dxl_err *err);
void      dxl_json_free(dxl_json *v);

/* Two-space indented, members in order. Caller frees. */
char *dxl_json_render(const dxl_json *v);
/* Writes via a temporary file and rename, so a crash mid-write cannot leave
 * the engine a truncated file to choke on. */
int   dxl_json_save(const dxl_json *v, const char *path, dxl_err *err);

dxl_json     *dxl_json_new_object(void);
dxl_json_type dxl_json_kind(const dxl_json *v);

/* Object member lookup, exact key match. NULL if v is not an object or has
 * no such member. */
dxl_json *dxl_json_get(const dxl_json *obj, const char *key);

/* The member object named key, created (appended) if absent. If a member of
 * that name exists but is not an object, it is replaced by an empty one. */
dxl_json *dxl_json_child(dxl_json *obj, const char *key);

/* Typed reads with a fallback for absent or mistyped members. */
const char *dxl_json_get_string(const dxl_json *obj, const char *key, const char *fallback);
int         dxl_json_get_bool  (const dxl_json *obj, const char *key, int fallback);
double      dxl_json_get_number(const dxl_json *obj, const char *key, double fallback);

/* Replace the member in place, keeping its position, or append it. */
void dxl_json_set_string(dxl_json *obj, const char *key, const char *value);
void dxl_json_set_bool  (dxl_json *obj, const char *key, int value);
void dxl_json_set_number(dxl_json *obj, const char *key, double value);

#endif
