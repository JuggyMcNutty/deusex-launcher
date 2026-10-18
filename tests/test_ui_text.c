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
    TTF_Quit();
TEST_MAIN_END
