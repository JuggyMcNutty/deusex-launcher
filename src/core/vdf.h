/* Valve's KeyValues text, as Steam writes its libraryfolders.vdf and its
 * appmanifest_<id>.acf: a key and its value, or a key and a block of more
 * of them in braces.
 *
 *   "libraryfolders"
 *   {
 *       "0" { "path" "/home/me/.local/share/Steam" "apps" { "6910" "..." } }
 *       "1" "/mnt/games/SteamLibrary"            (an older Steam's)
 *   }
 *
 * Keys and values are quoted, or bare words; \\ \" \n \t escaped in quotes;
 * // a comment to the line's end; a [$PLATFORM] condition after a value is
 * skipped. Keys match without regard to case, as KeyValues' do. Read only:
 * nothing here writes one.
 */
#ifndef DXL_VDF_H
#define DXL_VDF_H

#include "core/common.h"

typedef struct dxl_vdf dxl_vdf;

/* The file's top level as a block of no key. NULL, and err, when it does
 * not read: a brace or a quote left open, a key with nothing after it. */
dxl_vdf *dxl_vdf_parse(const char *text, size_t len, dxl_err *err);
dxl_vdf *dxl_vdf_load(const char *path, dxl_err *err);
void     dxl_vdf_free(dxl_vdf *v);

const char *dxl_vdf_key(const dxl_vdf *v);
/* A key's value; NULL for a block. */
const char *dxl_vdf_value(const dxl_vdf *v);
/* A block's keys, in the file's order; 0 for a value. */
size_t         dxl_vdf_count(const dxl_vdf *v);
const dxl_vdf *dxl_vdf_at(const dxl_vdf *v, size_t i);
/* The block's first key of that name, NULL for none. */
const dxl_vdf *dxl_vdf_get(const dxl_vdf *v, const char *key);
/* That key's value, NULL when it is not there or is a block. */
const char    *dxl_vdf_get_string(const dxl_vdf *v, const char *key);

#endif
