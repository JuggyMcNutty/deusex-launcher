/* The desktop's trash, as freedesktop.org's Trash spec has it: what the
 * launcher deletes of the player's -- a profile's folders, a save, a shot --
 * goes there, to be put back from the file manager, never removed outright.
 *
 * The home trash is <data home>/Trash: files/<name>, the thing itself, and
 * info/<name>.trashinfo, where it was ([Trash Info] Path, its URL-escaped
 * absolute path, and DeletionDate). A name taken already there gets ".2",
 * ".3"...: the .trashinfo is made first, by that name alone (O_EXCL), so two
 * deletions never take one name. Only what is on the trash's own
 * filesystem goes, by a rename.
 */
#ifndef DXL_TRASH_H
#define DXL_TRASH_H

#include "core/common.h"

/* Whether path can go to data_home's trash: it is there, and on the same
 * filesystem. */
int dxl_trash_can(const char *path, const char *data_home);

/* path into data_home's trash, under the name put in name (may be NULL).
 * 0, or -1 and why, with nothing moved. */
int dxl_trash(const char *path, const char *data_home, char *name, size_t name_size, dxl_err *err);
/* What went to the trash under name back at path, its .trashinfo gone. */
int dxl_trash_undo(const char *data_home, const char *name, const char *path, dxl_err *err);

/* The n paths there into data_home's trash, all of them or none: each
 * checked before any goes, and those gone put back when a later one
 * cannot go. 0, or -1 and why. */
int dxl_trash_all(char *const paths[], int n, const char *data_home, dxl_err *err);

#endif
