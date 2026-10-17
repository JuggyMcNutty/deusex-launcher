#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

static dxl_rgba rgb(int r, int g, int b) {
    dxl_rgba c = { (uint8_t)r, (uint8_t)g, (uint8_t)b, 255 };
    return c;
}

dxl_rgba dxl_rgba_fade(dxl_rgba c, float a) {
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    c.a = (uint8_t)(c.a * a + 0.5f);
    return c;
}

dxl_rgba dxl_rgba_mix(dxl_rgba a, dxl_rgba b, float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    dxl_rgba c = {
        (uint8_t)(a.r + (b.r - a.r) * t + 0.5f), (uint8_t)(a.g + (b.g - a.g) * t + 0.5f),
        (uint8_t)(a.b + (b.b - a.b) * t + 0.5f), (uint8_t)(a.a + (b.a - a.a) * t + 0.5f),
    };
    return c;
}

/* A tint over a base, as the game's menu textures are tinted: each channel
 * multiplied. */
static dxl_rgba tint(dxl_rgba base, dxl_rgba by) {
    dxl_rgba c = { (uint8_t)(base.r * by.r / 255), (uint8_t)(base.g * by.g / 255),
                   (uint8_t)(base.b * by.b / 255), base.a };
    return c;
}

static dxl_rgba scale(dxl_rgba c, float k) {
    float r = c.r * k, g = c.g * k, b = c.b * k;
    dxl_rgba out = { (uint8_t)(r > 255 ? 255 : r), (uint8_t)(g > 255 ? 255 : g),
                     (uint8_t)(b > 255 ? 255 : b), c.a };
    return out;
}

void dxl_theme_from_menu(dxl_theme *t, const char *name, const dxl_rgba menu[DXL_MENU_COLORS]) {
    memset(t, 0, sizeof *t);
    snprintf(t->name, sizeof t->name, "%s", name ? name : "");
    memcpy(t->menu, menu, sizeof t->menu);
    for (int i = 0; i < DXL_MENU_COLORS; i++) t->menu[i].a = 255;

    /* The steel the game's menus are drawn in: a cool dark grey, tinted
     * by the theme as its textures are. */
    dxl_rgba steel_hi = rgb(78, 88, 101), steel_lo = rgb(28, 33, 41);
    dxl_rgba bgtint = menu[DXL_MENU_BACKGROUND];
    dxl_rgba face = menu[DXL_MENU_BUTTON_FACE];
    dxl_rgba title = menu[DXL_MENU_TITLE_BACKGROUND];

    t->backdrop_top = scale(tint(rgb(22, 27, 36), bgtint), 1.2f);
    t->backdrop_bottom = scale(tint(rgb(6, 8, 12), bgtint), 1.2f);
    t->grid = dxl_rgba_fade(scale(title, 0.55f), 0.10f);
    t->panel_top = dxl_rgba_fade(tint(steel_hi, bgtint), 0.92f);
    t->panel_bottom = dxl_rgba_fade(tint(steel_lo, bgtint), 0.94f);
    t->panel_light = dxl_rgba_fade(scale(tint(rgb(170, 182, 196), bgtint), 1.1f), 0.85f);
    t->panel_shadow = rgb(3, 4, 6);
    t->face_top = tint(rgb(92, 103, 117), face);
    t->face_bottom = tint(rgb(40, 46, 55), face);
    t->face_focus_top = dxl_rgba_mix(tint(rgb(110, 122, 138), face), scale(title, 0.9f), 0.35f);
    t->face_focus_bottom = dxl_rgba_mix(tint(rgb(46, 53, 64), face), scale(title, 0.5f), 0.35f);
    t->title_bar = scale(title, 0.55f);
    t->title_text = menu[DXL_MENU_TITLE_TEXT];
    t->text = menu[DXL_MENU_BUTTON_TEXT_NORMAL];
    t->text_focus = menu[DXL_MENU_BUTTON_TEXT_FOCUS];
    t->text_disabled = menu[DXL_MENU_BUTTON_TEXT_DISABLED];
    t->help = menu[DXL_MENU_HELP_TEXT];
    t->accent = dxl_rgba_mix(title, rgb(255, 255, 255), 0.35f);
    t->bracket = menu[DXL_MENU_CURSOR];
    t->row_highlight = dxl_rgba_fade(menu[DXL_MENU_LIST_HIGHLIGHT], 0.55f);
    t->row_focus = dxl_rgba_fade(menu[DXL_MENU_LIST_FOCUS], 0.35f);
    t->good = rgb(120, 214, 140);
    t->warn = rgb(240, 196, 92);
    t->bad = rgb(236, 104, 96);
}

void dxl_theme_default(dxl_theme *t) {
    /* ColorThemeMenu_Default's Colors[0..12], read from DeusEx.u. */
    static const dxl_rgba menu[DXL_MENU_COLORS] = {
        { 200, 200, 200, 255 }, { 100, 100, 255, 255 }, { 250, 250, 250, 255 },
        { 255, 255, 255, 255 }, { 200, 200, 200, 255 }, { 255, 255, 255, 255 },
        { 100, 100, 100, 255 }, { 175, 175, 175, 255 }, { 175, 175, 175, 255 },
        { 255, 255, 255, 255 }, { 100, 100, 100, 255 }, { 150, 150, 150, 255 },
        { 255, 255, 255, 255 },
    };
    dxl_theme_from_menu(t, "Default", menu);
}
