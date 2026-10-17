/* SHA-1, for telling Deus Ex's builds apart by DeusEx.exe (core/gamedb.h),
 * as the engine itself does. Not for anything that needs a secure hash. */
#ifndef DXL_SHA1_H
#define DXL_SHA1_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t h[5];
    uint64_t length;      /* bytes so far */
    uint8_t  block[64];
    size_t   used;        /* bytes waiting in block */
} dxl_sha1;

void dxl_sha1_init(dxl_sha1 *s);
void dxl_sha1_update(dxl_sha1 *s, const void *data, size_t n);
void dxl_sha1_final(dxl_sha1 *s, uint8_t digest[20]);

/* The digest as 40 lower-case hex digits and a NUL. */
void dxl_sha1_hex(const uint8_t digest[20], char hex[41]);

/* A file's, as hex; -1 when it cannot be read. */
int  dxl_sha1_file(const char *path, char hex[41]);

#endif
