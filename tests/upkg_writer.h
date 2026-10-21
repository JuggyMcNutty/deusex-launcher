/* The tests' writer of UE1 packages' bytes (core/upkg.h's format): a
 * growing buffer, little-endian numbers, compact indices, strings and
 * property tags as the format keeps them. */
#ifndef DXL_TEST_UPKG_WRITER_H
#define DXL_TEST_UPKG_WRITER_H

#include "core/upkg.h"
#include "platform/fs.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

/* ---- a save's info ---------------------------------------------------- */

/* A save's SaveInfo.dxs as the game writes one: DeusExSaveInfo's fields
 * and, with snapshot, its picture -- a 256 x 128 P8 texture through a grey
 * palette, the game's 160 x 120 of it a skyline against the sky, its
 * buildings by the slot. */
typedef struct {
    int         date[6];     /* year, month, day, hour, minute, second */
    int         slot;
    const char *description, *location, *map;
    int         seconds;
    int         snapshot;
} dxl_test_save;

enum { DXL_TS_NONE, DXL_TS_CORE, DXL_TS_CLASS, DXL_TS_TEXTURE, DXL_TS_PALETTE, DXL_TS_INFO_CLASS, DXL_TS_INFO,
       DXL_TS_PAL, DXL_TS_TEX, DXL_TS_PALETTE_PROP, DXL_TS_YEAR, DXL_TS_MONTH, DXL_TS_DAY, DXL_TS_HOUR,
       DXL_TS_MINUTE, DXL_TS_SECOND, DXL_TS_INDEX, DXL_TS_DESC, DXL_TS_LOCATION, DXL_TS_MAP, DXL_TS_TIME,
       DXL_TS_SNAPSHOT, DXL_TS_NAMES };

static inline void dxl_ts_int(buf *b, int name, int value) {
    tag(b, name, DXL_UPROP_INT, 2, 0, 0, 0);
    u32(b, (uint32_t)value);
}

static inline void dxl_ts_str(buf *b, int name, const char *value) {
    tag(b, name, DXL_UPROP_STR, 5, 0, 0, (int)strlen(value) + 2);
    str(b, value);
}

/* The snapshot's index at x, y. */
static inline uint8_t dxl_ts_pixel(int x, int y, int slot) {
    if (x >= 160 || y >= 120) return 0;
    int ground = 92, top = 40 + ((x / 14 + slot) * 37) % 38;
    if (y >= ground) return (uint8_t)(70 - (y - ground));
    if (y >= top && (x % 14) < 11) return (uint8_t)(((x / 3 + y / 4) % 5) ? 30 : 150);
    return (uint8_t)(170 - y);
}

static inline buf dxl_test_saveinfo(const dxl_test_save *sv) {
    static const char *const names[DXL_TS_NAMES] = {
        "None", "Core", "Class", "Texture", "Palette", "DeusExSaveInfo", "MyDeusExSaveInfo", "SnapshotPalette",
        "Snapshot", "Palette", "Year", "Month", "Day", "Hour", "Minute", "Second", "DirectoryIndex", "Description",
        "MissionLocation", "MapName", "saveTime", "Snapshot",
    };
    buf obj[3];
    memset(obj, 0, sizeof obj);
    /* 1: the palette, grey */
    idx(&obj[0], DXL_TS_NONE);
    idx(&obj[0], 256);
    for (int i = 0; i < 256; i++) {
        uint8_t c[4] = { (uint8_t)i, (uint8_t)i, (uint8_t)i, 255 };
        put(&obj[0], c, 4);
    }
    /* 2: the snapshot through it */
    tag(&obj[1], DXL_TS_PALETTE_PROP, DXL_UPROP_OBJECT, 0, 0, 0, 0);
    idx(&obj[1], 1);
    idx(&obj[1], DXL_TS_NONE);
    idx(&obj[1], 1);
    u32(&obj[1], 0);
    idx(&obj[1], 256 * 128);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 256; x++) u8(&obj[1], dxl_ts_pixel(x, y, sv->slot));
    u32(&obj[1], 256); u32(&obj[1], 128); u8(&obj[1], 8); u8(&obj[1], 7);
    /* 3: the info */
    static const int fields[6] = { DXL_TS_YEAR, DXL_TS_MONTH, DXL_TS_DAY, DXL_TS_HOUR, DXL_TS_MINUTE, DXL_TS_SECOND };
    for (int i = 0; i < 6; i++) dxl_ts_int(&obj[2], fields[i], sv->date[i]);
    dxl_ts_int(&obj[2], DXL_TS_INDEX, sv->slot);
    dxl_ts_str(&obj[2], DXL_TS_DESC, sv->description);
    dxl_ts_str(&obj[2], DXL_TS_LOCATION, sv->location);
    dxl_ts_str(&obj[2], DXL_TS_MAP, sv->map ? sv->map : "01_NYC_UNATCOIsland");
    if (sv->snapshot) {
        tag(&obj[2], DXL_TS_SNAPSHOT, DXL_UPROP_OBJECT, 0, 0, 0, 0);
        idx(&obj[2], 2);
    }
    dxl_ts_int(&obj[2], DXL_TS_TIME, sv->seconds);
    idx(&obj[2], DXL_TS_NONE);

    buf h = { 0 };
    u32(&h, DXL_UPKG_TAG);
    u16(&h, 68); u16(&h, 0); u32(&h, 0);
    size_t counts = h.n;
    for (int i = 0; i < 6; i++) u32(&h, 0);
    size_t noff = h.n;
    for (int i = 0; i < DXL_TS_NAMES; i++) {
        str(&h, names[i]);
        u32(&h, 0);
    }
    size_t ioff = h.n;
    static const int imports[3] = { DXL_TS_TEXTURE, DXL_TS_PALETTE, DXL_TS_INFO_CLASS };
    for (int i = 0; i < 3; i++) {
        idx(&h, DXL_TS_CORE); idx(&h, DXL_TS_CLASS); u32(&h, 0); idx(&h, imports[i]);
    }
    size_t off[3];
    for (int i = 0; i < 3; i++) {
        off[i] = h.n;
        put(&h, obj[i].d, obj[i].n);
    }
    size_t eoff = h.n;
    static const int cls[3] = { -2, -1, -3 }, nm[3] = { DXL_TS_PAL, DXL_TS_TEX, DXL_TS_INFO };
    for (int i = 0; i < 3; i++) {
        idx(&h, cls[i]); idx(&h, 0); u32(&h, 0); idx(&h, nm[i]);
        u32(&h, 0x00070004u);
        idx(&h, (int32_t)obj[i].n);
        idx(&h, (int32_t)off[i]);
        free(obj[i].d);
    }
    uint32_t v[6] = { DXL_TS_NAMES, (uint32_t)noff, 3, (uint32_t)eoff, 3, (uint32_t)ioff };
    for (int i = 0; i < 6; i++) memcpy(h.d + counts + 4 * (size_t)i, &v[i], 4);
    return h;
}

/* save_dir/folder/SaveInfo.dxs written: 0, or -1. */
static inline int dxl_test_write_save(const char *save_dir, const char *folder, const dxl_test_save *sv) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", save_dir, folder);
    dxl_fs_mkdir(path);
    snprintf(path, sizeof path, "%s/%s/SaveInfo.dxs", save_dir, folder);
    buf b = dxl_test_saveinfo(sv);
    FILE *f = fopen(path, "wb");
    int ok = f && fwrite(b.d, 1, b.n, f) == b.n;
    if (f && fclose(f) != 0) ok = 0;
    free(b.d);
    return ok ? 0 : -1;
}

#endif
