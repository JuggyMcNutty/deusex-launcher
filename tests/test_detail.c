#include "test.h"
#include "scratch.h"
#include "core/detail.h"

/* The Detail page: dx-reverse-info/wizard.md "Detail". */

#define MB (1024ULL * 1024ULL)

static dxl_loc *loc;

static void setup(void) {
    const char *dir = scratch_dir("detail");
    scratch_write(dir, "Startup.int",
                  "[General]\r\nSoundLow=SL\r\nSoundHigh=SH\r\nSkinsLow=KL\r\nSkinsHigh=KH\r\n"
                  "WorldLow=WL\r\nWorldHigh=\"WH\"\r\nResHigh=RH\r\n");
    loc = dxl_loc_open(dir, NULL);
}

static dxl_ini *game(const char *renderer, int flags) {
    char text[256];
    snprintf(text, sizeof text, "[Engine.Engine]\r\nGameRenderDevice=%s\r\n[%s]\r\nDescFlags=%d\r\n",
             renderer, renderer, flags);
    return dxl_ini_parse(text, strlen(text));
}

static void test_a_fast_machine(void) {
    dxl_ini *ini = game("GlideDrv.GlideRenderDevice", 0);
    dxl_machine m = { .memory = 8192 * MB, .mmx = 1, .cpu_mhz = 3000 };
    char *text = dxl_detail_apply(ini, loc, &m);
    CHECK_STR(text, "SH\r\nKH\r\nWH\r\nRH\r\n");
    CHECK(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "UseReverb") == NULL);
    CHECK(dxl_ini_get(ini, "WinDrv.WindowsClient", "MinDesiredFrameRate") == NULL);
    /* The resolution is written whatever the machine. */
    CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "FullscreenViewportX"), "640");
    CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "WindowedColorBits"), "16");
    free(text);
    dxl_ini_free(ini);
}

/* MinDesiredFrameRate=1 for the software renderer, Direct3D, or a CPU under
 * 280 MHz. */
static void test_min_desired_frame_rate(void) {
    const char *renderers[] = { "SoftDrv.SoftwareRenderDevice", "D3DDrv.D3DRenderDevice" };
    dxl_machine m = { .memory = 8192 * MB, .mmx = 1, .cpu_mhz = 3000 };
    for (int i = 0; i < 2; i++) {
        dxl_ini *ini = game(renderers[i], 0);
        free(dxl_detail_apply(ini, loc, &m));
        CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "MinDesiredFrameRate"), "1");
        dxl_ini_free(ini);
    }
    dxl_ini *ini = game("GlideDrv.GlideRenderDevice", 0);
    m.cpu_mhz = 266;
    free(dxl_detail_apply(ini, loc, &m));
    CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "MinDesiredFrameRate"), "1");
    dxl_ini_free(ini);
}

/* The memory thresholds, each at its edge: sound at 64 MB or less, skins
 * under 96, world under 64. */
static void test_memory_edges(void) {
    struct { unsigned long long mb; const char *want; } cases[] = {
        { 96, "SH\r\nKH\r\nWH\r\nRH\r\n" },
        { 95, "SH\r\nKL\r\nWH\r\nRH\r\n" },
        { 65, "SH\r\nKL\r\nWH\r\nRH\r\n" },
        { 64, "SL\r\nKL\r\nWH\r\nRH\r\n" },
        { 63, "SL\r\nKL\r\nWL\r\nRH\r\n" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        dxl_ini *ini = game("GlideDrv.GlideRenderDevice", 0);
        dxl_machine m = { .memory = cases[i].mb * MB, .mmx = 1, .cpu_mhz = 3000 };
        char *text = dxl_detail_apply(ini, loc, &m);
        CHECK_STR(text, cases[i].want);
        free(text);
        dxl_ini_free(ini);
    }
}

static void test_low_sound_writes(void) {
    dxl_ini *ini = game("GlideDrv.GlideRenderDevice", 0);
    dxl_machine m = { .memory = 8192 * MB, .mmx = 0, .cpu_mhz = 3000 };   /* no MMX */
    char *text = dxl_detail_apply(ini, loc, &m);
    CHECK_STR(text, "SL\r\nKH\r\nWH\r\nRH\r\n");
    CHECK_STR(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "OutputRate"), "11025Hz");
    CHECK_STR(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "LowSoundQuality"), "True");
    CHECK_STR(dxl_ini_get(ini, "Galaxy.GalaxyAudioSubsystem", "UseSpatial"), "False");
    free(text);
    dxl_ini_free(ini);
}

/* The renderer's DescFlags: 8 low-detail skins, 4 low-detail world. */
static void test_desc_flags(void) {
    dxl_ini *ini = game("D3DDrv.D3DRenderDevice", 8 | 4);
    dxl_machine m = { .memory = 8192 * MB, .mmx = 1, .cpu_mhz = 3000 };
    char *text = dxl_detail_apply(ini, loc, &m);
    CHECK_STR(text, "SH\r\nKL\r\nWL\r\nRH\r\n");
    CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "SkinDetail"), "Medium");
    CHECK_STR(dxl_ini_get(ini, "WinDrv.WindowsClient", "TextureDetail"), "Medium");
    free(text);
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    setup();
    RUN(test_a_fast_machine);
    RUN(test_min_desired_frame_rate);
    RUN(test_memory_edges);
    RUN(test_low_sound_writes);
    RUN(test_desc_flags);
    dxl_loc_free(loc);
    scratch_remove(scratch_dir("detail"));
TEST_MAIN_END
