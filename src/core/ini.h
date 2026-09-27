/* UE1-style .ini read/write.
 *
 * Two properties drive the whole design, both taken from the shipped files:
 *
 *   - CRLF. System/DeusEx.ini is a Windows file. The engine rewrites it too,
 *     so we must not silently convert it.
 *   - Repeated keys. "Paths=" appears five times in [Core.System]. A key is
 *     not unique within a section.
 *
 * The representation is therefore the *line list*, not a section->key map: a
 * loaded file that is saved without modification comes back byte-identical,
 * including comments, blank lines and whatever spacing the original had.
 * Only lines we actually change get regenerated.
 *
 * Lookup is ASCII case-insensitive, matching FConfigCacheIni.
 */
#ifndef DXL_INI_H
#define DXL_INI_H

#include "core/common.h"

typedef struct dxl_ini dxl_ini;

dxl_ini *dxl_ini_new(void);
/* Loads path. A missing file is an error; use dxl_ini_new() for a fresh one. */
dxl_ini *dxl_ini_load(const char *path, dxl_err *err);
/* Parses an in-memory image. Used by the tests and by anything that already
 * has the bytes. */
dxl_ini *dxl_ini_parse(const char *text, size_t len);
void     dxl_ini_free(dxl_ini *ini);

/* Serialises to a freshly allocated buffer; caller frees. Never fails. */
char *dxl_ini_render(const dxl_ini *ini, size_t *out_len);
int   dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err);

/* First value for key, or NULL. The returned pointer is owned by the ini and
 * is invalidated by the next dxl_ini_set* on the same object. */
const char *dxl_ini_get(const dxl_ini *ini, const char *section, const char *key);
int   dxl_ini_get_int (const dxl_ini *ini, const char *section, const char *key, int fallback);
int   dxl_ini_get_bool(const dxl_ini *ini, const char *section, const char *key, int fallback);

/* All values for a repeated key, in file order. Returns the count; writes at
 * most max entries into out. Pass out=NULL to count only. */
size_t dxl_ini_get_all(const dxl_ini *ini, const char *section, const char *key,
                       const char **out, size_t max);

/* Replaces the first occurrence, or appends to the section, creating the
 * section at end of file if needed. */
void dxl_ini_set     (dxl_ini *ini, const char *section, const char *key, const char *value);
void dxl_ini_set_int (dxl_ini *ini, const char *section, const char *key, int value);
/* Writes "True"/"False" -- the spelling the engine reads back. */
void dxl_ini_set_bool(dxl_ini *ini, const char *section, const char *key, int value);
/* Appends without replacing, for array-style repeated keys. */
void dxl_ini_append  (dxl_ini *ini, const char *section, const char *key, const char *value);

/* Sets every key/value pair of src onto dst (first occurrence semantics, as
 * dxl_ini_set). Used to rebuild a config from Default.ini while keeping the
 * values a partial file already carried. */
void dxl_ini_overlay(dxl_ini *dst, const dxl_ini *src);

int  dxl_ini_has_section(const dxl_ini *ini, const char *section);
/* Removes every key line in the section, keeping the header. Mirrors
 * FConfigCache::EmptySection. */
void dxl_ini_empty_section(dxl_ini *ini, const char *section);

/* True once any set/append/empty has run. */
int  dxl_ini_dirty(const dxl_ini *ini);

#endif
