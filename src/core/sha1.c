#include "core/sha1.h"

#include <stdio.h>
#include <string.h>

static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void transform(uint32_t h[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)block[i * 4] << 24 | (uint32_t)block[i * 4 + 1] << 16 |
               (uint32_t)block[i * 4 + 2] << 8 | block[i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20)      { f = (b & c) | (~b & d);          k = 0x5a827999; }
        else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ed9eba1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdc; }
        else             { f = b ^ c ^ d;                   k = 0xca62c1d6; }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

void dxl_sha1_init(dxl_sha1 *s) {
    static const uint32_t start[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
    memcpy(s->h, start, sizeof start);
    s->length = 0;
    s->used = 0;
}

void dxl_sha1_update(dxl_sha1 *s, const void *data, size_t n) {
    const uint8_t *p = data;
    s->length += n;
    while (n > 0) {
        size_t take = 64 - s->used < n ? 64 - s->used : n;
        memcpy(s->block + s->used, p, take);
        s->used += take;
        p += take;
        n -= take;
        if (s->used == 64) {
            transform(s->h, s->block);
            s->used = 0;
        }
    }
}

void dxl_sha1_final(dxl_sha1 *s, uint8_t digest[20]) {
    uint64_t bits = s->length * 8;
    uint8_t pad = 0x80;
    dxl_sha1_update(s, &pad, 1);
    uint8_t zero = 0;
    while (s->used != 56) dxl_sha1_update(s, &zero, 1);
    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - i * 8));
    dxl_sha1_update(s, len, 8);
    for (int i = 0; i < 5; i++) {
        digest[i * 4] = (uint8_t)(s->h[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)s->h[i];
    }
}

void dxl_sha1_hex(const uint8_t digest[20], char hex[41]) {
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 20; i++) {
        hex[i * 2] = digits[digest[i] >> 4];
        hex[i * 2 + 1] = digits[digest[i] & 15];
    }
    hex[40] = '\0';
}

int dxl_sha1_file(const char *path, char hex[41]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    dxl_sha1 s;
    dxl_sha1_init(&s);
    uint8_t chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_sha1_update(&s, chunk, n);
    int bad = ferror(f);
    fclose(f);
    if (bad) return -1;
    uint8_t d[20];
    dxl_sha1_final(&s, d);
    dxl_sha1_hex(d, hex);
    return 0;
}
