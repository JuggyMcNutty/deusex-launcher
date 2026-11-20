/* The icon file the Windows programs' resources take in
 * (ports/windows-x86_64/win32/DXLauncher.ico) is the drawing as it stands
 * (ui/icon.h), at each of core/ico.h's sizes: a changed drawing fails here
 * until dxl-icon makes the file again (tools/icon.c). Pixels compared, not
 * bytes: the PNGs' bytes are SDL's encoder's (miniz, inside SDL), which
 * another SDL may write otherwise for the same pixels; a level or two
 * either way is taken for the renderer's rounding. And the packing both
 * makers share (core/ico.h), checked byte by byte. */
#include "test.h"
#include "core/ico.h"
#include "ui/icon.h"

#include <SDL3/SDL.h>
#include <stdint.h>

#ifndef DXL_ICO_FILE
#define DXL_ICO_FILE "ports/windows-x86_64/win32/DXLauncher.ico"
#endif

#define TOLERANCE 2

static const int sizes[] = { DXL_ICO_SIZES };
#define COUNT ((int)(sizeof sizes / sizeof *sizes))

static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* The largest difference of a channel between the two, -1 when they are
 * not the same size. */
static int differs(SDL_Surface *a, SDL_Surface *b) {
    SDL_Surface *x = SDL_ConvertSurface(a, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface *y = SDL_ConvertSurface(b, SDL_PIXELFORMAT_RGBA32);
    int worst = -1;
    if (x && y && x->w == y->w && x->h == y->h) {
        worst = 0;
        for (int r = 0; r < x->h; r++) {
            const unsigned char *p = (const unsigned char *)x->pixels + (size_t)r * (size_t)x->pitch;
            const unsigned char *q = (const unsigned char *)y->pixels + (size_t)r * (size_t)y->pitch;
            for (int i = 0; i < x->w * 4; i++) {
                int d = abs((int)p[i] - (int)q[i]);
                if (d > worst) worst = d;
            }
        }
    }
    SDL_DestroySurface(x);
    SDL_DestroySurface(y);
    return worst;
}

static void test_committed_file(void) {
    size_t len = 0;
    char *data = slurp(DXL_ICO_FILE, &len);
    if (!data) fprintf(stderr, "  no %s: make it with dxl-icon\n", DXL_ICO_FILE);
    CHECK(data != NULL);
    if (!data) return;
    const unsigned char *u = (const unsigned char *)data;
    CHECK(len >= 6 + 16 * (size_t)COUNT);
    if (len < 6 + 16 * (size_t)COUNT) {
        free(data);
        return;
    }
    CHECK_INT(u[2], 1);
    CHECK_INT(u[4] | u[5] << 8, COUNT);
    int stale = 0;
    for (int i = 0; i < COUNT && i < (u[4] | u[5] << 8); i++) {
        const unsigned char *e = u + 6 + 16 * i;
        int px = e[0] ? e[0] : 256;
        CHECK_INT(px, sizes[i]);
        uint32_t size = get32(e + 8), at = get32(e + 12);
        CHECK((size_t)at + size <= len);
        if ((size_t)at + size > len) continue;
        SDL_Surface *got = SDL_LoadPNG_IO(SDL_IOFromConstMem(data + at, size), true);
        SDL_Surface *want = dxl_icon(sizes[i]);
        CHECK(got != NULL);
        CHECK(want != NULL);
        int d = got && want ? differs(got, want) : -1;
        if (d < 0 || d > TOLERANCE) {
            fprintf(stderr, "  the %d px image is not the drawing (by %d): make the file again with dxl-icon\n",
                    sizes[i], d);
            stale = 1;
        }
        SDL_DestroySurface(got);
        SDL_DestroySurface(want);
    }
    CHECK_INT(stale, 0);
    free(data);
}

static void test_pack(void) {
    dxl_ico_image im[2] = { { 16, "abc", 3 }, { 256, "defgh", 5 } };
    dxl_buf out;
    dxl_buf_init(&out);
    CHECK_INT(dxl_ico_pack(im, 2, &out), 0);
    CHECK_INT(out.len, 6 + 32 + 8);
    const unsigned char *u = (const unsigned char *)out.data;
    static const unsigned char head[6] = { 0, 0, 1, 0, 2, 0 };
    CHECK(out.len >= 46 && memcmp(u, head, 6) == 0);
    if (out.len >= 46) {
        /* 16 px: its size, 1 plane, 32 bits, 3 bytes at 38; 256 px as 0. */
        CHECK(u[6] == 16 && u[7] == 16 && u[10] == 1 && u[12] == 32);
        CHECK_INT(get32(u + 14), 3);
        CHECK_INT(get32(u + 18), 38);
        CHECK(u[22] == 0 && u[23] == 0);
        CHECK_INT(get32(u + 30), 5);
        CHECK_INT(get32(u + 34), 41);
        CHECK(memcmp(u + 38, "abcdefgh", 8) == 0);
    }
    /* What an icon file cannot hold: out left as it was. */
    dxl_ico_image big = { 257, "x", 1 };
    CHECK_INT(dxl_ico_pack(&big, 1, &out), -1);
    CHECK_INT(dxl_ico_pack(im, 0, &out), -1);
    CHECK_INT(out.len, 46);
    dxl_buf_free(&out);
}

TEST_MAIN_BEGIN
    RUN(test_pack);
    RUN(test_committed_file);
TEST_MAIN_END
