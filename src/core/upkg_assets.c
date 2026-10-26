#include "core/upkg_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A texture's formats, its Format property's values. */
#define TEXF_P8    0
#define TEXF_DXT1  3
#define TEXF_RGBA8 5

void dxl_uimage_free(dxl_uimage *im) {
    if (!im) return;
    free(im->rgba);
    memset(im, 0, sizeof *im);
}

/* Palette export e's colours into pal (256 of RGBA): 0 when it has them. */
static int palette(const dxl_upkg *p, int32_t e, uint8_t pal[256][4]) {
    dxl_ureader r;
    dxl_ureader_export(&r, p, e);
    dxl_ur_skip_stack(&r, p, e);
    dxl_uprop pr;
    int rc;
    while ((rc = dxl_ur_prop(&r, p, &pr)) == 1) {}
    if (rc != 0) return -1;
    int32_t n = dxl_ur_index(&r);
    if (n <= 0 || n > 256) return -1;
    const uint8_t *c = dxl_ur_bytes(&r, (size_t)n * 4);
    if (!c) return -1;
    memset(pal, 0, 256 * 4);
    memcpy(pal, c, (size_t)n * 4);
    return 0;
}

/* One DXT1 block of 4x4 into its place in rgba. */
static void dxt1_block(const uint8_t *b, uint8_t *rgba, int w, int h, int bx, int by) {
    uint16_t c0 = (uint16_t)(b[0] | b[1] << 8), c1 = (uint16_t)(b[2] | b[3] << 8);
    uint8_t col[4][4];
    for (int i = 0; i < 2; i++) {
        uint16_t c = i ? c1 : c0;
        col[i][0] = (uint8_t)(((c >> 11) & 31) * 255 / 31);
        col[i][1] = (uint8_t)(((c >> 5) & 63) * 255 / 63);
        col[i][2] = (uint8_t)((c & 31) * 255 / 31);
        col[i][3] = 255;
    }
    for (int k = 0; k < 3; k++) {
        if (c0 > c1) {
            col[2][k] = (uint8_t)((2 * col[0][k] + col[1][k]) / 3);
            col[3][k] = (uint8_t)((col[0][k] + 2 * col[1][k]) / 3);
        } else {
            col[2][k] = (uint8_t)((col[0][k] + col[1][k]) / 2);
            col[3][k] = 0;
        }
    }
    col[2][3] = 255;
    col[3][3] = c0 > c1 ? 255 : 0;
    uint32_t bits = (uint32_t)b[4] | (uint32_t)b[5] << 8 | (uint32_t)b[6] << 16 | (uint32_t)b[7] << 24;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            int px = bx * 4 + x, py = by * 4 + y;
            if (px >= w || py >= h) continue;
            memcpy(rgba + ((size_t)py * (size_t)w + (size_t)px) * 4, col[(bits >> (2 * (y * 4 + x))) & 3], 4);
        }
}

int dxl_upkg_texture(const dxl_upkg *p, int32_t e, dxl_uimage *out, dxl_err *err) {
    memset(out, 0, sizeof *out);
    dxl_ureader r;
    dxl_ureader_export(&r, p, e);
    dxl_ur_skip_stack(&r, p, e);
    int format = TEXF_P8, masked = 0;
    int32_t pal_ref = 0;
    dxl_uprop pr;
    int rc;
    while ((rc = dxl_ur_prop(&r, p, &pr)) == 1) {
        if (!dxl_stricmp(pr.name, "Format")) format = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(pr.name, "Palette")) pal_ref = dxl_uprop_index(&pr);
        else if (!dxl_stricmp(pr.name, "bMasked")) masked = pr.bool_value;
    }
    if (rc != 0) {
        dxl_err_set(err, "texture %s: its properties do not hold together", dxl_upkg_ref_name(p, e + 1));
        return -1;
    }
    /* The first of its mipmaps: after 61, the offset past its data
     * first; its bytes; its size, and its sizes as powers of two. */
    int32_t mips = dxl_ur_index(&r);
    if (mips <= 0) {
        dxl_err_set(err, "texture %s has no mipmaps", dxl_upkg_ref_name(p, e + 1));
        return -1;
    }
    if (p->version > 61) dxl_ur_u32(&r);
    int32_t n = dxl_ur_index(&r);
    const uint8_t *data = n > 0 ? dxl_ur_bytes(&r, (size_t)n) : NULL;
    int32_t w = dxl_ur_i32(&r), h = dxl_ur_i32(&r);
    if (r.bad || !data || w <= 0 || h <= 0 || w > 8192 || h > 8192) {
        dxl_err_set(err, "texture %s: its first mipmap does not hold together", dxl_upkg_ref_name(p, e + 1));
        return -1;
    }
    /* Its bytes all there before its pixels are made: a size it claims is
     * no size to allocate. */
    size_t px = (size_t)w * (size_t)h, need;
    if (format == TEXF_P8) need = px;
    else if (format == TEXF_RGBA8) need = px * 4;
    else if (format == TEXF_DXT1) need = (size_t)((w + 3) / 4) * (size_t)((h + 3) / 4) * 8;
    else {
        dxl_err_set(err, "texture %s is of a format the launcher does not read (%d)", dxl_upkg_ref_name(p, e + 1), format);
        return -1;
    }
    if ((size_t)n < need) {
        dxl_err_set(err, "texture %s: fewer bytes than its size", dxl_upkg_ref_name(p, e + 1));
        return -1;
    }
    out->w = w;
    out->h = h;
    out->masked = masked;
    out->rgba = dxl_xmalloc(px * 4);
    if (format == TEXF_P8) {
        uint8_t pal[256][4];
        int has = pal_ref > 0 && palette(p, pal_ref - 1, pal) == 0;
        for (size_t i = 0; i < px; i++) {
            uint8_t c = data[i];
            uint8_t *o = out->rgba + i * 4;
            if (has) {
                o[0] = pal[c][0];
                o[1] = pal[c][1];
                o[2] = pal[c][2];
            } else {
                o[0] = o[1] = o[2] = c;
            }
            o[3] = masked && c == 0 ? 0 : 255;
        }
    } else if (format == TEXF_RGBA8) {
        for (size_t i = 0; i < px; i++) {
            const uint8_t *s = data + i * 4;
            uint8_t *o = out->rgba + i * 4;
            o[0] = s[2];
            o[1] = s[1];
            o[2] = s[0];
            o[3] = s[3];
        }
    } else {
        int bw = (w + 3) / 4, bh = (h + 3) / 4;
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) dxt1_block(data + ((size_t)by * (size_t)bw + (size_t)bx) * 8, out->rgba, w, h, bx, by);
    }
    return 0;
}

uint8_t *dxl_upkg_file(const dxl_upkg *p, int32_t e, size_t *len, char *type, size_t type_size, dxl_err *err) {
    *len = 0;
    if (type_size) type[0] = '\0';
    dxl_ureader r;
    dxl_ureader_export(&r, p, e);
    dxl_ur_skip_stack(&r, p, e);
    dxl_uprop pr;
    int rc;
    while ((rc = dxl_ur_prop(&r, p, &pr)) == 1) {}
    /* Its file's type, a name; after 61 the offset past it; its bytes. */
    int32_t t = rc == 0 ? dxl_ur_index(&r) : -1;
    if (p->version > 61) dxl_ur_u32(&r);
    int32_t n = dxl_ur_index(&r);
    const uint8_t *data = n > 0 ? dxl_ur_bytes(&r, (size_t)n) : NULL;
    if (rc != 0 || r.bad || !data) {
        dxl_err_set(err, "%s: no file in it", dxl_upkg_ref_name(p, e + 1));
        return NULL;
    }
    if (type_size) snprintf(type, type_size, "%s", dxl_upkg_name(p, t));
    uint8_t *copy = dxl_xmalloc((size_t)n);
    memcpy(copy, data, (size_t)n);
    *len = (size_t)n;
    return copy;
}

int dxl_upkg_class_defaults(const dxl_upkg *p, int32_t e, dxl_ureader *r) {
    dxl_ureader_export(r, p, e);
    dxl_ur_skip_stack(r, p, e);
    /* UField: its super and next. UStruct: its text, its children, its
     * friendly name, line and text position, its script's size -- none
     * here, or its bytecode would follow. */
    dxl_ur_index(r);
    dxl_ur_index(r);
    dxl_ur_index(r);
    dxl_ur_index(r);
    dxl_ur_index(r);
    dxl_ur_u32(r);
    dxl_ur_u32(r);
    if (dxl_ur_u32(r) != 0) return -1;
    /* UState: the probe and ignore masks, the label table, its flags. */
    dxl_ur_u64(r);
    dxl_ur_u64(r);
    dxl_ur_u16(r);
    dxl_ur_u32(r);
    /* UClass: before 62 the old record size; its flags, its GUID, its
     * dependencies and package imports; from 62 its within and config. */
    if (p->version <= 61) dxl_ur_u32(r);
    dxl_ur_u32(r);
    dxl_ur_bytes(r, 16);
    int32_t deps = dxl_ur_index(r);
    for (int32_t i = 0; i < deps && !r->bad; i++) {
        dxl_ur_index(r);
        dxl_ur_u32(r);
        dxl_ur_u32(r);
    }
    int32_t imports = dxl_ur_index(r);
    for (int32_t i = 0; i < imports && !r->bad; i++) dxl_ur_index(r);
    if (p->version >= 62) {
        dxl_ur_index(r);
        dxl_ur_index(r);
    }
    return r->bad || deps < 0 || imports < 0 ? -1 : 0;
}

/* ---- a save's info ------------------------------------------------------ */

void dxl_saveinfo_free(dxl_saveinfo *s) {
    if (!s) return;
    dxl_uimage_free(&s->snapshot);
}

/* A Str for showing: UTF-8, from the original's Latin-1 or as DXEngine
 * wrote it. */
static void shown_str(const dxl_uprop *pr, char *out, size_t size) {
    char raw[512];
    dxl_uprop_str(pr, raw, sizeof raw);
    /* A text longer than the buffer was cut at a byte: cut at a character. */
    if (strlen(raw) == sizeof raw - 1) dxl_text_drop_cut(raw);
    dxl_text_utf8(raw, out, size);
}

int dxl_saveinfo_read(const char *path, int with_snapshot, dxl_saveinfo *out, dxl_err *err) {
    memset(out, 0, sizeof *out);
    dxl_upkg p;
    if (dxl_upkg_load(&p, path, err) != 0) return -1;
    int32_t e = -1;
    for (int32_t i = 0; i < p.export_count && e < 0; i++)
        if (!dxl_stricmp(dxl_upkg_ref_class(&p, i + 1), "DeusExSaveInfo")) e = i;
    if (e < 0) {
        dxl_err_set(err, "%s: no DeusExSaveInfo in it", path);
        dxl_upkg_free(&p);
        return -1;
    }
    dxl_ureader r;
    dxl_ureader_export(&r, &p, e);
    dxl_ur_skip_stack(&r, &p, e);
    dxl_uprop pr;
    int32_t snapshot = 0;
    int rc;
    while ((rc = dxl_ur_prop(&r, &p, &pr)) == 1) {
        const char *n = pr.name;
        if (!dxl_stricmp(n, "Year")) out->year = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Month")) out->month = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Day")) out->day = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Hour")) out->hour = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Minute")) out->minute = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Second")) out->second = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "DirectoryIndex")) out->directory_index = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Description")) shown_str(&pr, out->description, sizeof out->description);
        else if (!dxl_stricmp(n, "MissionLocation")) shown_str(&pr, out->location, sizeof out->location);
        else if (!dxl_stricmp(n, "MapName")) shown_str(&pr, out->map, sizeof out->map);
        else if (!dxl_stricmp(n, "saveTime")) out->play_seconds = dxl_uprop_int(&pr);
        else if (!dxl_stricmp(n, "Snapshot")) snapshot = dxl_uprop_index(&pr);
    }
    if (rc != 0) {
        dxl_err_set(err, "%s: its DeusExSaveInfo does not hold together", path);
        dxl_upkg_free(&p);
        return -1;
    }
    /* The snapshot, none when the renderer could not take one. */
    if (with_snapshot && snapshot > 0) {
        dxl_err e2;
        if (dxl_upkg_texture(&p, snapshot - 1, &out->snapshot, &e2) != 0) memset(&out->snapshot, 0, sizeof out->snapshot);
    }
    dxl_upkg_free(&p);
    return 0;
}
