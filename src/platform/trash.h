/* The desktop's trash: what the launcher deletes of the player's -- a
 * profile's folders, a save, a shot -- goes there, to be put back from it,
 * never removed outright. Windows' is its Recycle Bin
 * (platform/win32/trash.c), which says nothing back of where it put a
 * thing: the launcher puts nothing back there, data_home is unused and a
 * name comes back empty.
 *
 * POSIX's (platform/posix/trash.c) is freedesktop.org's Trash spec's. The
 * home trash is <data home>/Trash: files/<name>, the thing itself, and
 * info/<name>.trashinfo, where it was ([Trash Info] Path, its URL-escaped
 * absolute path, and DeletionDate). A name taken already there gets ".2",
 * ".3"...: the .trashinfo is made first, by that name alone (O_EXCL), so two
 * deletions never take one name. Only what is on the trash's own
 * filesystem goes, by a rename.
 *
 * What is on another file system goes to that file system's own trash, at
 * the top of it ($topdir, its mount point): $topdir/.Trash/<uid> where an
 * administrator made $topdir/.Trash, a folder with its sticky bit set and
 * no link, else $topdir/.Trash-<uid>, made at need; its Path relative to
 * $topdir. Where none can be made (a file system read-only, its top not
 * the user's to write), nothing goes but by the player's yes to deleting
 * it for good (bugs.md LAPP.26).
 */
#ifndef DXL_TRASH_H
#define DXL_TRASH_H

#include <stdint.h>
#include "core/common.h"

/* Whether path can go to data_home's trash: it is there, and on the same
 * filesystem. */
int dxl_trash_can(const char *path, const char *data_home);

/* path into data_home's trash, under the name put in name (may be NULL).
 * 0, or -1 and why, with nothing moved. */
int dxl_trash(const char *path, const char *data_home, char *name, size_t name_size, dxl_err *err);
/* What went to the trash under name back at path, its .trashinfo gone. */
int dxl_trash_undo(const char *data_home, const char *name, const char *path, dxl_err *err);

/* The n paths there into the trash, all of them or none: each
 * checked before any goes, and those gone put back when a later one
 * cannot go. 0, or -1 and why. */
int dxl_trash_all(char *const paths[], int n, const char *data_home, dxl_err *err);

/* dxl_trash_or_delete's answer for paths of which one no trash takes. */
#define DXL_TRASH_NONE 2

/* POSIX's: the n paths there each into the trash of its own file system,
 * all of them or none (dxl_trash_all is this with for_good 0, its
 * DXL_TRASH_NONE a -1): every trash made, every name taken and every .trashinfo
 * written before any moves, and those moved put back when a later one
 * cannot move (err then says what stays where). What no trash takes --
 * none can be made on its file system, or the move to the one there is
 * refused as one across file systems: with for_good 0, nothing done, what
 * moved put back, DXL_TRASH_NONE; with for_good 1, deleted for good once
 * the rest is in its trash. why (may be NULL) gets a line for each that
 * no trash takes, its path and why. 0 done; -1 nothing done, err why; 1
 * the rest done, but one for good not removed whole, err naming it. */
int dxl_trash_or_delete(char *const paths[], int n, const char *data_home, int for_good, char *why,
                        size_t why_size, dxl_err *err);

/* The tests': a folder taken for the top of a file system of its own, NULL
 * but in tests; and a path's mount and the move to a trash as a test gives
 * them, where no test may mount anything. */
extern const char *dxl_trash_test_top;
extern uint64_t (*dxl_trash_test_mount)(const char *path);
extern int (*dxl_trash_test_move)(const char *from, const char *to);

#endif
