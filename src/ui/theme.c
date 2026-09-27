#include "theme.h"

#include "platform/target.h"

#include <stdio.h>
#include <sys/stat.h>

const dxl_palette DXL_PAL = {
    .bg         = {  10,  10,  12, 255 },
    .panel      = {  22,  23,  28, 255 },
    .panel_sel  = {  52,  42,  16, 255 },
    .text       = { 226, 222, 210, 255 },
    .text_dim   = { 132, 130, 122, 255 },
    .accent     = { 255, 186,  58, 255 },   /* the game's amber */
    .accent_dim = { 150, 110,  36, 255 },
    .ok         = { 120, 200, 120, 255 },
    .warn       = { 240, 190,  80, 255 },
    .bad        = { 226,  92,  80, 255 },
    .rule       = {  58,  56,  50, 255 },
};

void dxl_metrics_for(dxl_metrics *m, int height) {
    /* Reference is a 720p panel. Rounding to whole pixels keeps
     * text crisp on a small screen, where half-pixel baselines are visible. */
    double s = (double)height / 720.0;
    if (s < 0.5) s = 0.5;

    m->pad         = (int)(40 * s + 0.5);
    m->title_size  = (int)(34 * s + 0.5);
    m->body_size   = (int)(21 * s + 0.5);
    m->item_size   = (int)(25 * s + 0.5);
    m->footer_size = (int)(18 * s + 0.5);
    m->item_height = (int)(46 * s + 0.5);
    m->line_gap    = (int)(28 * s + 0.5);
}

static int is_font(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

const char *dxl_theme_find_font(void) {
    /* What desktop distros put where; the device profile's own come first. */
    static const char *generic[] = {
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        NULL
    };
    const char *env = SDL_getenv("DXL_FONT");
    if (env && *env) {
        struct stat st;
        if (stat(env, &st) == 0) return env;
    }
    const char *const *own = dxl_target_get()->fonts;
    for (int i = 0; own && own[i]; i++)
        if (is_font(own[i])) return own[i];
    for (int i = 0; generic[i]; i++)
        if (is_font(generic[i])) return generic[i];
    return NULL;
}
