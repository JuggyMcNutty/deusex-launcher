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
/* Whole or not at all (dxl_path_write_atomic). */
int   dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err);

/* Every line's terminator, and new lines', made eol ("\n" or "\r\n"); an
 * unterminated last line stays unterminated. The desktop launcher writes
 * its profiles' inis LF, as the engine writes them back. */
void  dxl_ini_set_eol(dxl_ini *ini, const char *eol);

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

/* Appends every line of src's section to dst's, repeated keys and all,
 * creating the section in dst if it has none. */
void dxl_ini_copy_section(dxl_ini *dst, const dxl_ini *src, const char *section);

int  dxl_ini_has_section(const dxl_ini *ini, const char *section);
/* The key=value lines of a section, in file order: at most max of each into
 * keys and values (either may be NULL), the count returned. The strings
 * are the ini's, valid until it changes. */
size_t dxl_ini_pairs(const dxl_ini *ini, const char *section, const char **keys, const char **values, size_t max);
/* The names of the sections that start with prefix (any case), in file
 * order, each once. Returns the count; writes at most max into out. The
 * names are the ini's, valid until it changes. */
size_t dxl_ini_sections(const dxl_ini *ini, const char *prefix, const char **out, size_t max);
/* Removes every key line in the section, keeping the header. Mirrors
 * FConfigCache::EmptySection. */
void dxl_ini_empty_section(dxl_ini *ini, const char *section);

/* Removes every line of the key in the section. */
void dxl_ini_remove(dxl_ini *ini, const char *section, const char *key);

/* True once any set/append/empty/remove has run. */
int  dxl_ini_dirty(const dxl_ini *ini);
/* The ini as its file now is (saved): not dirty until the next change. */
void dxl_ini_clean(dxl_ini *ini);

#endif
