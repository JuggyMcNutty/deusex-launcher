#include "test.h"
#include "ui_offscreen.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <math.h>

static int valid_utf8(const char *s) {
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        int n = *p < 0x80 ? 1 : (*p & 0xe0) == 0xc0 ? 2 : (*p & 0xf0) == 0xe0 ? 3 : (*p & 0xf8) == 0xf0 ? 4 : 0;
        if (!n) return 0;
        for (int i = 1; i < n; i++)
            if ((p[i] & 0xc0) != 0x80) return 0;
        p += n;
    }
    return 1;
}

static void test_sizes(dxl_ui *ui) {
    SDL_FPoint a = dxl_text_size(ui, DXL_FONT_BODY, "Play", 0);
    SDL_FPoint b = dxl_text_size(ui, DXL_FONT_BODY, "Play the game", 0);
    CHECK(a.x > 10 && b.x > a.x);
    CHECK(a.y > 10 && a.y < 40);
    CHECK(dxl_text_line(ui, DXL_FONT_TITLE) > dxl_text_line(ui, DXL_FONT_SMALL));
    /* Wrapped narrower than its line: taller. */
    const char *s = "It keeps its own copy of the game's settings, and your saves and screenshots, in your home folder.";
    SDL_FPoint one = dxl_text_size(ui, DXL_FONT_BODY, s, 0);
    SDL_FPoint wrapped = dxl_text_size(ui, DXL_FONT_BODY, s, one.x / 2.5f);
    CHECK(wrapped.y >= one.y * 2.5f);
    CHECK(wrapped.x <= one.x / 2.5f + 1);
}

static void test_fit(dxl_ui *ui) {
    char out[256];
    CHECK_INT(dxl_text_fit(ui, DXL_FONT_BODY, "Play", 200, out, sizeof out), 0);
    CHECK_STR(out, "Play");
    const char *path = "/home/someone/.local/share/Steam/steamapps/common/Deus Ex";
    float full = dxl_text_size(ui, DXL_FONT_BODY, path, 0).x;
    CHECK_INT(dxl_text_fit(ui, DXL_FONT_BODY, path, full / 2, out, sizeof out), 1);
    CHECK(strncmp(out, "/home/", 6) == 0);
    CHECK(strcmp(out + strlen(out) - 3, "...") == 0);
    CHECK(dxl_text_size(ui, DXL_FONT_BODY, out, 0).x <= full / 2 + 0.5f);
    CHECK_INT(dxl_text_fit_tail(ui, DXL_FONT_BODY, path, full / 2, out, sizeof out), 1);
    CHECK(strncmp(out, "...", 3) == 0);
    CHECK(strcmp(out + strlen(out) - 7, "Deus Ex") == 0);
    CHECK(dxl_text_size(ui, DXL_FONT_BODY, out, 0).x <= full / 2 + 0.5f);
    CHECK_INT(dxl_text_fit_tail(ui, DXL_FONT_BODY, "Deus Ex", 500, out, sizeof out), 0);
    CHECK_STR(out, "Deus Ex");
    /* A string longer than the buffer, whose kept bytes would fit: cut
     * into the buffer, the head's start and the tail's end kept. */
    char small[8];
    CHECK_INT(dxl_text_fit(ui, DXL_FONT_BODY, "abcdefghijklmnopqrstuvwxyz0123456789ABCD", 5000, small, sizeof small), 1);
    CHECK_STR(small, "abcd...");
    CHECK_INT(dxl_text_fit_tail(ui, DXL_FONT_BODY, "abcdefghijklmnopqrstuvwxyz0123456789ABCD", 5000, small, sizeof small), 1);
    CHECK_STR(small, "...ABCD");
    /* Never inside a character. */
    const char *accents = "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9";
    float aw = dxl_text_size(ui, DXL_FONT_BODY, accents, 0).x;
    for (int i = 2; i < 9; i++) {
        dxl_text_fit(ui, DXL_FONT_BODY, accents, aw * i / 10, out, sizeof out);
        CHECK(valid_utf8(out));
        dxl_text_fit_tail(ui, DXL_FONT_BODY, accents, aw * i / 10, out, sizeof out);
        CHECK(valid_utf8(out));
    }
}

/* A label cut short is a diagnostic, unless it may be. */
static void test_label_diagnostic(dxl_ui *ui) {
    dxl_ui_begin(ui, 1);
    dxl_label(ui, (dxl_rect){ 0, 0, 30, 20 }, DXL_FONT_BODY, "A label far too long for its place", ui->theme.text,
              DXL_W_LEFT);
    CHECK_INT(ui->diag_count, 1);
    dxl_label(ui, (dxl_rect){ 0, 0, 30, 20 }, DXL_FONT_BODY, "A label far too long for its place", ui->theme.text,
              DXL_W_LEFT | DXL_W_MAY_CUT);
    CHECK_INT(ui->diag_count, 1);
    dxl_ui_end(ui);
    dxl_ui_begin(ui, 2);
    CHECK_INT(ui->diag_count, 0);
    dxl_ui_end(ui);
}

/* More texts drawn than the cache holds: the oldest go, nothing breaks. */
static void test_cache(dxl_ui *ui) {
    dxl_ui_begin(ui, 3);
    for (int i = 0; i < 1000; i++) {
        char s[32];
        snprintf(s, sizeof s, "line %d", i);
        SDL_FPoint p = dxl_text_draw(ui, DXL_FONT_SMALL, s, 0, (float)(i % 30) * 12, 0, ui->theme.text, 1);
        CHECK(p.x > 0);
    }
    CHECK(dxl_text_size(ui, DXL_FONT_SMALL, "line 5", 0).x > 0);
    dxl_ui_end(ui);
}

/* At twice the scale, the same text in dp; its pixels doubled. */
static void test_scale(void) {
    offscreen o1, o2;
    static dxl_ui a, b;
    if (offscreen_open(&o1, &a, 640, 360, 1) || offscreen_open(&o2, &b, 1280, 720, 2)) {
        CHECK(0);
        return;
    }
    SDL_FPoint sa = dxl_text_size(&a, DXL_FONT_BUTTON, "Make the profile", 0);
    SDL_FPoint sb = dxl_text_size(&b, DXL_FONT_BUTTON, "Make the profile", 0);
    CHECK(sb.x > sa.x * 0.9f && sb.x < sa.x * 1.1f);
    CHECK(a.w == 640 && b.w == 640);
    offscreen_close(&o2, &b);
    offscreen_close(&o1, &a);
}

/* The big screen: the layout's 1280 x 720 dp an eighth larger, the screen
 * 640 dp tall, never narrower than 960 dp; the kit on a television's
 * screen drawn so. */
static void test_big_scale(void) {
    CHECK(fabsf(dxl_ui_big_scale(1920, 1080) - 1.6875f) < 0.0001f);
    CHECK(fabsf(dxl_ui_big_scale(3840, 2160) - 3.375f) < 0.0001f);
    CHECK(fabsf(dxl_ui_big_scale(1280, 800) - 1.25f) < 0.0001f);
    CHECK(fabsf(dxl_ui_big_scale(1024, 768) - 1024 / 960.0f) < 0.0001f);   /* 4:3: its width the bound */
    offscreen o;
    static dxl_ui tv;
    if (offscreen_open(&o, &tv, 1920, 1080, 1)) {
        CHECK(0);
        return;
    }
    tv.big = 1;
    dxl_ui_input_begin(&tv);
    dxl_ui_begin(&tv, 1.0);
    CHECK(fabsf(tv.h - 640) < 0.01f);
    CHECK(tv.w > 1137 && tv.w < 1138);
    dxl_ui_end(&tv);
    offscreen_close(&o, &tv);
}

/* The window's smallest, 960 x 600 dp, held within the room its screen
 * leaves: 1366x768 at 125% with a 40 px taskbar and a 39 px title bar,
 * 1080p at 175%, the launcher at twice its size on 1080p -- the dp made
 * smaller; a room it fits, or none known, the scale as it was. The kit's
 * scale is held to it. */
static void test_fit_scale(void) {
    float k = dxl_ui_fit_scale(1.25f, 1366, 768 - 40 - 39, 960, 600);
    CHECK(fabsf(k - 689 / 600.0f) < 0.0001f);
    CHECK(600 * k <= 689 && 960 * k <= 1366);
    CHECK(fabsf(dxl_ui_fit_scale(1.75f, 1920, 1080 - 48 - 54, 960, 600) - 978 / 600.0f) < 0.0001f);
    CHECK(fabsf(dxl_ui_fit_scale(2, 1920, 1040, 960, 600) - 1040 / 600.0f) < 0.0001f);
    CHECK(fabsf(dxl_ui_fit_scale(1, 1920, 1040, 960, 600) - 1) < 0.0001f);
    CHECK(fabsf(dxl_ui_fit_scale(1.5f, 0, 0, 960, 600) - 1.5f) < 0.0001f);
    /* A narrow room: its width the bound. */
    CHECK(fabsf(dxl_ui_fit_scale(2, 1280, 1400, 960, 600) - 1280 / 960.0f) < 0.0001f);

    offscreen o;
    static dxl_ui kit;
    if (offscreen_open(&o, &kit, 1366, 689, 1.25f)) {
        CHECK(0);
        return;
    }
    kit.fit_scale = k;
    dxl_ui_input_begin(&kit);
    dxl_ui_begin(&kit, 1.0);
    CHECK(fabsf(kit.scale - k) < 0.0001f);
    CHECK(kit.h >= 600 - 0.01f);
    dxl_ui_end(&kit);
    /* Held only where it is under the scale. */
    kit.fit_scale = 2;
    dxl_ui_input_begin(&kit);
    dxl_ui_begin(&kit, 1.1);
    CHECK(fabsf(kit.scale - 1.25f) < 0.0001f);
    dxl_ui_end(&kit);
    offscreen_close(&o, &kit);
}

/* The typewriter: 220 characters a second from its start; whole once a key
 * comes after it began, and under reduced motion. */
static void test_typed(dxl_ui *ui) {
    static const char text[] = "A line the window types itself, as the game's menus do.";
    dxl_ui_input_begin(ui);
    dxl_ui_begin(ui, 100.0);
    float part = dxl_ui_typed(ui, 100.0 - 0.1, text), want = 0.1f * 220 / (float)strlen(text);
    CHECK(part > want - 0.01f && part < want + 0.01f);
    CHECK(ui->animating);
    CHECK(dxl_ui_typed(ui, 100.0, text) < 0.01f);
    CHECK(dxl_ui_typed(ui, 99.0, text) >= 1);
    dxl_ui_end(ui);
    ui->in.activate = 1;
    dxl_ui_begin(ui, 100.05);
    CHECK(dxl_ui_typed(ui, 100.0, text) >= 1);
    /* Begun as the key came: it types. */
    CHECK(dxl_ui_typed(ui, 100.05, text) < 0.01f);
    dxl_ui_end(ui);
    dxl_ui_input_begin(ui);
    ui->reduced_motion = 1;
    dxl_ui_begin(ui, 100.1);
    CHECK(dxl_ui_typed(ui, 100.1, text) >= 1);
    dxl_ui_end(ui);
    ui->reduced_motion = 0;
}

/* What s draws at 0,0 in white on black, its pixels kept: NULL when nothing
 * is drawn. */
static Uint32 *drawn(offscreen *o, dxl_ui *ui, dxl_font_role role, const char *s) {
    SDL_SetRenderDrawColor(o->renderer, 0, 0, 0, 255);
    SDL_RenderClear(o->renderer);
    dxl_text_draw(ui, role, s, 0, 0, 0, (dxl_rgba){ 255, 255, 255, 255 }, 1);
    SDL_FlushRenderer(o->renderer);
    size_t n = (size_t)o->surface->h * (size_t)o->surface->pitch;
    Uint32 *px = malloc(n);
    memcpy(px, o->surface->pixels, n);
    int lit = 0;
    for (size_t i = 0; i < n / 4 && !lit; i++) lit = (px[i] & 0xffffff) != 0;
    if (!lit) {
        free(px);
        return NULL;
    }
    return px;
}

/* Cyrillic and Greek, which the roles' fonts lack, drawn by the fallback in
 * every role: measured and drawn unlike as many of the box, which CJK
 * still draws as. */
static void test_fallback(void) {
    offscreen o;
    static dxl_ui ui;
    if (offscreen_open(&o, &ui, 640, 120, 1)) {
        CHECK(0);
        return;
    }
    static const char *const words[][2] = {
        { "\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd", "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad" },   /* Иван */
        { "\xce\xa3\xce\xaf\xcf\x83\xcf\x85\xcf\x86\xce\xbf\xcf\x82",                          /* Σίσυφος */
          "\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad\xe4\xb8\xad" },
    };
    for (int role = 0; role < DXL_FONT_COUNT; role++) {
        dxl_ui_begin(&ui, 1);
        for (int k = 0; k < 2; k++) {
            float w = dxl_text_size(&ui, (dxl_font_role)role, words[k][0], 0).x;
            float box = dxl_text_size(&ui, (dxl_font_role)role, words[k][1], 0).x;
            CHECK(w > 0 && box > 0);
            Uint32 *a = drawn(&o, &ui, (dxl_font_role)role, words[k][0]);
            Uint32 *b = drawn(&o, &ui, (dxl_font_role)role, words[k][1]);
            CHECK(a && b);
            int same = a && b && memcmp(a, b, (size_t)o.surface->h * (size_t)o.surface->pitch) == 0;
            if (same || fabsf(w - box) < 0.01f)
                fprintf(stderr, "  role %d: \"%s\" drawn as boxes (%.1f dp, the boxes %.1f)\n", role, words[k][0], w, box);
            CHECK(!same);
            free(a);
            free(b);
        }
        dxl_ui_end(&ui);
    }
    offscreen_close(&o, &ui);
}

/* A copy of the app without the fallback: the kit opens all the same, what
 * the roles' fonts lack drawn as their box. */
static void test_no_fallback(void) {
    static const char *const files[] = { "BarlowSemiCondensed-SemiBold.ttf", "BarlowSemiCondensed-Medium.ttf",
                                         "BarlowSemiCondensed-Regular.ttf", "ShareTechMono-Regular.ttf" };
    char dir[600], from[1200], to[1200];
    snprintf(dir, sizeof dir, "%s/dxl-fonts-%ld", dxl_test_tmp(), dxl_os_pid());
    dxl_fs_mkdir(dir);
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        snprintf(from, sizeof from, "%s/%s", DXL_FONTS, files[i]);
        snprintf(to, sizeof to, "%s/%s", dir, files[i]);
        size_t n = 0;
        void *data = SDL_LoadFile(from, &n);
        CHECK(data && SDL_SaveFile(to, data, n));
        SDL_free(data);
    }
    SDL_Surface *surface = SDL_CreateSurface(320, 60, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
    static dxl_ui ui;
    CHECK(renderer && dxl_ui_init(&ui, NULL, renderer, dir, 1) == 0);
    CHECK(ui.fonts[DXL_FONT_BODY] != NULL && ui.fallback[DXL_FONT_BODY] == NULL);
    dxl_ui_begin(&ui, 1);
    CHECK(dxl_text_size(&ui, DXL_FONT_BODY, "\xd0\x98\xd0\xb2\xd0\xb0\xd0\xbd", 0).x > 0);
    dxl_ui_end(&ui);
    dxl_ui_free(&ui);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    dxl_fs_remove_tree(dir);
}

TEST_MAIN_BEGIN
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init: %s\n", SDL_GetError());
        return 1;
    }
    static dxl_ui ui;
    offscreen o;
    if (offscreen_open(&o, &ui, 1280, 720, 1) != 0) return 1;
    dxl_test_current = "test_sizes";
    test_sizes(&ui);
    dxl_test_current = "test_fit";
    test_fit(&ui);
    dxl_test_current = "test_label_diagnostic";
    test_label_diagnostic(&ui);
    dxl_test_current = "test_cache";
    test_cache(&ui);
    dxl_test_current = "test_typed";
    test_typed(&ui);
    offscreen_close(&o, &ui);
    RUN(test_scale);
    RUN(test_big_scale);
    RUN(test_fit_scale);
    RUN(test_fallback);
    RUN(test_no_fallback);
    TTF_Quit();
TEST_MAIN_END
