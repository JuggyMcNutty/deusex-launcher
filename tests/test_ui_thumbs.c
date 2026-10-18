/* Pictures read off the window's thread (ui/thumbs.h): a shot of the
 * game's size, a BMP and a PNG, made to fit a gallery's box and a whole
 * one's; a file that does not read; the set cleared and asked again; the
 * textures made again for another renderer. */
#include "test.h"
#include "scratch.h"
#include "ui/thumbs.h"

/* A w x h picture of bands, as path's kind (.bmp or .png). */
static void picture(const char *path, int w, int h) {
    SDL_Surface *s = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_XRGB8888);
    const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(s->format);
    for (int y = 0; y < h; y += 16) {
        SDL_Rect band = { 0, y, w, 16 };
        SDL_FillSurfaceRect(s, &band, SDL_MapRGB(f, NULL, (Uint8)(y * 255 / h), 80, 160));
    }
    size_t n = strlen(path);
    if (!strcmp(path + n - 4, ".png")) SDL_SavePNG(s, path);
    else SDL_SaveBMP(s, path);
    SDL_DestroySurface(s);
}

static double now(void) { return (double)SDL_GetTicksNS() / 1e9; }

static void test_fit(void) {
    const char *dir = scratch_dir("thumbs");
    char *bmp = scratch_path(dir, "Shot0001.bmp"), *png = scratch_path(dir, "Shot0002.png"),
         *bad = scratch_path(dir, "Shot0003.bmp");
    picture(bmp, 1920, 1080);
    picture(png, 1280, 1024);
    scratch_write(dir, "Shot0003.bmp", "not a picture");

    SDL_Surface *target = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer *r = SDL_CreateSoftwareRenderer(target);
    dxl_thumbs *small = dxl_thumbs_new(384, 216), *whole = dxl_thumbs_new(2560, 1600);
    CHECK(small && whole && r);
    double t0 = now();
    dxl_thumbs_want(small, bmp);
    dxl_thumbs_want(small, png);
    dxl_thumbs_want(small, bad);
    dxl_thumbs_want(whole, bmp);
    dxl_thumbs_wait(small, r);
    dxl_thumbs_wait(whole, r);
    double took = now() - t0;
    if (took > 1) fprintf(stderr, "  slow: %.2f s for four pictures\n", took);
    CHECK(took < 5);
    int failed = -1, w = 0, h = 0;
    /* Fitted to the box, their shapes kept; never made larger. */
    CHECK(dxl_thumbs_get(small, bmp, &failed, &w, &h) != NULL);
    CHECK_INT(failed, 0);
    CHECK_INT(w, 384);
    CHECK_INT(h, 216);
    CHECK(dxl_thumbs_get(small, png, &failed, &w, &h) != NULL);
    CHECK_INT(w, 270);
    CHECK_INT(h, 216);
    CHECK(dxl_thumbs_get(whole, bmp, &failed, &w, &h) != NULL);
    CHECK_INT(w, 1920);
    CHECK_INT(h, 1080);
    CHECK(dxl_thumbs_get(small, bad, &failed, &w, &h) == NULL);
    CHECK_INT(failed, 1);
    /* None asked for: none there, nothing failed. */
    CHECK(dxl_thumbs_get(small, "/nowhere.png", &failed, NULL, NULL) == NULL);
    CHECK_INT(failed, 0);
    CHECK_INT(dxl_thumbs_update(small, r), 0);

    /* Cleared: gone, and read again when asked again. */
    dxl_thumbs_clear(small);
    CHECK(dxl_thumbs_get(small, bmp, NULL, NULL, NULL) == NULL);
    dxl_thumbs_want(small, bmp);
    dxl_thumbs_wait(small, r);
    CHECK(dxl_thumbs_get(small, bmp, NULL, &w, NULL) != NULL);

    /* Another renderer, the set not cleared: the first's textures let go,
     * the picture read again and made one of the second's. */
    SDL_Surface *target2 = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer *r2 = SDL_CreateSoftwareRenderer(target2);
    CHECK(dxl_thumbs_get(small, bmp, NULL, NULL, NULL) != NULL);
    CHECK_INT(dxl_thumbs_update(small, r2), 1);
    CHECK(dxl_thumbs_get(small, bmp, NULL, NULL, NULL) == NULL);
    dxl_thumbs_wait(small, r2);
    CHECK(dxl_thumbs_get(small, bmp, NULL, &w, &h) != NULL);
    CHECK_INT(w, 384);
    CHECK_INT(dxl_thumbs_busy(small), 0);

    /* The sets' textures go before their renderers do. */
    dxl_thumbs_free(whole);
    SDL_DestroyRenderer(r);
    dxl_thumbs_free(small);
    SDL_DestroyRenderer(r2);
    SDL_DestroySurface(target2);
    SDL_DestroySurface(target);
    free(bmp);
    free(png);
    free(bad);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_fit);
TEST_MAIN_END
