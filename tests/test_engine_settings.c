#include "test.h"
#include "core/engine_settings.h"
#include "core/json.h"

#include <math.h>
#include <sys/stat.h>
#include <unistd.h>

static char dir[512];

static void setup(const char *tag) {
    snprintf(dir, sizeof dir, "/tmp/dxl-es-%s-%d", tag, (int)getpid());
    mkdir(dir, 0755);
}

static char *in_dir(const char *leaf) {
    static char p[4][640];
    static int n;
    char *out = p[n++ % 4];
    snprintf(out, 640, "%s/%s", dir, leaf);
    return out;
}

static void put(const char *leaf, const char *text) {
    FILE *f = fopen(in_dir(leaf), "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static void teardown(void) {
    remove(in_dir("Settings.json"));
    remove(in_dir("default.json"));
    remove(in_dir("sub/deeper/Settings.json"));
    rmdir(in_dir("sub/deeper"));
    rmdir(in_dir("sub"));
    rmdir(dir);
}

/* The packaged default for the handheld. */
static const char *device_default =
    "{\"RenderDevice\": {\"Type\": \"Vulkan\", \"Antialias\": \"Off\", \"Light\": \"Normal\","
    " \"Gamma\": \"D3D9\", \"GammaCorrectScreenshots\": false, \"UseVSync\": false, \"Hdr\": false,"
    " \"HdrScale\": 128, \"Bloom\": false, \"BloomAmount\": 128, \"UseDebugLayer\": false},"
    " \"Games\": {\"SearchList\": [], \"LastSelected\": 0}}";

static void test_missing_file_seeds_from_default(void) {
    setup("missing");
    put("default.json", device_default);
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), in_dir("default.json"));
    CHECK_INT(dxl_es_existed(s), 0);
    CHECK_INT(dxl_es_dirty(s), 1);
    CHECK_STR(dxl_es_choice(s, DXL_ES_RENDER_TYPE), "Vulkan");
    CHECK_STR(dxl_es_choice(s, DXL_ES_ANTIALIAS), "Off");
    CHECK_INT(dxl_es_bool(s, DXL_ES_VSYNC), 0);
    CHECK_INT(dxl_es_bool(s, DXL_ES_PAD_ENABLED), 1);
    CHECK_INT(dxl_es_was_present(s, DXL_ES_RENDER_TYPE), 0);
    dxl_es_free(s);
    teardown();
}

/* A file the engine cannot parse costs every setting -- MSAA4x included,
 * which is speckle on the PowerVR. It must be replaced, not trusted. */
static void test_corrupt_file_is_replaced(void) {
    setup("corrupt");
    put("default.json", device_default);
    put("Settings.json", "{\"RenderDevice\": {\"Type\": \"Vulkan\",");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), in_dir("default.json"));
    CHECK_INT(dxl_es_was_corrupt(s), 1);
    CHECK_STR(dxl_es_choice(s, DXL_ES_ANTIALIAS), "Off");
    dxl_err e;
    CHECK_INT(dxl_es_save(s, &e), 0);
    dxl_es_free(s);

    dxl_json *j = dxl_json_load(in_dir("Settings.json"), NULL);
    CHECK(j != NULL);
    CHECK_STR(dxl_json_get_string(dxl_json_get(j, "RenderDevice"), "Antialias", NULL), "Off");
    dxl_json_free(j);
    teardown();
}

/* The engine reads a missing member as empty/false/0 -- HdrScale 0 -- so a
 * save must carry every RenderDevice member even if the file lacked some. */
static void test_save_writes_every_field_and_keeps_games(void) {
    setup("full");
    put("Settings.json",
        "{\"RenderDevice\": {\"Type\": \"Vulkan\"}, \"Games\": {\"SearchList\": [\"/x\"], \"LastSelected\": 0}}");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), NULL);
    CHECK_INT(dxl_es_was_present(s, DXL_ES_RENDER_TYPE), 1);
    CHECK_INT(dxl_es_was_present(s, DXL_ES_HDR_SCALE), 0);
    dxl_err e;
    CHECK_INT(dxl_es_save(s, &e), 0);
    dxl_es_free(s);

    dxl_json *j = dxl_json_load(in_dir("Settings.json"), NULL);
    dxl_json *rd = dxl_json_get(j, "RenderDevice");
    for (int f = 0; f < DXL_ES_FIELD_COUNT; f++) {
        const dxl_es_info *in = dxl_es_describe((dxl_es_field)f);
        CHECK(dxl_json_get(dxl_json_get(j, in->section), in->key) != NULL);
    }
    CHECK_INT((int)dxl_json_get_number(rd, "HdrScale", 0), 128);
    CHECK(dxl_json_get(dxl_json_get(j, "Games"), "SearchList") != NULL);
    dxl_json_free(j);
    teardown();
}

static void test_setters_validate(void) {
    setup("validate");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), NULL);
    CHECK_INT(dxl_es_set_choice(s, DXL_ES_LIGHT, "OneX"), 0);
    CHECK_STR(dxl_es_choice(s, DXL_ES_LIGHT), "OneX");
    /* Not a value the engine knows: refused, previous value stands. */
    CHECK_INT(dxl_es_set_choice(s, DXL_ES_LIGHT, "Dazzling"), -1);
    CHECK_STR(dxl_es_choice(s, DXL_ES_LIGHT), "OneX");
    CHECK_INT(dxl_es_set_choice(s, DXL_ES_RENDER_TYPE, "OpenGLDrv.OpenGLRenderDevice"), -1);

    dxl_es_set_number(s, DXL_ES_PAD_DEADZONE, 9.0);
    CHECK(dxl_es_number(s, DXL_ES_PAD_DEADZONE) <= 0.5);
    dxl_es_set_number(s, DXL_ES_BLOOM_AMOUNT, 255);
    CHECK(dxl_es_number(s, DXL_ES_BLOOM_AMOUNT) <= 255);

    /* Stepping by 0.05 lands on the grid, not near it. */
    double v = 0.05;
    for (int i = 0; i < 5; i++) v += 0.05;
    dxl_es_set_number(s, DXL_ES_PAD_DEADZONE, v);
    CHECK(dxl_es_number(s, DXL_ES_PAD_DEADZONE) == 0.3);
    dxl_es_free(s);
    teardown();
}

static void test_reset_uses_packaged_default(void) {
    setup("reset");
    put("default.json", "{\"RenderDevice\": {\"UseVSync\": true, \"Light\": \"BrighterActors\"}}");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), in_dir("default.json"));
    dxl_es_set_bool(s, DXL_ES_VSYNC, 0);
    dxl_es_set_choice(s, DXL_ES_LIGHT, "Normal");
    dxl_es_set_choice(s, DXL_ES_ANTIALIAS, "MSAA4x");
    dxl_es_reset_section(s, "RenderDevice");
    CHECK_INT(dxl_es_bool(s, DXL_ES_VSYNC), 1);
    CHECK_STR(dxl_es_choice(s, DXL_ES_LIGHT), "BrighterActors");
    /* Absent from the packaged file: the built-in default, which is Off. */
    CHECK_STR(dxl_es_choice(s, DXL_ES_ANTIALIAS), "Off");
    dxl_es_free(s);
    teardown();
}

/* A Settings.json written before a field existed takes the port's packaged
 * default for it -- an install upgraded on the handheld gets AI level of
 * detail on -- and a save writes the new section. */
static void test_new_field_takes_the_packaged_default(void) {
    setup("newfield");
    put("default.json", "{\"Performance\": {\"AiLevelOfDetail\": true}}");
    put("Settings.json", device_default);
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), in_dir("default.json"));
    CHECK_INT(dxl_es_was_present(s, DXL_ES_AI_LOD), 0);
    CHECK_INT(dxl_es_bool(s, DXL_ES_AI_LOD), 1);
    dxl_err e;
    CHECK_INT(dxl_es_save(s, &e), 0);
    dxl_es_free(s);

    s = dxl_es_open(in_dir("Settings.json"), NULL);
    CHECK_INT(dxl_es_was_present(s, DXL_ES_AI_LOD), 1);
    CHECK_INT(dxl_es_bool(s, DXL_ES_AI_LOD), 1);
    dxl_es_free(s);

    /* Without a packaged default it is off: the table's built-in value. */
    s = dxl_es_open(in_dir("missing.json"), NULL);
    CHECK_INT(dxl_es_bool(s, DXL_ES_AI_LOD), 0);
    dxl_es_free(s);
    teardown();
}

/* The resolutions the Video tab offers: the panel's own, then the usual ones
 * below it, never under the 480 lines Deus Ex's menus need. */
static void test_render_heights(void) {
    int h[8];
    CHECK_INT(dxl_es_render_heights(720, h, 8), 3);
    CHECK_INT(h[0], 720); CHECK_INT(h[1], 540); CHECK_INT(h[2], 480);
    CHECK_INT(dxl_es_render_heights(1080, h, 8), 5);
    CHECK_INT(h[0], 1080); CHECK_INT(h[1], 900); CHECK_INT(h[4], 480);
    CHECK_INT(dxl_es_render_heights(768, h, 8), 4);   /* a 1366x768 laptop */
    CHECK_INT(h[0], 768); CHECK_INT(h[1], 720);
    CHECK_INT(dxl_es_render_heights(480, h, 8), 1);
    CHECK_INT(dxl_es_render_heights(400, h, 8), 1);   /* smaller than the floor: native only */
    CHECK_INT(h[0], 400);
    CHECK_INT(dxl_es_render_heights(1080, h, 2), 2);  /* never past max */

    /* RenderScale defaults to 1 (native) and stays in the engine's range. */
    setup("scale");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), NULL);
    CHECK(dxl_es_number(s, DXL_ES_RENDER_SCALE) == 1.0);
    dxl_es_set_number(s, DXL_ES_RENDER_SCALE, 0.1);
    CHECK(dxl_es_number(s, DXL_ES_RENDER_SCALE) == 0.25);

    /* Each offered height is kept as a scale the Video tab recognises again
     * (within 0.002) and the engine turns back into that size (engine patch
     * 0009: the native size times the scale as a float, rounded). A step grid
     * would break both: 480 of 720 lines once became 0.65, 832x468. */
    static const int panels[][2] = { { 1280, 720 }, { 1920, 1080 }, { 1366, 768 }, { 1280, 800 } };
    for (size_t p = 0; p < sizeof panels / sizeof *panels; p++) {
        int pw = panels[p][0], ph = panels[p][1], n = dxl_es_render_heights(ph, h, 8);
        for (int i = 0; i < n; i++) {
            double want = (double)h[i] / ph;
            dxl_es_set_number(s, DXL_ES_RENDER_SCALE, want);
            double got = dxl_es_number(s, DXL_ES_RENDER_SCALE);
            CHECK(fabs(got - want) < 0.002);
            CHECK_INT((int)lroundf((float)ph * (float)got), h[i]);
            CHECK_INT((int)lroundf((float)pw * (float)got), (int)((double)pw * h[i] / ph + 0.5));
        }
    }
    dxl_es_free(s);
    teardown();
}

static void test_save_creates_the_directory(void) {
    setup("mkdir");
    dxl_engine_settings *s = dxl_es_open(in_dir("sub/deeper/Settings.json"), NULL);
    dxl_err e;
    CHECK_INT(dxl_es_save(s, &e), 0);
    CHECK(access(in_dir("sub/deeper/Settings.json"), F_OK) == 0);
    CHECK_INT(dxl_es_dirty(s), 0);
    dxl_es_free(s);
    teardown();
}

/* Opening and saving an untouched, complete file changes no value. */
static void test_clean_file_is_not_dirty(void) {
    setup("clean");
    dxl_engine_settings *s = dxl_es_open(in_dir("Settings.json"), NULL);
    dxl_err e;
    dxl_es_save(s, &e);
    dxl_es_free(s);
    s = dxl_es_open(in_dir("Settings.json"), NULL);
    CHECK_INT(dxl_es_dirty(s), 0);
    CHECK_INT(dxl_es_was_present(s, DXL_ES_PAD_LAYOUT), 1);
    dxl_es_set_bool(s, DXL_ES_VSYNC, dxl_es_bool(s, DXL_ES_VSYNC));
    CHECK_INT(dxl_es_dirty(s), 0);
    dxl_es_free(s);
    teardown();
}

TEST_MAIN_BEGIN
    RUN(test_missing_file_seeds_from_default);
    RUN(test_corrupt_file_is_replaced);
    RUN(test_save_writes_every_field_and_keeps_games);
    RUN(test_setters_validate);
    RUN(test_reset_uses_packaged_default);
    RUN(test_new_field_takes_the_packaged_default);
    RUN(test_render_heights);
    RUN(test_save_creates_the_directory);
    RUN(test_clean_file_is_not_dirty);
TEST_MAIN_END
