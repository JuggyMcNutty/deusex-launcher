/* The settings' registry (core/settings.h): its rows, read and written
 * through each place's codec in both generations of the system ini, the
 * sizes, the run's words, Settings.json, the presets, and the defaults a
 * new profile has. The inis are written here, a line or two each. */
#include "test.h"
#include "core/engine_sections.h"
#include "core/engine_settings.h"
#include "core/ini.h"
#include "core/profile.h"
#include "core/settings.h"

#include <math.h>

#ifndef DXL_ES_DEFAULTS
#define DXL_ES_DEFAULTS "data/engine-settings.json.default"
#endif

static dxl_ini *parse(const char *text) { return dxl_ini_parse(text, strlen(text)); }

static const dxl_setting *row(const char *id) {
    const dxl_setting *s = dxl_setting_find(id);
    if (!s) fprintf(stderr, "  no row %s\n", id);
    return s;
}

static const char *get(const dxl_stores *st, const char *id) {
    static char v[512];
    if (!row(id)) return "";
    dxl_setting_get(st, row(id), v, sizeof v);
    return v;
}

static int set(dxl_stores *st, const char *id, const char *value) {
    return row(id) ? dxl_setting_set(st, row(id), value) : -2;
}

static int same_number(const char *a, const char *b) { return fabs(atof(a) - atof(b)) < 1e-4; }

static void open_all(dxl_stores *st) {
    memset(st, 0, sizeof *st);
    st->system = dxl_ini_new();
    st->user = dxl_ini_new();
    st->launcher = dxl_ini_new();
    st->profile = dxl_ini_new();
    st->engine = dxl_es_open("", DXL_ES_DEFAULTS);
}

static void close_all(dxl_stores *st) {
    dxl_ini_free(st->system);
    dxl_ini_free(st->user);
    dxl_ini_free(st->launcher);
    dxl_ini_free(st->profile);
    dxl_es_free(st->engine);
    memset(st, 0, sizeof *st);
}

/* Every row well made: a page of the list, an id of its own, a default
 * it takes and reads back; a number's default in its range, a choice's
 * among its choices. */
static void test_rows(void) {
    dxl_stores st;
    open_all(&st);
    size_t n = dxl_settings_count();
    CHECK(n > 50);
    for (size_t i = 0; i < n; i++) {
        const dxl_setting *s = dxl_setting_at(i);
        int page = 0;
        for (size_t p = 0; p < dxl_settings_page_count(); p++) page |= !strcmp(dxl_settings_page(p), s->page);
        if (!page) fprintf(stderr, "  %s: no page %s\n", s->id, s->page);
        CHECK(page);
        for (size_t j = 0; j < i; j++) CHECK(strcmp(dxl_setting_at(j)->id, s->id) != 0);
        if (s->kind == DXL_SET_HEADING || s->kind == DXL_SET_ACTION) continue;
        CHECK(s->def != NULL);
        if (!s->def || (s->flags & DXL_SET_PRESET)) continue;
        if (s->kind == DXL_SET_NUMBER) {
            CHECK(s->min < s->max && s->step > 0);
            CHECK(atof(s->def) >= s->min && atof(s->def) <= s->max);
        }
        if (s->kind == DXL_SET_CHOICE && s->choices && !(s->flags & DXL_SET_RENDERER)) {
            int among = 0;
            for (int c = 0; s->choices[c].value; c++) among |= !strcmp(s->choices[c].value, s->def);
            if (!among) fprintf(stderr, "  %s: default %s is no choice\n", s->id, s->def);
            CHECK(among);
        }
        int rc = dxl_setting_set(&st, s, s->def);
        if (rc) fprintf(stderr, "  %s: default %s refused\n", s->id, s->def);
        CHECK_INT(rc, 0);
        char v[512];
        dxl_setting_get(&st, s, v, sizeof v);
        if (s->kind == DXL_SET_NUMBER ? !same_number(v, s->def) : strcmp(v, s->def) != 0) {
            fprintf(stderr, "  %s: default %s read back as %s\n", s->id, s->def, v);
            CHECK(0);
        }
    }
    close_all(&st);
}

/* The retail sections are read until the engine's own are there; a key
 * missing in the generation read is the engine's default, never the other
 * generation's; a value is written to both. */
static void test_generations(void) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    st.system = parse("[WinDrv.WindowsClient]\nStartupFullscreen=True\nBrightness=0.700000\n"
                      "WindowedViewportX=800\nWindowedViewportY=600\n"
                      "[Galaxy.GalaxyAudioSubsystem]\nMusicVolume=153\nOutputRate=22050Hz\n");
    CHECK_INT(dxl_settings_engine_generation(st.system), 0);
    CHECK_STR(get(&st, "display.fullscreen"), "1");
    CHECK_STR(get(&st, "display.brightness"), "0.7");
    CHECK_STR(get(&st, "display.window_size"), "800x600");
    CHECK_STR(get(&st, "audio.music_volume"), "0.6");
    CHECK_STR(get(&st, "audio.rate"), "22050");
    CHECK_STR(get(&st, "audio.sound_volume"), "0.784314");
    CHECK_INT(set(&st, "display.fullscreen", "0"), 0);
    CHECK_STR(dxl_ini_get(st.system, "WinDrv.WindowsClient", "StartupFullscreen"), "False");
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "StartupFullscreen"), "False");
    dxl_ini_free(st.system);

    st.system = parse("[WinDrv.WindowsClient]\nBrightness=0.700000\n[Engine.DXEngineClient]\nBrightness=0.450000\n"
                      "[Engine.DXEngineAudioDevice]\nMusicVolume=128\n[Galaxy.GalaxyAudioSubsystem]\nSoundVolume=100\n");
    CHECK_INT(dxl_settings_engine_generation(st.system), 1);
    CHECK_STR(get(&st, "display.brightness"), "0.45");
    CHECK_STR(get(&st, "audio.music_volume"), "0.501961");
    CHECK_STR(get(&st, "audio.sound_volume"), "0.784314");
    dxl_ini_free(st.system);
}

/* Read as the engine reads, written as it writes. */
static void test_codecs(void) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    st.system = parse("[Engine.DXEngineAudioDevice]\nReverseStereo=TRUE\nUseReverb=No\nOutputRate=22050\n"
                      "EffectsChannels=24 channels\nMusicVolume=300\nAmbientFactor=0.5abc\n"
                      "[Engine.DXEngineClient]\nNoDynamicLights=True\nDecals=false\n");
    CHECK_STR(get(&st, "audio.reverse"), "1");          /* TRUE: any case, as the engine reads it */
    CHECK_STR(get(&st, "audio.reverb"), "0");
    CHECK_STR(get(&st, "audio.rate"), "44100");         /* no Hz: not read */
    CHECK_STR(get(&st, "audio.channels"), "24");        /* the leading number */
    CHECK_STR(get(&st, "audio.music_volume"), "0.172549");   /* a byte: 300 wraps to 44 */
    CHECK_STR(get(&st, "audio.ambient"), "0.5");
    CHECK_STR(get(&st, "graphics.dynamic_lights"), "0");
    CHECK_STR(get(&st, "graphics.decals"), "0");

    CHECK_INT(set(&st, "audio.reverb", "1"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "UseReverb"), "True");
    CHECK_INT(set(&st, "graphics.dynamic_lights", "1"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "NoDynamicLights"), "False");
    CHECK_INT(set(&st, "audio.music_volume", "0.5"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "MusicVolume"), "128");
    CHECK_INT(set(&st, "audio.music_volume", "0.627451"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "MusicVolume"), "160");
    CHECK_INT(set(&st, "audio.rate", "11025"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "OutputRate"), "11025Hz");
    CHECK_INT(set(&st, "audio.ambient", "0.35"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "AmbientFactor"), "0.350000");
    CHECK_INT(set(&st, "audio.channels", "32"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "EffectsChannels"), "32");
    CHECK_INT(set(&st, "audio.channels", "12"), -1);    /* no such choice */
    CHECK_INT(set(&st, "audio.reverb", "yes"), -1);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineAudioDevice", "UseReverb"), "True");
    dxl_ini_free(st.system);
}

/* A size is two places, read and written as WxH; a number clamped. */
static void test_sizes_and_numbers(void) {
    dxl_stores st;
    open_all(&st);
    dxl_engine_sections_seed(st.system);
    CHECK_INT(set(&st, "display.window_size", "1600x900"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "WindowedViewportX"), "1600");
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "WindowedViewportY"), "900");
    CHECK_STR(dxl_ini_get(st.system, "WinDrv.WindowsClient", "WindowedViewportX"), "1600");
    CHECK_STR(get(&st, "display.window_size"), "1600x900");
    CHECK_INT(set(&st, "display.window_size", "100x100"), -1);
    CHECK_INT(set(&st, "display.window_size", "wide"), -1);
    CHECK_INT(set(&st, "display.window_size", "0x0"), -1);      /* a window has no desktop's size */
    CHECK_STR(get(&st, "display.window_size"), "1600x900");

    /* Full screen's size: the desktop's, 0x0, a new profile's; a size, the
     * display's nearest mode as the game starts, in both generations. */
    CHECK_STR(get(&st, "display.fullscreen_size"), "0x0");
    CHECK_INT(set(&st, "display.fullscreen_size", "1920x1080"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "FullscreenViewportX"), "1920");
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "FullscreenViewportY"), "1080");
    CHECK_STR(dxl_ini_get(st.system, "WinDrv.WindowsClient", "FullscreenViewportX"), "1920");
    CHECK_STR(dxl_ini_get(st.system, "WinDrv.WindowsClient", "FullscreenViewportY"), "1080");
    CHECK_STR(get(&st, "display.fullscreen_size"), "1920x1080");
    CHECK_INT(set(&st, "display.fullscreen_size", "1680x1050"), 0);   /* none of the list, as Screen Resolution sets */
    CHECK_STR(get(&st, "display.fullscreen_size"), "1680x1050");
    CHECK_INT(set(&st, "display.fullscreen_size", "0x0"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "FullscreenViewportX"), "0");
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "FullscreenViewportY"), "0");
    CHECK_STR(get(&st, "display.fullscreen_size"), "0x0");
    CHECK_INT(set(&st, "display.fullscreen_size", "640x0"), -1);

    CHECK_INT(set(&st, "display.fov", "200"), 0);
    CHECK_STR(dxl_ini_get(st.user, "Engine.PlayerPawn", "DefaultFOV"), "120.000000");
    CHECK_STR(dxl_ini_get(st.user, "Engine.PlayerPawn", "DesiredFOV"), "120.000000");
    CHECK_INT(set(&st, "display.fov", " 80 "), 0);
    CHECK_STR(get(&st, "display.fov"), "80");
    CHECK_INT(set(&st, "display.fov", "80 degrees"), -1);
    CHECK_INT(set(&st, "display.brightness", "0.55"), 0);
    CHECK_STR(dxl_ini_get(st.system, "Engine.DXEngineClient", "Brightness"), "0.550000");
    close_all(&st);
}

/* A choice is kept in its own spelling; a text with the row's characters
 * alone, its default for none. */
static void test_choices_and_text(void) {
    dxl_stores st;
    open_all(&st);
    CHECK_INT(set(&st, "hud.menu_theme", "blue & gold"), 0);
    CHECK_STR(dxl_ini_get(st.user, "DeusEx.DeusExPlayer", "MenuThemeName"), "Blue & Gold");
    CHECK_INT(set(&st, "hud.menu_theme", "Dark Blue"), -1);   /* the HUD's alone */
    CHECK_INT(set(&st, "hud.hud_theme", "Dark Blue"), 0);
    CHECK_INT(set(&st, "hud.background", "2"), 0);
    CHECK_STR(dxl_ini_get(st.user, "DeusEx.DeusExPlayer", "UIBackground"), "2");

    CHECK_INT(set(&st, "mp.name", "JC (Denton)!"), 0);
    CHECK_STR(dxl_ini_get(st.user, "DefaultPlayer", "Name"), "JC(Denton)");
    CHECK_INT(set(&st, "mp.name", "!!!"), 0);
    CHECK_STR(dxl_ini_get(st.user, "DefaultPlayer", "Name"), "Player");
    CHECK_INT(set(&st, "profile.engine_args", "-log -nosound"), 0);
    CHECK_STR(dxl_ini_get(st.profile, "Profile", "EngineArgs"), "-log -nosound");
    CHECK_INT(set(&st, "mp.master", ""), 0);
    CHECK_STR(get(&st, "mp.master"), "master.333networks.com");
    close_all(&st);
}

/* The next run's words: on and off, a two-word one too, kept nowhere. */
static void test_next_run(void) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    CHECK_INT(set(&st, "next.windowed", "1"), 0);
    CHECK_STR(st.next_run, "-nohard -noddraw");
    CHECK_INT(set(&st, "next.nosound", "1"), 0);
    CHECK_STR(st.next_run, "-nohard -noddraw -nosound");
    CHECK_STR(get(&st, "next.windowed"), "1");
    CHECK_INT(set(&st, "next.nosound", "1"), 0);
    CHECK_STR(st.next_run, "-nohard -noddraw -nosound");
    CHECK_INT(set(&st, "next.windowed", "0"), 0);
    CHECK_STR(st.next_run, "-nosound");
    CHECK_STR(get(&st, "next.windowed"), "0");
    CHECK_INT(set(&st, "next.nosound", "0"), 0);
    CHECK_STR(st.next_run, "");
    snprintf(st.next_run, sizeof st.next_run, "-nosoundx");
    CHECK_STR(get(&st, "next.nosound"), "0");
}

/* Settings.json's rows go through its own setters: a choice it does not
 * know is not written, a number on its own steps. */
static void test_engine_json(void) {
    dxl_stores st;
    open_all(&st);
    CHECK_INT(set(&st, "graphics.antialias", "MSAA2x"), 0);
    CHECK_STR(dxl_es_choice(st.engine, DXL_ES_ANTIALIAS), "MSAA2x");
    CHECK_INT(set(&st, "graphics.bloom_amount", "0.5"), 0);
    CHECK(fabs(dxl_es_number(st.engine, DXL_ES_BLOOM_AMOUNT) - 128) < 1e-9);
    CHECK_STR(get(&st, "graphics.bloom_amount"), "0.501961");
    CHECK_INT(set(&st, "display.render_scale", "0.75"), 0);
    CHECK(fabs(dxl_es_number(st.engine, DXL_ES_RENDER_SCALE) - 0.75) < 1e-9);
    CHECK_INT(set(&st, "graphics.renderer", "OpenGL"), 0);
    CHECK_STR(get(&st, "graphics.renderer"), "OpenGL");
    CHECK_INT(set(&st, "graphics.renderer", "D3D11"), -1);   /* fatal on Linux: refused */
    CHECK_STR(get(&st, "graphics.renderer"), "OpenGL");
    close_all(&st);
}

/* The renderer where Settings.json names no Type: the one the engine takes
 * by the ini's GameRenderDevice, the software one for SoftDrv's, else
 * Vulkan; another row changed names none, a renderer chosen does. */
static void test_renderer_unnamed(void) {
    dxl_stores st;
    open_all(&st);
    CHECK_INT(dxl_es_names_type(st.engine), 0);
    CHECK_STR(get(&st, "graphics.renderer"), "Vulkan");
    dxl_ini_set(st.system, "Engine.Engine", "GameRenderDevice", "SoftDrv.SoftwareRenderDevice");
    CHECK_STR(get(&st, "graphics.renderer"), "Software");
    CHECK_INT(set(&st, "graphics.antialias", "Off"), 0);
    CHECK_INT(dxl_es_names_type(st.engine), 0);
    CHECK_STR(get(&st, "graphics.renderer"), "Software");
    dxl_ini_set(st.system, "Engine.Engine", "GameRenderDevice", "D3DDrv.D3DRenderDevice");
    CHECK_STR(get(&st, "graphics.renderer"), "Vulkan");
    /* Chosen: named, even as the defaults file's Vulkan, and kept over the ini */
    CHECK_INT(set(&st, "graphics.renderer", "Vulkan"), 0);
    CHECK_INT(dxl_es_names_type(st.engine), 1);
    dxl_ini_set(st.system, "Engine.Engine", "GameRenderDevice", "SoftDrv.SoftwareRenderDevice");
    CHECK_STR(get(&st, "graphics.renderer"), "Vulkan");
    close_all(&st);
}

/* A preset applied is the one matched; a row changed after, none. */
static void test_presets(void) {
    dxl_stores st;
    open_all(&st);
    dxl_engine_sections_seed(st.system);
    for (size_t i = 0; i < dxl_preset_count(); i++) {
        const dxl_preset *p = dxl_preset_at(i);
        dxl_preset_apply(&st, p);
        if (dxl_preset_matching(&st) != p) fprintf(stderr, "  preset %s not matched\n", p->id);
        CHECK(dxl_preset_matching(&st) == p);
        for (const dxl_preset_value *v = p->values; v->setting; v++) CHECK(row(v->setting) != NULL);
    }
    const dxl_preset *perf = NULL;
    for (size_t i = 0; i < dxl_preset_count(); i++)
        if (!strcmp(dxl_preset_at(i)->id, "performance")) perf = dxl_preset_at(i);
    CHECK(perf != NULL);
    if (perf) {
        dxl_preset_apply(&st, perf);
        CHECK_INT(set(&st, "graphics.bloom", "1"), 0);
        CHECK(dxl_preset_matching(&st) == NULL);
    }
    close_all(&st);
}

/* A GPU that multisamples badly: anti-aliasing takes Off alone, from a
 * preset, a reset or the row, and each preset is matched without it. */
static void test_no_msaa(void) {
    dxl_stores st;
    open_all(&st);
    dxl_engine_sections_seed(st.system);
    st.no_msaa = 1;
    for (size_t i = 0; i < dxl_preset_count(); i++) {
        const dxl_preset *p = dxl_preset_at(i);
        dxl_preset_apply(&st, p);
        CHECK_STR(get(&st, DXL_ANTIALIAS_ROW), "Off");
        CHECK(dxl_preset_matching(&st) == p);
    }
    CHECK_INT(set(&st, DXL_ANTIALIAS_ROW, "MSAA4x"), 0);
    CHECK_STR(get(&st, DXL_ANTIALIAS_ROW), "Off");
    CHECK_INT(set(&st, DXL_ANTIALIAS_ROW, "Sixteen"), -1);
    dxl_settings_reset(&st, NULL, "Graphics");
    CHECK_STR(get(&st, DXL_ANTIALIAS_ROW), "Off");
    /* Multisampling the file holds from elsewhere: the preset still matches. */
    const dxl_preset *p = dxl_preset_at(0);
    dxl_preset_apply(&st, p);
    dxl_es_set_choice(st.engine, DXL_ES_ANTIALIAS, "MSAA2x");
    CHECK(dxl_preset_matching(&st) == p);
    st.no_msaa = 0;
    CHECK_INT(set(&st, DXL_ANTIALIAS_ROW, "MSAA4x"), 0);
    CHECK_STR(get(&st, DXL_ANTIALIAS_ROW), "MSAA4x");
    close_all(&st);
}

/* Back to the defaults: what a new profile has (the install's Default.ini
 * as the import seeds it, its DefUser.ini), a page at a time; with none,
 * the rows' own. */
static void test_reset(void) {
    dxl_stores fresh;
    memset(&fresh, 0, sizeof fresh);
    dxl_profile_defaults(DXL_FIXTURES, NULL, &fresh.system, &fresh.user);
    CHECK(fresh.system && fresh.user);
    if (!fresh.system || !fresh.user) return;
    fresh.engine = dxl_es_open("", DXL_ES_DEFAULTS);
    CHECK_INT(dxl_settings_engine_generation(fresh.system), 1);

    dxl_stores st;
    open_all(&st);
    dxl_engine_sections_seed(st.system);
    set(&st, "display.brightness", "0.9");
    set(&st, "display.fullscreen", "0");
    set(&st, "audio.rate", "11025");
    set(&st, "mouse.sensitivity", "8");
    set(&st, "graphics.antialias", "Off");
    dxl_settings_reset(&st, &fresh, "Display");
    CHECK_STR(get(&st, "display.brightness"), "0.6");    /* Default.ini's */
    CHECK_STR(get(&st, "display.fullscreen"), "1");
    CHECK_STR(get(&st, "audio.rate"), "11025");          /* another page's: as it was */
    dxl_settings_reset(&st, &fresh, NULL);
    CHECK_STR(get(&st, "audio.rate"), "22050");
    CHECK_STR(get(&st, "mouse.sensitivity"), "3");        /* DefUser.ini's */
    CHECK_STR(get(&st, "graphics.antialias"), "MSAA4x");  /* the defaults file's */
    dxl_settings_reset(&st, NULL, "Keyboard & mouse");
    CHECK_STR(get(&st, "mouse.sensitivity"), "5");        /* the engine's, with nothing kept */
    close_all(&st);
    dxl_ini_free(fresh.system);
    dxl_ini_free(fresh.user);
    dxl_es_free(fresh.engine);
}

/* Each of the engine's keys a row keeps is one the seeding writes, and a
 * row's default is the engine's own: what a new engine section holds. */
static void test_engine_keys_seeded(void) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    st.system = dxl_ini_new();
    dxl_engine_sections_seed(st.system);
    for (size_t i = 0; i < dxl_settings_count(); i++) {
        const dxl_setting *s = dxl_setting_at(i);
        int engine = 0;
        for (int p = 0; p < DXL_SET_PLACES && s->at[p].store != DXL_STORE_NONE; p++) {
            const dxl_set_place *pl = &s->at[p];
            if (pl->store != DXL_STORE_SYSTEM || pl->gen != DXL_GEN_ENGINE) continue;
            engine = 1;
            if (!dxl_ini_get(st.system, pl->section, pl->key))
                fprintf(stderr, "  %s: [%s] %s not seeded\n", s->id, pl->section, pl->key);
            CHECK(dxl_ini_get(st.system, pl->section, pl->key) != NULL);
        }
        if (!engine) continue;
        char v[512];
        CHECK_INT(dxl_setting_get(&st, s, v, sizeof v), 1);
        if (s->kind == DXL_SET_NUMBER ? !same_number(v, s->def) : strcmp(v, s->def) != 0) {
            fprintf(stderr, "  %s: default %s, the engine's %s\n", s->id, s->def, v);
            CHECK(0);
        }
    }
    dxl_ini_free(st.system);
}

TEST_MAIN_BEGIN
    RUN(test_rows);
    RUN(test_generations);
    RUN(test_codecs);
    RUN(test_sizes_and_numbers);
    RUN(test_choices_and_text);
    RUN(test_next_run);
    RUN(test_engine_json);
    RUN(test_renderer_unnamed);
    RUN(test_presets);
    RUN(test_no_msaa);
    RUN(test_reset);
    RUN(test_engine_keys_seeded);
TEST_MAIN_END
