/* The tests' writer of UE1 packages' bytes (core/upkg.h's format): a
 * growing buffer, little-endian numbers, compact indices, strings and
 * property tags as the format keeps them. */
#ifndef DXL_TEST_UPKG_WRITER_H
#define DXL_TEST_UPKG_WRITER_H

#include "core/upkg.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t *d;
    size_t   n, cap;
} buf;

static inline void put(buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 64;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static inline void u8(buf *b, uint8_t v) { put(b, &v, 1); }
static inline void u16(buf *b, uint16_t v) { u8(b, (uint8_t)v); u8(b, (uint8_t)(v >> 8)); }
static inline void u32(buf *b, uint32_t v) { u16(b, (uint16_t)v); u16(b, (uint16_t)(v >> 16)); }
static inline void u64(buf *b, uint64_t v) { u32(b, (uint32_t)v); u32(b, (uint32_t)(v >> 32)); }

static inline void idx(buf *b, int32_t v) {
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    uint8_t first = (uint8_t)((v < 0 ? 0x80 : 0) | (a & 0x3f) | (a >= 0x40 ? 0x40 : 0));
    u8(b, first);
    a >>= 6;
    while (a) {
        u8(b, (uint8_t)((a & 0x7f) | (a >= 0x80 ? 0x80 : 0)));
        a >>= 7;
    }
}

static inline void str(buf *b, const char *s) {
    idx(b, (int32_t)strlen(s) + 1);
    put(b, s, strlen(s) + 1);
}

/* A property's tag, in the format's order: its name, its type and size
 * code, a struct's name, a size of a byte (code 5), an array index. */
static inline void tag(buf *b, int name, int type, int size_code, int index, int struct_name, int size) {
    idx(b, name);
    u8(b, (uint8_t)(type | size_code << 4 | (index ? 0x80 : 0)));
    if (type == DXL_UPROP_STRUCT) idx(b, struct_name);
    if (size_code == 5) u8(b, (uint8_t)size);
    if (index) u8(b, (uint8_t)index);
}

#endif
