/* The game's localised strings: the .int files in System/.
 *
 * Every word the launcher shows is the game's own, from Startup.int (its
 * pages), Window.int (the buttons, the CD prompt) and each driver's .int
 * (a renderer's ClassCaption). A .int file is read as UE1's config reader
 * reads any ini: a value wrapped in double quotes loses them
 * (WorldHigh="High detail textures"). Localize looks in <Package>.<language>
 * first, then <Package>.int; the language is [Engine.Engine] Language, int
 * by default. A string that is nowhere reads <?int?Package.Section.Key?>, as
 * the original's does, unless the caller marks it optional.
 */
#ifndef DXL_LOCALIZE_H
#define DXL_LOCALIZE_H

#include "core/common.h"

typedef struct dxl_loc dxl_loc;

/* language may be NULL or empty for "int". Files load on first use. */
dxl_loc    *dxl_loc_open(const char *system_dir, const char *language);
void        dxl_loc_free(dxl_loc *l);

/* The string, owned by l and valid until dxl_loc_free. Optional strings
 * that are missing read "". */
const char *dxl_loc_get(dxl_loc *l, const char *package, const char *section,
                        const char *key, int optional);

/* LocalizeGeneral: the [General] section. */
const char *dxl_loc_general(dxl_loc *l, const char *package, const char *key);

/* Window.h's LineFormat, which a page applies to every text it localises:
 * "\n" becomes a line break, and a backslash before anything else leaves
 * just that character. Caller frees. */
char *dxl_loc_line_format(const char *s);

#endif
