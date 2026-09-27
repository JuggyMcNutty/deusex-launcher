/* Video: the renderer, and the Surreal Engine render options that do
 * something on this device.
 *
 * The original's Detail page offered sound quality, skin and world texture
 * detail and a 640x480 mode. Surreal Engine reads none of those -- its
 * renderer ignores TextureDetail/SkinDetail and its mixer ignores the Galaxy
 * quality keys -- so they are gone. What is here is what the engine's render
 * device and client actually consume, and each help line says what it does
 * in the renderer rather than repeating its name.
 */
#include "screens_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- renderer ---------------------------------------------------------- */

static void renderer_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    int cur = dxl_app_current_renderer(s->app);
    snprintf(out, n, "%s", cur >= 0 ? s->app->renderers.items[cur].label
                                    : dxl_es_choice(s->app->es, DXL_ES_RENDER_TYPE));
}

static void renderer_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    int cur = dxl_app_current_renderer(s->app);
    if (cur < 0) {
        snprintf(out, n, "Settings.json names \"%s\", which is not in renderers.ini. "
                 "Press A to choose one.", dxl_es_choice(s->app->es, DXL_ES_RENDER_TYPE));
        return;
    }
    const dxl_renderer *rd = &s->app->renderers.items[cur];
    snprintf(out, n, "%s. Press A to see every renderer and why the others can or "
             "cannot run here.", rd->status);
}

/* Left/right steps through the ones that can run; the rest are only in the
 * picker, where the reason is shown. */
static void renderer_step(dxl_session *s, const dxl_row *r, int dir) {
    const dxl_renderer_list *rl = &s->app->renderers;
    int n = (int)rl->count, cur = dxl_app_current_renderer(s->app), selectable = 0;
    for (int i = 0; i < n; i++) selectable += rl->items[i].selectable;
    if (selectable <= 1) {
        notice(s, 1, "%s is the only renderer that can run here. Press A to see why.",
               selectable ? rl->items[dxl_renderers_first_selectable(rl)].label : "Nothing");
        return;
    }
    for (int k = 1; k <= n; k++) {
        int i = ((cur < 0 ? 0 : cur) + dir * k + n * k) % n;
        if (rl->items[i].selectable) { dxl_app_choose_renderer(s->app, i); return; }
    }
}

static void renderer_activate(dxl_session *s, const dxl_row *r) { open_renderer_picker(s); }

static void renderer_reset(dxl_session *s, const dxl_row *r) {
    dxl_es_reset(s->app->es, DXL_ES_RENDER_TYPE);
}

/* ---- CPU mode ------------------------------------------------------------ */

/* The modes, their order and their help come from the device profile
 * (platform/target.h); a device that offers none has no row. */

static int cpu_visible(dxl_session *s, const dxl_row *r) {
    return dxl_target_cpu_mode_count(dxl_target_get()) > 0;
}

static void cpu_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s", dxl_app_cpu_mode(s->app));
}

static void cpu_step(dxl_session *s, const dxl_row *r, int dir) {
    const dxl_target *t = dxl_target_get();
    int count = dxl_target_cpu_mode_count(t), at = 0;
    for (int i = 0; i < count; i++)
        if (strcmp(t->cpu_modes[i].name, dxl_app_cpu_mode(s->app)) == 0) at = i;
    dxl_app_set_cpu_mode(s->app, t->cpu_modes[(at + dir + count) % count].name);
}

static void cpu_reset(dxl_session *s, const dxl_row *r) {
    dxl_app_set_cpu_mode(s->app, dxl_target_cpu_mode(dxl_target_get(), NULL)->name);
}

static void cpu_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const dxl_cpu_mode_info *m = dxl_target_cpu_mode(dxl_target_get(), dxl_app_cpu_mode(s->app));
    snprintf(out, n, "%s", m->help);
}

/* ---- ini-backed rows --------------------------------------------------- */

static void brightness_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%d%%", (int)(dxl_config_brightness(s->app->cfg) * 100 + 0.5));
}
static double brightness_slider(dxl_session *s, const dxl_row *r) {
    return dxl_config_brightness(s->app->cfg);
}
static void brightness_step(dxl_session *s, const dxl_row *r, int dir) {
    double v = dxl_config_brightness(s->app->cfg) + dir * 0.05;
    dxl_config_set_brightness(s->app->cfg, (double)(int)(v * 20 + 0.5) / 20);
}
static void brightness_reset(dxl_session *s, const dxl_row *r) {
    dxl_config_set_brightness(s->app->cfg, 0.5);
}
static void brightness_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    int d3d = strcmp(dxl_es_choice(s->app->es, DXL_ES_GAMMA), "D3D9") == 0;
    snprintf(out, n, "The setting the game's own Adjust Brightness screen changes. "
             "%s 50%% leaves the image as drawn.",
             d3d ? "With the D3D9 curve it sets the gamma power on each colour."
                 : "With the XOpenGL curve, above 50% it lifts the mid-tones; below, it "
                   "darkens evenly.");
}

static void decals_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s", dxl_config_decals(s->app->cfg) ? "On" : "Off");
}
static void decals_step(dxl_session *s, const dxl_row *r, int dir) {
    dxl_config_set_decals(s->app->cfg, !dxl_config_decals(s->app->cfg));
}
static void decals_reset(dxl_session *s, const dxl_row *r) {
    dxl_config_set_decals(s->app->cfg, 1);
}

/* ---- per-choice help --------------------------------------------------- */

static const char *const light_labels[] = { "Normal", "1x", "Brighter actors", NULL };
static void light_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const char *v = dxl_es_choice(s->app->es, DXL_ES_LIGHT);
    if (!strcmp(v, "OneX"))
        snprintf(out, n, "1x: lightmaps blended at 1x. Darker overall, close to the "
                 "software renderer's look.");
    else if (!strcmp(v, "BrighterActors"))
        snprintf(out, n, "Brighter actors: as Normal, but characters, weapons and items "
                 "are drawn 1.5x brighter, so they stand out in dark rooms.");
    else
        snprintf(out, n, "Normal: lightmaps blended at 2x, the way the game's Direct3D "
                 "renderer drew them.");
}

static const char *const gamma_labels[] = { "D3D9", "XOpenGL", NULL };
static void gamma_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    if (!strcmp(dxl_es_choice(s->app->es, DXL_ES_GAMMA), "XOpenGL"))
        snprintf(out, n, "XOpenGL: the curve of the XOpenGL community renderer. "
                 "Brightness scales each colour by the same factor, so hues are kept.");
    else
        snprintf(out, n, "D3D9: the Direct3D 9 curve. Brightness is a gamma power "
                 "applied to red, green and blue separately.");
}

/* Engine patches 0008 and 0022: every third frame out of sight, every sixth
 * beyond 4000 units; nearby and visible characters are never slowed. */
static void ai_lod_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    if (dxl_es_bool(s->app->es, DXL_ES_AI_LOD))
        snprintf(out, n, "On: characters out of sight and not nearby decide what to do "
                 "less often, so the game runs faster. They still move normally, but "
                 "may react a moment later.");
    else
        snprintf(out, n, "Off: every character decides what to do every frame, seen or "
                 "not. Slower.");
}

/* ---- anti-aliasing: a device limit, not a preference -------------------- */

/* A GPU whose multisample resolve is broken (core/renderers.h). */
static int aa_enabled(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (!dxl_gpu_msaa_broken(&s->app->gpu)) return 1;
    snprintf(why, n, "Kept off on the %s: its multisample resolve turns the edges "
             "of polygons into speckle.", s->app->gpu.vulkan_device);
    return 0;
}

static const char *const aa_labels[] = { "Off", "2x MSAA", "4x MSAA", NULL };

static int bloom_on(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (dxl_es_bool(s->app->es, DXL_ES_BLOOM)) return 1;
    snprintf(why, n, "Turn Bloom on to set its strength.");
    return 0;
}

/* Render resolution: Performance.RenderScale, offered as the panel's own
 * resolution and the usual ones below it (dxl_es_render_heights). */
static int render_heights(int *h, int max) {
    return dxl_es_render_heights(dxl_target_get()->panel_h, h, max);
}
static int render_current(dxl_session *s, const int *h, int count) {
    double scale = dxl_es_number(s->app->es, DXL_ES_RENDER_SCALE);
    int panel_h = dxl_target_get()->panel_h, best = -1;
    for (int i = 0; i < count; i++)
        if (fabs(scale - (double)h[i] / panel_h) < 0.002) best = i;
    return best;
}
static void render_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    int h[8], count = render_heights(h, 8), i = render_current(s, h, count);
    const dxl_target *t = dxl_target_get();
    if (i < 0)
        snprintf(out, n, "%d%%", (int)(dxl_es_number(s->app->es, DXL_ES_RENDER_SCALE) * 100 + 0.5));
    else
        snprintf(out, n, "%dx%d%s", (int)((double)t->panel_w * h[i] / t->panel_h + 0.5), h[i],
                 i == 0 ? " (native)" : "");
}
static void render_step(dxl_session *s, const dxl_row *r, int dir) {
    int h[8], count = render_heights(h, 8), i = render_current(s, h, count);
    i = i < 0 ? 0 : i + dir;          /* right: fewer lines */
    if (i < 0 || i >= count) return;
    dxl_es_set_number(s->app->es, DXL_ES_RENDER_SCALE, (double)h[i] / dxl_target_get()->panel_h);
}
static int render_enabled(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (!strcmp(dxl_es_choice(s->app->es, DXL_ES_RENDER_TYPE), "Vulkan")) return 1;
    snprintf(why, n, "Only the Vulkan renderer can draw below the screen's resolution; "
             "this one draws at the screen's own.");
    return 0;
}

static const dxl_row rows[] = {
    { .label = "Renderer", .value = renderer_value, .step = renderer_step,
      .activate = renderer_activate, .reset = renderer_reset, .describe = renderer_describe },
    { .label = "CPU mode", .visible = cpu_visible, .value = cpu_value, .step = cpu_step,
      .reset = cpu_reset, .describe = cpu_describe },
    { .label = "Resolution",
      .help = "The resolution the game is drawn at, scaled up to fill the screen. Fewer "
              "pixels are faster -- the GPU's work, and a share of the CPU's -- and softer, "
              "the HUD and menus included, which also draw larger. The lowest is 480 "
              "lines, the least the game's menus fit in.",
      .enabled = render_enabled, .value = render_value, .step = render_step,
      .reset = es_reset, .arg = DXL_ES_RENDER_SCALE },
    { .label = "Distant AI", .value = es_value, .step = es_step, .reset = es_reset,
      .describe = ai_lod_describe, .arg = DXL_ES_AI_LOD },
    { .label = "Vertical sync",
      .help = "Waits for the display's refresh before showing a frame. Removes tearing, "
              "but when the game runs below the refresh rate it then holds frames to a "
              "half or a third of it. Off shows each frame as soon as it is drawn.",
      .value = es_value, .step = es_step, .reset = es_reset, .arg = DXL_ES_VSYNC },
    { .label = "Brightness", .value = brightness_value, .slider = brightness_slider,
      .step = brightness_step, .reset = brightness_reset, .describe = brightness_describe },
    { .label = "Lighting", .value = es_value, .step = es_step, .reset = es_reset,
      .describe = light_describe, .arg = DXL_ES_LIGHT, .choice_labels = light_labels },
    { .label = "Gamma curve", .value = es_value, .step = es_step, .reset = es_reset,
      .describe = gamma_describe, .arg = DXL_ES_GAMMA, .choice_labels = gamma_labels },
    { .label = "Bloom",
      .help = "A soft glow around bright lights, added by Surreal Engine. Costs GPU time, "
              "which a handheld rarely has to spare.",
      .value = es_value, .step = es_step, .reset = es_reset, .arg = DXL_ES_BLOOM },
    { .label = "Bloom strength",
      .help = "How far the glow spreads and how bright it is.",
      .enabled = bloom_on, .value = es_value, .slider = es_slider, .step = es_step,
      .reset = es_reset, .arg = DXL_ES_BLOOM_AMOUNT },
    { .label = "Anti-aliasing",
      .help = "Multisampling smooths the edges of polygons. 2x and 4x cost GPU time and "
              "memory.",
      .enabled = aa_enabled, .value = es_value, .step = es_step, .reset = es_reset,
      .arg = DXL_ES_ANTIALIAS, .choice_labels = aa_labels },
    { .label = "Decals",
      .help = "Scorch marks, bullet holes and blood left on walls and floors.",
      .value = decals_value, .step = decals_step, .reset = decals_reset },
};

dxl_rows tab_video_rows(void) {
    return (dxl_rows){ rows, (int)(sizeof rows / sizeof *rows) };
}
