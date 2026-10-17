/* The package reader on packages the test writes itself -- a name table,
 * imports and exports, tagged properties of each kind it reads, a P8
 * texture and its palette, a sound, a class's defaults, a save's info --
 * then on the same bytes broken a thousand ways, which it must refuse or
 * read without going past them. */
#include "test.h"
#include "scratch.h"
#include "core/theme_colors.h"
#include "core/upkg_assets.h"

#include <stdint.h>

/* ---- a writer ---------------------------------------------------------- */

typedef struct {
    uint8_t *d;
    size_t   n, cap;
} buf;

static void put(buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2 + 64;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static void u8(buf *b, uint8_t v) { put(b, &v, 1); }
static void u16(buf *b, uint16_t v) { u8(b, (uint8_t)v); u8(b, (uint8_t)(v >> 8)); }
static void u32(buf *b, uint32_t v) { u16(b, (uint16_t)v); u16(b, (uint16_t)(v >> 16)); }
static void u64(buf *b, uint64_t v) { u32(b, (uint32_t)v); u32(b, (uint32_t)(v >> 32)); }

static void idx(buf *b, int32_t v) {
    uint32_t a = (uint32_t)(v < 0 ? -v : v);
    uint8_t first = (uint8_t)((v < 0 ? 0x80 : 0) | (a & 0x3f) | (a >= 0x40 ? 0x40 : 0));
    u8(b, first);
    a >>= 6;
    while (a) {
        u8(b, (uint8_t)((a & 0x7f) | (a >= 0x80 ? 0x80 : 0)));
        a >>= 7;
    }
}

static void str(buf *b, const char *s) {
    idx(b, (int32_t)strlen(s) + 1);
    put(b, s, strlen(s) + 1);
}

/* The names the test's package has, by index. */
enum { N_NONE, N_CORE, N_ENGINE, N_TEXTURE, N_PALETTE, N_SOUND, N_CLASS, N_PAL, N_TEX, N_SND, N_WAV, N_FORMAT,
       N_PALETTE_PROP, N_COUNT_PROP, N_TITLE, N_COLOR, N_COLORS, N_THEME, N_THEMENAME, N_BASE, N_MASKED,
       N_INFO_CLASS, N_INFO, N_YEAR, N_DESC, N_SNAP, N_NAMES };
static const char *const names[N_NAMES] = {
    "None", "Core", "Engine", "Texture", "Palette", "Sound", "Class", "MyPal", "MyTex", "MySound", "WAV", "Format",
    "Palette", "Count", "Title", "Color", "Colors", "ColorThemeMenu_Test", "themeName", "ColorThemeMenu", "bMasked",
    "DeusExSaveInfo", "MyInfo", "Year", "Description", "Snapshot",
};

/* A property's tag, in the format's order: its name, its type and size
 * code, a struct's name, a size of a byte (code 5), an array index. */
static void tag(buf *b, int name, int type, int size_code, int index, int struct_name, int size) {
    idx(b, name);
    u8(b, (uint8_t)(type | size_code << 4 | (index ? 0x80 : 0)));
    if (type == DXL_UPROP_STRUCT) idx(b, struct_name);
    if (size_code == 5) u8(b, (uint8_t)size);
    if (index) u8(b, (uint8_t)index);
}

/* A class's headers with no script, its defaults after. */
static void class_head(buf *b) {
    idx(b, 0); idx(b, 0);                        /* UField */
    idx(b, 0); idx(b, 0); idx(b, 0); u32(b, 0); u32(b, 0); u32(b, 0);   /* UStruct, no script */
    u64(b, 0); u64(b, 0); u16(b, 0); u32(b, 0);  /* UState */
    u32(b, 0); put(b, "0123456789abcdef", 16);   /* UClass: flags, GUID */
    idx(b, 1); idx(b, 0); u32(b, 0); u32(b, 0);  /* one dependency */
    idx(b, 0);                                   /* no package imports */
    idx(b, 0); idx(b, N_NONE);                   /* within, config */
}

/* The package: Core.Texture/Palette/Sound/Class imported, and seven
 * exports -- a palette, a 4x2 P8 masked texture, a sound, a theme class
 * over a base class, a save's info. */
static buf make_package(void) {
    buf obj[7];
    memset(obj, 0, sizeof obj);
    /* 1: MyPal */
    idx(&obj[0], N_NONE);
    idx(&obj[0], 3);
    put(&obj[0], "\x10\x20\x30\xff\x40\x50\x60\xff\x70\x80\x90\xff", 12);
    /* 2: MyTex, its palette export 1, masked */
    tag(&obj[1], N_PALETTE_PROP, DXL_UPROP_OBJECT, 0, 0, 0, 0);
    idx(&obj[1], 1);
    tag(&obj[1], N_MASKED, DXL_UPROP_BOOL, 0, 0, 0, 0);
    obj[1].d[obj[1].n - 1] |= 0x80;   /* a bool's value: its tag's high bit */
    idx(&obj[1], N_NONE);
    idx(&obj[1], 1);                       /* one mipmap */
    u32(&obj[1], 0);                       /* the offset past it */
    idx(&obj[1], 8);
    put(&obj[1], "\x00\x01\x02\x01\x02\x00\x01\x02", 8);
    u32(&obj[1], 4); u32(&obj[1], 2); u8(&obj[1], 2); u8(&obj[1], 1);
    /* 3: MySound, a WAV of five bytes */
    idx(&obj[2], N_NONE);
    idx(&obj[2], N_WAV);
    u32(&obj[2], 0);
    idx(&obj[2], 5);
    put(&obj[2], "RIFF!", 5);
    /* 4: ColorThemeMenu, Colors[0] and [1] */
    class_head(&obj[3]);
    tag(&obj[3], N_COLORS, DXL_UPROP_STRUCT, 2, 0, N_COLOR, 0);
    put(&obj[3], "\x01\x02\x03\x04", 4);
    tag(&obj[3], N_COLORS, DXL_UPROP_STRUCT, 2, 1, N_COLOR, 0);
    put(&obj[3], "\x05\x06\x07\x08", 4);
    idx(&obj[3], N_NONE);
    /* 5: ColorThemeMenu_Test, its name and Colors[1] its own */
    class_head(&obj[4]);
    tag(&obj[4], N_THEMENAME, DXL_UPROP_STR, 5, 0, 0, 7);
    str(&obj[4], "Ma Gr");
    tag(&obj[4], N_COLORS, DXL_UPROP_STRUCT, 2, 1, N_COLOR, 0);
    put(&obj[4], "\x09\x0a\x0b\x0c", 4);
    idx(&obj[4], N_NONE);
    /* 6: MyInfo, a save's: its year, description, snapshot export 2 */
    tag(&obj[5], N_YEAR, DXL_UPROP_INT, 2, 0, 0, 0);
    u32(&obj[5], 2026);
    tag(&obj[5], N_DESC, DXL_UPROP_STR, 5, 0, 0, 8);
    str(&obj[5], "Unatco");
    tag(&obj[5], N_SNAP, DXL_UPROP_OBJECT, 0, 0, 0, 0);
    idx(&obj[5], 2);
    idx(&obj[5], N_NONE);
    /* 7: nothing, no bytes */

    buf h = { 0 };
    u32(&h, DXL_UPKG_TAG);
    u16(&h, 68); u16(&h, 0); u32(&h, 0);
    size_t counts = h.n;
    for (int i = 0; i < 6; i++) u32(&h, 0);   /* filled below */
    /* Names. */
    size_t noff = h.n;
    for (int i = 0; i < N_NAMES; i++) {
        str(&h, names[i]);
        u32(&h, 0);
    }
    /* Imports: Core's classes, and the save's info class. */
    size_t ioff = h.n;
    int imp_names[5] = { N_TEXTURE, N_PALETTE, N_SOUND, N_CLASS, N_INFO_CLASS };
    for (int i = 0; i < 5; i++) {
        idx(&h, N_CORE); idx(&h, N_CLASS); u32(&h, 0); idx(&h, imp_names[i]);
    }
    /* The objects' bytes, then the exports' table naming them. */
    size_t off[7];
    for (int i = 0; i < 7; i++) {
        off[i] = h.n;
        if (obj[i].n) put(&h, obj[i].d, obj[i].n);
    }
    size_t eoff = h.n;
    int cls[7] = { -2, -1, -3, 0, 0, -5, -1 };
    int nm[7] = { N_PAL, N_TEX, N_SND, N_BASE, N_THEME, N_INFO, N_NONE };
    int sup[7] = { 0, 0, 0, 0, 4, 0, 0 };
    for (int i = 0; i < 7; i++) {
        idx(&h, cls[i]); idx(&h, sup[i]); u32(&h, 0); idx(&h, nm[i]);
        u32(&h, i == 5 ? 0x00070004u : 0x00070000u);
        idx(&h, (int32_t)obj[i].n);
        if (obj[i].n) idx(&h, (int32_t)off[i]);
        free(obj[i].d);
    }
    uint32_t v[6] = { N_NAMES, (uint32_t)noff, 7, (uint32_t)eoff, 5, (uint32_t)ioff };
    for (int i = 0; i < 6; i++) memcpy(h.d + counts + 4 * (size_t)i, &v[i], 4);
    return h;
}

/* ---- the tests ---------------------------------------------------------- */

static void test_tables(void) {
    buf b = make_package();
    dxl_upkg p;
    dxl_err e;
    CHECK_INT(dxl_upkg_parse(&p, b.d, b.n, &e), 0);
    CHECK_INT(p.version, 68);
    CHECK_INT(p.name_count, N_NAMES);
    CHECK_INT(p.import_count, 5);
    CHECK_INT(p.export_count, 7);
    CHECK_STR(dxl_upkg_name(&p, N_THEME), "ColorThemeMenu_Test");
    CHECK_STR(dxl_upkg_name(&p, 999), "");
    CHECK_STR(dxl_upkg_ref_class(&p, 2), "Texture");
    CHECK_STR(dxl_upkg_ref_class(&p, 4), "Class");
    CHECK_STR(dxl_upkg_ref_name(&p, -3), "Sound");
    CHECK_INT(dxl_upkg_find(&p, "texture", "mytex"), 1);
    CHECK_INT(dxl_upkg_find(&p, "Sound", "MyTex"), -1);
    CHECK_INT(dxl_upkg_find(&p, NULL, "MySound"), 2);
    dxl_upkg_free(&p);
    /* Not a package; one cut short. */
    CHECK_INT(dxl_upkg_parse(&p, "not a package", 13, &e), -1);
    CHECK_INT(dxl_upkg_parse(&p, b.d, 40, &e), -1);
    free(b.d);
}

static void test_assets(void) {
    buf b = make_package();
    dxl_upkg p;
    dxl_err e;
    CHECK_INT(dxl_upkg_parse(&p, b.d, b.n, &e), 0);
    dxl_uimage im;
    CHECK_INT(dxl_upkg_texture(&p, 1, &im, &e), 0);
    CHECK_INT(im.w, 4);
    CHECK_INT(im.h, 2);
    CHECK_INT(im.masked, 1);
    /* Its first pixel colour 0: clear; its second the palette's second. */
    CHECK_INT(im.rgba[3], 0);
    CHECK_INT(im.rgba[4], 0x40);
    CHECK_INT(im.rgba[5], 0x50);
    CHECK_INT(im.rgba[7], 255);
    dxl_uimage_free(&im);
    CHECK_INT(dxl_upkg_texture(&p, 2, &im, &e), -1);
    size_t len;
    char type[16];
    uint8_t *f = dxl_upkg_file(&p, 2, &len, type, sizeof type, &e);
    CHECK(f && len == 5 && !memcmp(f, "RIFF!", 5));
    CHECK_STR(type, "WAV");
    free(f);
    CHECK(dxl_upkg_file(&p, 6, &len, type, sizeof type, &e) == NULL);
    dxl_upkg_free(&p);

    /* The theme: its own Colors[1], the base's [0], black after. */
    const char *dir = scratch_dir("upkg");
    char *path = scratch_path(dir, "DeusEx.u");
    FILE *out = fopen(path, "wb");
    fwrite(b.d, 1, b.n, out);
    fclose(out);
    dxl_menu_theme t[4];
    CHECK_INT(dxl_menu_themes_read(path, t, 4, &e), 1);
    CHECK_STR(t[0].name, "Ma Gr");
    CHECK_STR(t[0].class_name, "ColorThemeMenu_Test");
    CHECK(!memcmp(t[0].colors[0], "\x01\x02\x03\x04", 4));
    CHECK(!memcmp(t[0].colors[1], "\x09\x0a\x0b\x0c", 4));
    CHECK(!memcmp(t[0].colors[2], "\0\0\0\0", 4));
    CHECK(dxl_menu_theme_find(t, 1, "ma gr") == &t[0]);
    CHECK(dxl_menu_theme_find(t, 1, "Default") == NULL);
    /* The save's info, its snapshot the texture. */
    dxl_saveinfo s;
    CHECK_INT(dxl_saveinfo_read(path, 1, &s, &e), 0);
    CHECK_INT(s.year, 2026);
    CHECK_STR(s.description, "Unatco");
    CHECK_INT(s.snapshot.w, 4);
    dxl_saveinfo_free(&s);
    free(path);
    scratch_remove(dir);
    free(b.d);
}

/* Broken a thousand ways -- bytes changed, cut short -- every reader
 * refuses it or reads within it (ASan's builds see to the second). */
static void test_broken(void) {
    buf b = make_package();
    uint32_t seed = 12345;
    int parsed = 0;
    for (int round = 0; round < 1000; round++) {
        uint8_t *d = malloc(b.n);
        memcpy(d, b.d, b.n);
        size_t n = b.n;
        int flips = 1 + round % 8;
        for (int k = 0; k < flips; k++) {
            seed = seed * 1103515245u + 12345u;
            size_t at = (seed >> 8) % n;
            seed = seed * 1103515245u + 12345u;
            d[at] = (uint8_t)(seed >> 16);
        }
        if (round % 10 == 9) n = (seed >> 4) % n;
        dxl_upkg p;
        dxl_err e;
        if (dxl_upkg_parse(&p, d, n, &e) == 0) {
            parsed++;
            for (int32_t i = 0; i < p.export_count; i++) {
                dxl_uimage im;
                if (dxl_upkg_texture(&p, i, &im, &e) == 0) dxl_uimage_free(&im);
                size_t len;
                char type[16];
                free(dxl_upkg_file(&p, i, &len, type, sizeof type, &e));
                dxl_ureader r;
                if (dxl_upkg_class_defaults(&p, i, &r) == 0) {
                    dxl_uprop pr;
                    for (int guard = 0; guard < 1000 && dxl_ur_prop(&r, &p, &pr) == 1; guard++) {
                        char s[64];
                        dxl_uprop_str(&pr, s, sizeof s);
                        (void)dxl_uprop_int(&pr);
                    }
                }
                (void)dxl_upkg_ref_class(&p, i + 1);
            }
            dxl_upkg_free(&p);
        }
        free(d);
    }
    CHECK(parsed > 0);
    free(b.d);
}

TEST_MAIN_BEGIN
    RUN(test_tables);
    RUN(test_assets);
    RUN(test_broken);
TEST_MAIN_END
