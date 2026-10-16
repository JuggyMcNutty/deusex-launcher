#include "core/peicon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RT_ICON = 3, RT_GROUP_ICON = 14 };

typedef struct {
    const uint8_t *data;
    size_t         size;
    size_t         sections, nsections;  /* the section table's offset and length */
    size_t         rsrc;                 /* the resource directory's offset */
} pe_file;

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int has(const pe_file *f, size_t off, size_t n) { return off <= f->size && n <= f->size - off; }

/* A virtual address's place in the file, through the section holding it. */
static int rva_offset(const pe_file *f, uint32_t rva, size_t *off) {
    for (size_t i = 0; i < f->nsections; i++) {
        const uint8_t *s = f->data + f->sections + i * 40;
        uint32_t vsize = u32(s + 8), va = u32(s + 12), rawsize = u32(s + 16), rawptr = u32(s + 20);
        uint32_t span = vsize > rawsize ? vsize : rawsize;
        if (rva >= va && rva - va < span) {
            *off = (size_t)rawptr + (rva - va);
            return 1;
        }
    }
    return 0;
}

/* The entry of a resource directory with the numeric id, skipping the named
 * ones; the target is an offset from the resource directory, its top bit
 * set for a subdirectory. */
static int dir_entry(const pe_file *f, uint32_t dir, int id, uint32_t *target) {
    size_t at = f->rsrc + (dir & 0x7fffffff);
    if (!has(f, at, 16)) return 0;
    unsigned named = u16(f->data + at + 12), ids = u16(f->data + at + 14);
    for (unsigned i = 0; i < named + ids; i++) {
        size_t e = at + 16 + (size_t)i * 8;
        if (!has(f, e, 8)) return 0;
        uint32_t name = u32(f->data + e);
        if (!(name & 0x80000000) && id >= 0 && name == (uint32_t)id) {
            *target = u32(f->data + e + 4);
            return 1;
        }
        if (id < 0 && i == 0) {   /* the first, whatever it is: a language */
            *target = u32(f->data + e + 4);
            return 1;
        }
    }
    return 0;
}

/* A resource's data: type, id, its first language. */
static int resource(const pe_file *f, int type, int id, const uint8_t **data, size_t *len) {
    uint32_t t, n, lang;
    if (!dir_entry(f, 0, type, &t) || !(t & 0x80000000)) return 0;
    if (!dir_entry(f, t, id, &n) || !(n & 0x80000000)) return 0;
    if (!dir_entry(f, n, -1, &lang) || (lang & 0x80000000)) return 0;
    size_t leaf = f->rsrc + lang, off;
    if (!has(f, leaf, 16) || !rva_offset(f, u32(f->data + leaf), &off)) return 0;
    uint32_t size = u32(f->data + leaf + 4);
    if (!has(f, off, size)) return 0;
    *data = f->data + off;
    *len = size;
    return 1;
}

static int open_pe(pe_file *f, dxl_err *err) {
    if (!has(f, 0x40, 0) || f->data[0] != 'M' || f->data[1] != 'Z') { dxl_err_set(err, "not an executable"); return 0; }
    size_t pe = u32(f->data + 0x3c);
    if (!has(f, pe, 24) || memcmp(f->data + pe, "PE\0\0", 4) != 0) { dxl_err_set(err, "no PE header"); return 0; }
    size_t nsec = u16(f->data + pe + 6), optsz = u16(f->data + pe + 20), opt = pe + 24;
    if (!has(f, opt, optsz) || optsz < 2) { dxl_err_set(err, "no optional header"); return 0; }
    uint16_t magic = u16(f->data + opt);
    size_t dirs = opt + (magic == 0x20b ? 112 : 96);   /* PE32+ or PE32 */
    if (dirs + 3 * 8 > opt + optsz) { dxl_err_set(err, "no resource directory"); return 0; }
    uint32_t rsrc_rva = u32(f->data + dirs + 2 * 8);
    f->sections = opt + optsz;
    f->nsections = nsec;
    if (!has(f, f->sections, nsec * 40)) { dxl_err_set(err, "no section table"); return 0; }
    if (!rsrc_rva || !rva_offset(f, rsrc_rva, &f->rsrc) || !has(f, f->rsrc, 16)) {
        dxl_err_set(err, "no resources");
        return 0;
    }
    return 1;
}

/* An icon's DIB, as ARGB top row first. */
static int decode(const uint8_t *d, size_t len, int w, int h, dxl_icon *out, dxl_err *err) {
    if (len >= 8 && memcmp(d, "\x89PNG", 4) == 0) { dxl_err_set(err, "a PNG icon"); return 0; }
    if (len < 40 || u32(d) < 40) { dxl_err_set(err, "no bitmap header"); return 0; }
    uint32_t hdr = u32(d);
    int bpp = u16(d + 14);
    uint32_t compression = u32(d + 16), used = u32(d + 32);
    if (compression != 0 || !(bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32)) {
        dxl_err_set(err, "an icon of %d bits, compression %u", bpp, (unsigned)compression);
        return 0;
    }
    size_t colours = bpp <= 8 ? (used ? used : (size_t)1 << bpp) : 0;
    size_t table = hdr, pixels = table + colours * 4;
    size_t xor_stride = ((size_t)w * bpp + 31) / 32 * 4, and_stride = ((size_t)w + 31) / 32 * 4;
    size_t mask = pixels + xor_stride * h;
    if (mask + and_stride * h > len) { dxl_err_set(err, "an icon cut short"); return 0; }

    uint32_t *argb = dxl_xmalloc((size_t)w * h * sizeof *argb);
    int any_alpha = 0;
    for (int y = 0; y < h; y++) {
        const uint8_t *row = d + pixels + xor_stride * (h - 1 - y);
        const uint8_t *mrow = d + mask + and_stride * (h - 1 - y);
        for (int x = 0; x < w; x++) {
            uint32_t b, g, r, a = 255;
            if (bpp <= 8) {
                size_t bit = (size_t)x * bpp;
                unsigned index = (row[bit / 8] >> (8 - bpp - bit % 8)) & ((1u << bpp) - 1);
                const uint8_t *c = d + table + (index < colours ? index : 0) * 4;
                b = c[0]; g = c[1]; r = c[2];
            } else {
                const uint8_t *c = row + (size_t)x * (bpp / 8);
                b = c[0]; g = c[1]; r = c[2];
                if (bpp == 32) { a = c[3]; if (a) any_alpha = 1; }
            }
            if (bpp != 32 && (mrow[x / 8] >> (7 - x % 8)) & 1) a = 0;
            argb[(size_t)y * w + x] = a << 24 | r << 16 | g << 8 | b;
        }
    }
    /* A 32-bit icon without alpha leaves its pixels out by the mask. */
    if (bpp == 32 && !any_alpha) {
        for (int y = 0; y < h; y++) {
            const uint8_t *mrow = d + mask + and_stride * (h - 1 - y);
            for (int x = 0; x < w; x++) {
                uint32_t *p = &argb[(size_t)y * w + x];
                *p = (*p & 0xffffff) | ((mrow[x / 8] >> (7 - x % 8)) & 1 ? 0u : 0xff000000u);
            }
        }
    }
    out->w = w;
    out->h = h;
    out->argb = argb;
    return 1;
}

int dxl_pe_icon(const char *path, int group, int size, dxl_icon *out, dxl_err *err) {
    memset(out, 0, sizeof *out);
    FILE *fp = fopen(path, "rb");
    if (!fp) { dxl_err_set(err, "cannot open %s", path); return -1; }
    uint8_t *buf = NULL;
    size_t n = 0, cap = 0;
    for (;;) {
        if (n == cap) buf = dxl_xrealloc(buf, cap = cap ? cap * 2 : 65536);
        size_t got = fread(buf + n, 1, cap - n, fp);
        if (!got) break;
        n += got;
    }
    fclose(fp);

    pe_file f = { buf, n, 0, 0, 0 };
    const uint8_t *grp, *img;
    size_t grp_len, img_len;
    int ok = 0;
    if (!open_pe(&f, err)) goto done;
    if (!resource(&f, RT_GROUP_ICON, group, &grp, &grp_len) || grp_len < 6) {
        dxl_err_set(err, "no icon group %d in %s", group, path);
        goto done;
    }
    /* GRPICONDIR: 6 bytes, then 14 an entry -- width, height (0 is 256),
     * colours, reserved, planes, bits, bytes, the RT_ICON's id. */
    unsigned count = u16(grp + 4);
    int best = -1, best_score = -1;
    for (unsigned i = 0; i < count && 6 + (i + 1) * 14 <= grp_len; i++) {
        const uint8_t *e = grp + 6 + i * 14;
        int w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256, bits = u16(e + 6);
        int score = (w == size && h == size ? 1 << 20 : 0) + w * 64 + bits;
        if (w == h && score > best_score) { best = (int)i; best_score = score; }
    }
    if (best < 0) { dxl_err_set(err, "icon group %d is empty", group); goto done; }
    const uint8_t *e = grp + 6 + best * 14;
    int w = e[0] ? e[0] : 256, h = e[1] ? e[1] : 256;
    if (!resource(&f, RT_ICON, u16(e + 12), &img, &img_len)) {
        dxl_err_set(err, "no icon %d for group %d", u16(e + 12), group);
        goto done;
    }
    ok = decode(img, img_len, w, h, out, err);
done:
    free(buf);
    return ok ? 0 : -1;
}

void dxl_icon_free(dxl_icon *icon) {
    free(icon->argb);
    icon->argb = NULL;
    icon->w = icon->h = 0;
}
