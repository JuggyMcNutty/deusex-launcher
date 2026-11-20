/* core/patch.h: a patch's operations applied in memory, every number
 * checked against both files before a byte is copied, and the result
 * written only once its SHA-1 is the patch's. */
#include "core/patch.h"
#include "core/paths.h"
#include "core/sha1.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KIND_ADD     0
#define KIND_COPY    1      /* from the diagonal: the last copy's end, and the bytes added since */
#define KIND_COPY_AT 2      /* from the diagonal and a distance */

static uint64_t le(const unsigned char *p, int n) {
    uint64_t v = 0;
    for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i];
    return v;
}

/* A LEB128 number at *pos, of nine bytes at most (63 bits); -1 for one cut
 * short or longer. */
static int number(const unsigned char *p, size_t len, size_t *pos, uint64_t *out) {
    uint64_t v = 0;
    for (int shift = 0; shift < 63; shift += 7) {
        if (*pos >= len) return -1;
        unsigned char b = p[(*pos)++];
        v |= (uint64_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) {
            *out = v;
            return 0;
        }
    }
    return -1;
}

int dxl_patch_header(const unsigned char *data, size_t len, dxl_patch_info *out, dxl_err *err) {
    if (len < DXL_PATCH_HEADER || memcmp(data, DXL_PATCH_MAGIC, 8) != 0) {
        dxl_err_set(err, "not a patch of the launcher's");
        return -1;
    }
    if (le(data + 8, 4) != DXL_PATCH_VERSION) {
        dxl_err_set(err, "a patch of another version (%u)", (unsigned)le(data + 8, 4));
        return -1;
    }
    out->source_size = le(data + 12, 8);
    dxl_sha1_hex(data + 20, out->source_sha1);
    out->result_size = le(data + 40, 8);
    dxl_sha1_hex(data + 48, out->result_sha1);
    return 0;
}

unsigned char *dxl_patch_read_file(const char *path, size_t max, size_t *len, dxl_err *err) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot read %s: %s", path, strerror(errno));
        return NULL;
    }
    size_t cap = 1 << 16, n = 0;
    unsigned char *buf = dxl_xmalloc(cap);
    for (;;) {
        if (n == cap) {
            if (cap > max) break;
            cap *= 2;
            buf = dxl_xrealloc(buf, cap);
        }
        size_t got = fread(buf + n, 1, cap - n, f);
        n += got;
        if (got == 0) break;
    }
    int bad = ferror(f);
    fclose(f);
    if (bad || n > max) {
        dxl_err_set(err, bad ? "cannot read %s" : "%s is larger than it can be", path);
        free(buf);
        return NULL;
    }
    *len = n;
    return buf;
}

int dxl_patch_info_read(const char *path, dxl_patch_info *out, dxl_err *err) {
    unsigned char head[DXL_PATCH_HEADER];
    FILE *f = fopen(path, "rb");
    if (!f) {
        dxl_err_set(err, "cannot read %s: %s", path, strerror(errno));
        return -1;
    }
    size_t got = fread(head, 1, sizeof head, f);
    fclose(f);
    return dxl_patch_header(head, got, out, err);
}

int dxl_patch_apply_mem(const unsigned char *source, size_t source_len, const unsigned char *patch,
                        size_t patch_len, unsigned char **out, size_t *out_len, dxl_err *err) {
    dxl_patch_info info;
    uint8_t digest[20];
    char hex[41];
    *out = NULL;
    if (dxl_patch_header(patch, patch_len, &info, err) != 0) return -1;
    dxl_sha1 s;
    dxl_sha1_init(&s);
    dxl_sha1_update(&s, source, source_len);
    dxl_sha1_final(&s, digest);
    dxl_sha1_hex(digest, hex);
    if (source_len != info.source_size || strcmp(hex, info.source_sha1) != 0) {
        dxl_err_set(err, "the file is not the one the patch is for (its SHA-1 %s, not %s)", hex, info.source_sha1);
        return -1;
    }
    if (info.result_size > DXL_PATCH_MAX) {
        dxl_err_set(err, "the patch's result is larger than it can be");
        return -1;
    }
    size_t size = (size_t)info.result_size, done = 0, pos = DXL_PATCH_HEADER;
    uint64_t cursor = 0, added = 0;
    unsigned char *r = dxl_xmalloc(size ? size : 1);
    while (pos < patch_len) {
        uint64_t v, len, distance = 0;
        if (number(patch, patch_len, &pos, &v) != 0) goto cut;
        unsigned kind = (unsigned)(v & 3);
        len = v >> 2;
        if (len == 0 || kind > KIND_COPY_AT || len > size - done) goto wrong;
        if (kind == KIND_ADD) {
            if (len > patch_len - pos) goto cut;
            memcpy(r + done, patch + pos, (size_t)len);
            pos += (size_t)len;
            added += len;
        } else {
            if (kind == KIND_COPY_AT && number(patch, patch_len, &pos, &distance) != 0) goto cut;
            /* Zigzag-signed: 0 1 2 3 4 for 0 -1 1 -2 2. The diagonal may
             * lie past the source's end (bytes added after a copy that
             * reached it), a distance back bringing it in, as
             * tools/dxpatch.py makes and applies it. cursor + added and
             * ahead stay far below 2^64: the result is at most
             * DXL_PATCH_MAX, the distance under 2^63. */
            uint64_t back = distance & 1 ? (distance >> 1) + 1 : 0, ahead = distance & 1 ? 0 : distance >> 1;
            uint64_t from = cursor + added;
            if (back > from) goto wrong;
            from = from - back;
            if (from > source_len || ahead > source_len - from) goto wrong;
            from += ahead;
            if (len > source_len - from) goto wrong;
            memcpy(r + done, source + from, (size_t)len);
            cursor = from + len;
            added = 0;
        }
        done += (size_t)len;
    }
    if (done != size) goto cut;
    dxl_sha1_init(&s);
    dxl_sha1_update(&s, r, size);
    dxl_sha1_final(&s, digest);
    dxl_sha1_hex(digest, hex);
    if (strcmp(hex, info.result_sha1) != 0) {
        dxl_err_set(err, "the patch's result is not the one it names (its SHA-1 %s, not %s)", hex,
                    info.result_sha1);
        free(r);
        return -1;
    }
    *out = r;
    *out_len = size;
    return 0;
cut:
    dxl_err_set(err, "the patch is cut short");
    free(r);
    return -1;
wrong:
    dxl_err_set(err, "the patch is damaged: an operation reaches past its files");
    free(r);
    return -1;
}

int dxl_patch_apply(const char *source_path, const char *patch_path, const char *result_path, dxl_err *err) {
    const size_t max = DXL_PATCH_MAX;
    size_t source_len = 0, patch_len = 0, out_len = 0;
    unsigned char *out = NULL;
    unsigned char *patch = dxl_patch_read_file(patch_path, max, &patch_len, err);
    unsigned char *source = patch ? dxl_patch_read_file(source_path, max, &source_len, err) : NULL;
    int rc = source ? dxl_patch_apply_mem(source, source_len, patch, patch_len, &out, &out_len, err) : -1;
    if (rc == 0) rc = dxl_path_write_atomic(result_path, out, out_len, err);
    free(out);
    free(source);
    free(patch);
    return rc;
}
