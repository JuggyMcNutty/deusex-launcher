#include "core/detail.h"
#include "core/renderdev.h"

#include <stdlib.h>

#define MB (1024ULL * 1024ULL)
#define CLIENT "WinDrv.WindowsClient"
#define AUDIO  "Galaxy.GalaxyAudioSubsystem"

static void line(dxl_buf *b, dxl_loc *loc, const char *key) {
    dxl_buf_puts(b, dxl_loc_general(loc, "Startup", key));
    dxl_buf_puts(b, "\r\n");
}

char *dxl_detail_apply(dxl_ini *ini, dxl_loc *loc, const dxl_machine *m) {
    dxl_buf b;
    dxl_buf_init(&b);
    dxl_buf_add(&b, "", 0);

    const char *driver = dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice");
    if (!driver) driver = "";
    int flags = dxl_ini_get_int(ini, driver, "DescFlags", 0);

    /* 280e6 × GSecondsPerCycle > 1: a CPU under 280 MHz. An unknown clock
     * counts as fast. */
    int slow_cpu = m->cpu_mhz > 0 && m->cpu_mhz < 280.0;
    if (dxl_stricmp(driver, DXL_SOFTWARE_DEVICE) == 0 || slow_cpu ||
        dxl_stricmp(driver, DXL_D3D_DEVICE) == 0)
        dxl_ini_set(ini, CLIENT, "MinDesiredFrameRate", "1");

    if (!m->mmx || m->memory <= 64 * MB) {
        line(&b, loc, "SoundLow");
        dxl_ini_set(ini, AUDIO, "UseReverb", "False");
        dxl_ini_set(ini, AUDIO, "OutputRate", "11025Hz");
        dxl_ini_set(ini, AUDIO, "UseSpatial", "False");
        dxl_ini_set(ini, AUDIO, "UseFilter", "False");
        dxl_ini_set_bool(ini, AUDIO, "LowSoundQuality", 1);
    } else {
        line(&b, loc, "SoundHigh");
    }

    if (m->memory < 96 * MB || (flags & DXL_DESCF_LOW_DETAIL_SKINS)) {
        line(&b, loc, "SkinsLow");
        dxl_ini_set(ini, CLIENT, "SkinDetail", "Medium");
    } else {
        line(&b, loc, "SkinsHigh");
    }

    if (m->memory < 64 * MB || (flags & DXL_DESCF_LOW_DETAIL_WORLD)) {
        line(&b, loc, "WorldLow");
        dxl_ini_set(ini, CLIENT, "TextureDetail", "Medium");
    } else {
        line(&b, loc, "WorldHigh");
    }

    dxl_ini_set(ini, CLIENT, "WindowedViewportX", "640");
    dxl_ini_set(ini, CLIENT, "WindowedViewportY", "480");
    dxl_ini_set(ini, CLIENT, "WindowedColorBits", "16");
    dxl_ini_set(ini, CLIENT, "FullscreenViewportX", "640");
    dxl_ini_set(ini, CLIENT, "FullscreenViewportY", "480");
    dxl_ini_set(ini, CLIENT, "FullscreenColorBits", "16");
    line(&b, loc, "ResHigh");
    return b.data;
}
