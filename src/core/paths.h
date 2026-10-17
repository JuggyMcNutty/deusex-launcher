/* Path handling across the Windows/POSIX seam.
 *
 * The engine's config is full of Windows paths -- "..\Save\", "CdPath=..\",
 * "Textures\Palettes.utx" -- hard-coded in the original
 * (dx-reverse-info/launch-flow.md, "Platform seams"). Our rule: normalise for
 * *lookup*, but keep writing whatever the engine expects. So
 * dxl_path_from_ini() converts on the way in, and stored values
 * are never rewritten just because we read them.
 *
 * Case: the device's SD card is exFAT, which is case-insensitive, but a host
 * build on ext4 is not, and the ini files name files in whatever case the
 * 2000-era tools used. dxl_path_resolve_ci() bridges that.
 */
#ifndef DXL_PATHS_H
#define DXL_PATHS_H

#include "core/common.h"

/* Backslashes to slashes, in place. */
void  dxl_path_normalize(char *p);
/* A normalised copy of an ini value. Caller frees. */
char *dxl_path_from_ini(const char *value);

/* Joins with a single separator, tolerating a trailing one on base.
 * Caller frees. */
char *dxl_path_join(const char *base, const char *leaf);

/* Directory part of a path (no trailing slash), or "." . Caller frees. */
char *dxl_path_dirname(const char *p);
/* Pointer into p, after the last separator. */
const char *dxl_path_basename(const char *p);

int  dxl_path_exists(const char *p);
int  dxl_path_is_dir(const char *p);
/* Size in bytes, or -1. Mirrors GFileManager->FileSize. */
long dxl_path_size(const char *p);

/* Resolves <dir>/<leaf> case-insensitively: returns the real path if an entry
 * matching leaf (ignoring case) exists, else NULL. An exact hit skips the
 * directory scan. Caller frees. */
char *dxl_path_resolve_ci(const char *dir, const char *leaf);

/* mkdir -p. Returns 0 if the directory exists afterwards. */
int  dxl_path_mkdirs(const char *p);

/* Copies a file byte for byte, replacing to. A failed copy removes the
 * partial destination rather than leaving half a config behind. */
int  dxl_path_copy(const char *from, const char *to, dxl_err *err);

/* Copies the files of a folder into another, made if missing, and its
 * folders' in turn; a file already there is replaced. Counts the files
 * copied into *copied (may be NULL). */
int  dxl_path_copy_dir(const char *from, const char *to, int *copied, dxl_err *err);

/* Seconds since the epoch the file was last changed; -1 when it is not
 * there. */
long dxl_path_mtime(const char *p);

/* Replaces path with data whole or not at all: written to a temporary file
 * beside it (mkstemp), synced, given the old file's mode (0644 for a new
 * one), renamed over it, and the folder synced. A crash or a power cut
 * leaves the old file or the new one, never half of either. */
int  dxl_path_write_atomic(const char *path, const void *data, size_t len, dxl_err *err);

/* Full path of the running executable, for the safe-mode re-exec
 * (GModuleFilename in the original). Caller frees; NULL if undeterminable. */
char *dxl_path_self(void);

#endif
