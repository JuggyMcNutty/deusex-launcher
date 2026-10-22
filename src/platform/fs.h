/* The file system where POSIX and Windows differ: the rest of the launcher
 * calls what both C libraries do alike -- stdio, stat, opendir and readdir,
 * remove, rmdir -- itself, and these for the rest. platform/posix/fs.c is
 * POSIX's, platform/win32/fs.c Windows'.
 *
 * Paths are UTF-8, with '/' between names, as everything above the
 * platform keeps them: a root is "/", and on Windows "C:/" or
 * "//server/share/". The platform's own answers come so too.
 */
#ifndef DXL_FS_H
#define DXL_FS_H

#include "core/common.h"

#include <stdio.h>

/* What a program's file is named with: "" here, ".exe" on Windows. */
#ifdef _WIN32
#define DXL_EXE_SUFFIX ".exe"
#else
#define DXL_EXE_SUFFIX ""
#endif

/* The length of path's root ("/" 1, "C:/" 3); 0 when path is relative. */
size_t dxl_fs_root_len(const char *path);
int    dxl_fs_is_absolute(const char *path);

/* The user's home folder, or NULL when there is none. */
const char *dxl_fs_home(void);

/* A path from outside the launcher -- the environment, a command line --
 * as the launcher keeps paths: a copy of it here, its '\' made '/' on
 * Windows. Caller frees. */
char *dxl_fs_path(const char *outside);

/* dir/leaf by the name the file system stores: leaf as it is where names
 * keep their case, the stored one where they take any (Windows'); NULL when
 * nothing is there by that name. Caller frees. */
char *dxl_fs_stored(const char *dir, const char *leaf);

/* The running program's file. Caller frees; NULL if undeterminable. */
char *dxl_fs_self(void);

/* The file or folder path is, links followed, by its absolute name; NULL
 * when it is not there. Caller frees. */
char *dxl_fs_real(const char *path);

/* The working folder. Caller frees; NULL if undeterminable. */
char *dxl_fs_cwd(void);

/* Makes one folder. 0, or -1 with errno (EEXIST when something is there). */
int dxl_fs_mkdir(const char *path);

/* Renames from to to, replacing a file there. 0, or -1 with errno. */
int dxl_fs_replace(const char *from, const char *to);

/* Whether path is a link itself, not followed. */
int dxl_fs_is_link(const char *path);

/* Makes link a link to the folder target. 0, or -1 with errno: EPERM,
 * EOPNOTSUPP or ENOSYS where the file system or the user can make none. */
int dxl_fs_link_dir(const char *target, const char *link);

/* Whether path is a file this user may run. */
int dxl_fs_is_program(const char *path);

/* path opened to append to, never handed on to a program the launcher
 * starts; NULL when it cannot be. */
FILE *dxl_fs_open_log(const char *path);

/* Replaces path with data whole or not at all: written to a temporary file
 * beside it, synced, given the old file's mode where files have modes (0644
 * for a new one), put in its place, and the folder synced. A crash or a
 * power cut leaves the old file or the new one, never half of either. */
int dxl_fs_write_atomic(const char *path, const void *data, size_t len, dxl_err *err);

/* The tops of the file system, NULL-terminated: "/" alone here, each
 * drive's on Windows. Caller frees with dxl_fs_roots_free. */
char **dxl_fs_roots(void);
void   dxl_fs_roots_free(char **roots);

/* For the tests and the tools: the system's folder for temporary files;
 * caller frees. */
char *dxl_fs_temp_dir(void);
/* A file's time of its last change, in seconds since the epoch. */
int   dxl_fs_set_mtime(const char *path, long when);
/* A file, or a folder and all in it; 0 when nothing is left there. */
int   dxl_fs_remove_tree(const char *path);

#endif
