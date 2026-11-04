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
 * Only lines we actually change get regenerated. A NUL in a line ends what
 * is read of it, not what is written.
 *
 * Text is UTF-8 in memory, read and written as the original's Core does
 * (appLoadFileToString 0x101488c0, appSaveStringToFile 0x10148c40;
 * core-dll.md): a file of even size beginning FF FE or FE FF is UTF-16,
 * any other a byte a character, Latin-1; one is written a byte a character
 * when every character is below U+0100, else UTF-16 little-endian after
 * FF FE.
 *
 * Lookup is ASCII case-insensitive, matching FConfigCacheIni. A key given
 * twice reads at its last value and is set there, and a section written
 * twice is one section, its blocks read in order, as the original reads
 * them.
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

/* The file's bytes as dxl_ini_save writes them (UTF-16 holds NULs: take
 * out_len), freshly allocated; caller frees. Never fails. */
char *dxl_ini_render(const dxl_ini *ini, size_t *out_len);
int   dxl_ini_save(const dxl_ini *ini, const char *path, dxl_err *err);

/* The value the original reads for key, the last given in the section's
 * blocks, or NULL. The returned pointer is owned by the ini and is
 * invalidated by the next dxl_ini_set* on the same object. */
const char *dxl_ini_get(const dxl_ini *ini, const char *section, const char *key);
int   dxl_ini_get_int (const dxl_ini *ini, const char *section, const char *key, int fallback);

/* All values for a repeated key, in file order across the section's blocks.
 * Returns the count; writes at
 * most max entries into out. Pass out=NULL to count only. */
size_t dxl_ini_get_all(const dxl_ini *ini, const char *section, const char *key,
                       const char **out, size_t max);

/* Replaces the value the original reads (the last given), or appends to the
 * section's last block, creating the section at end of file if needed. */
void dxl_ini_set     (dxl_ini *ini, const char *section, const char *key, const char *value);
void dxl_ini_set_int (dxl_ini *ini, const char *section, const char *key, int value);
/* Writes "True"/"False" -- the spelling the engine reads back. */
void dxl_ini_set_bool(dxl_ini *ini, const char *section, const char *key, int value);
/* Appends without replacing, for array-style repeated keys. */
void dxl_ini_append  (dxl_ini *ini, const char *section, const char *key, const char *value);

/* True once any set or append has run. */
int  dxl_ini_dirty(const dxl_ini *ini);

#endif
