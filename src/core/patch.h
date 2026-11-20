/* The launcher's binary patches: one file made from another, byte for byte,
 * by a patch made with tools/dxpatch.py (its format there, and in
 * docs/LAUNCHER.md's DXController). A patch names the one source it takes
 * and the one result it gives, each by its size and SHA-1: any other
 * source is refused, and a result that is not the one named is never
 * written. */
#ifndef DXL_PATCH_H
#define DXL_PATCH_H

#include "core/common.h"

#include <stddef.h>
#include <stdint.h>

#define DXL_PATCH_MAGIC   "DXLPATCH"
#define DXL_PATCH_VERSION 1
#define DXL_PATCH_HEADER  68    /* the magic, the version, two sizes and two SHA-1s */
/* The largest source, patch or result taken, in bytes: twice the largest
 * package the game ships, and room to spare. A header naming a larger
 * result is refused before anything is allocated. */
#define DXL_PATCH_MAX     ((size_t)256 << 20)

typedef struct {
    uint64_t source_size, result_size;
    char     source_sha1[41], result_sha1[41];   /* lower-case hex */
} dxl_patch_info;

/* The header of the patch in data: -1, with why, for none of this format. */
int dxl_patch_header(const unsigned char *data, size_t len, dxl_patch_info *out, dxl_err *err);
/* The header of the patch file at path. */
int dxl_patch_info_read(const char *path, dxl_patch_info *out, dxl_err *err);

/* The result in memory (caller frees *out), from a source of exactly the
 * patch's size and SHA-1, every operation inside both files, the result
 * exactly the patch's size and SHA-1: else NULL and -1, with why. */
int dxl_patch_apply_mem(const unsigned char *source, size_t source_len, const unsigned char *patch,
                        size_t patch_len, unsigned char **out, size_t *out_len, dxl_err *err);

/* The patch at patch_path applied to the file source_path, its result
 * written to result_path whole or not at all (core/paths.h's
 * dxl_path_write_atomic): a failure, a kill or a full disk leaves the file
 * there before, or none, never a short one. */
int dxl_patch_apply(const char *source_path, const char *patch_path, const char *result_path, dxl_err *err);

/* A file read whole (caller frees), at most max bytes: NULL, with why, for
 * one that cannot be, or is larger. */
unsigned char *dxl_patch_read_file(const char *path, size_t max, size_t *len, dxl_err *err);

#endif
