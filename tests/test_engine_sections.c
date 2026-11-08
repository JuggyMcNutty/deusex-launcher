#include "test.h"
#include "core/engine_sections.h"

/* Before the engine's sections: each from its retail key, as the engine
 * writes it; a retail key it would not read gives way to its default. */
static void test_from_retail(void) {
    const char *src = "[Engine.Engine]\nGameRenderDevice=D3DDrv.D3DRenderDevice\n"
                      "[WinDrv.WindowsClient]\nStartupFullscreen=True\nWindowedViewportX=640\nWindowedViewportY=480\n"
                      "Brightness=0.6\nMinDesiredFrameRate=1.0\nDecals=No\n"
                      "[Galaxy.GalaxyAudioSubsystem]\nMusicVolume=153\nOutputRate=22050Hz\nUseSurround=False\n"
                      "[D3DDrv.D3DRenderDevice]\nDetailTextures=False\nUsePrecache=True\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_INT(dxl_engine_sections_seed(ini), 66);
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "StartupFullscreen"), "True");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "WindowedViewportX"), "640");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "Brightness"), "0.600000");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "MinDesiredFrameRate"), "1.000000");   /* a float */
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "Decals"), "True");          /* No: refused, the default */
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "TextureDetail"), "High");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "MusicVolume"), "153");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "OutputRate"), "22050Hz");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "UseSurround"), "False");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "SpeechVolume"), "255");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "DetailTextures"), "False");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "UsePrecache"), "True");
    /* No key the engine neither reads nor writes. */
    CHECK_INT(dxl_ini_has_section(ini, "Engine.DXEngineWindowSystem"), 0);
    CHECK(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "Translucency") == NULL);
    CHECK(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "UseSpatial") == NULL);
    /* Seeded, again: nothing more. */
    CHECK_INT(dxl_engine_sections_seed(ini), 0);
    dxl_ini_free(ini);
}

/* Each key read as the engine reads its retail one (DXEngine's
 * IniProperty.cpp, the U*Device classes; bugs.md LCORE.18): NoLighting and
 * the original's config it keeps carried; a bool's yes or no refused for
 * the default; an empty float 0; a number by std::stoi, a byte wrapped, the
 * latency held; a rate or a detail by its name's token, in any case. */
static void test_as_the_engine_reads(void) {
    const char *src = "[Engine.Engine]\nGameRenderDevice=D3DDrv.D3DRenderDevice\n"
                      "[WinDrv.WindowsClient]\nNoLighting=True\nDecals=No\nScreenFlashes=Yes\nUseJoystick=\"true\"\n"
                      "Brightness=\nMipFactor=1.000000\nCaptureMouse=True\nScaleXYZ=1000.000000\n"
                      "WindowedViewportX= 800 px\nWindowedViewportY=+600\nFullscreenViewportX=99999999999\n"
                      "TextureDetail=medium\nSkinDetail=Ultra Low\nMinDesiredFrameRate=0.0078125\n"
                      "[Galaxy.GalaxyAudioSubsystem]\nUseDirectSound=True\nUseSpatial=False\nOutputRate=22050hz\n"
                      "MusicVolume=300\nLatency=5\nAmbientFactor=abc\nEffectsChannels=24 channels\n"
                      "[D3DDrv.D3DRenderDevice]\nTranslucency=False\nUseMipmapping=True\nUseVSync=Yes\n"
                      "Description=Direct3D\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_engine_sections_seed(ini);
    static const char *const want[][3] = {
        { "Engine.DXEngineClient", "NoLighting", "True" },
        { "Engine.DXEngineClient", "Decals", "True" },
        { "Engine.DXEngineClient", "ScreenFlashes", "True" },
        { "Engine.DXEngineClient", "UseJoystick", "True" },
        { "Engine.DXEngineClient", "Brightness", "0.000000" },
        { "Engine.DXEngineClient", "MipFactor", "1.000000" },
        { "Engine.DXEngineClient", "CaptureMouse", "True" },
        { "Engine.DXEngineClient", "CurvedSurfaces", "False" },
        { "Engine.DXEngineClient", "ScaleXYZ", "1000.000000" },
        { "Engine.DXEngineClient", "ScaleRUV", "0.000000" },
        { "Engine.DXEngineClient", "WindowedViewportX", "800" },
        { "Engine.DXEngineClient", "WindowedViewportY", "600" },
        { "Engine.DXEngineClient", "FullscreenViewportX", "0" },
        { "Engine.DXEngineClient", "TextureDetail", "Medium" },
        { "Engine.DXEngineClient", "SkinDetail", "High" },
        { "Engine.DXEngineClient", "MinDesiredFrameRate", "0.007813" },   /* a seventh decimal of 5 rounds up */
        { "Engine.DXEngineAudioDevice", "UseDirectSound", "True" },
        { "Engine.DXEngineAudioDevice", "OutputRate", "22050Hz" },
        { "Engine.DXEngineAudioDevice", "MusicVolume", "44" },
        { "Engine.DXEngineAudioDevice", "Latency", "10" },
        { "Engine.DXEngineAudioDevice", "AmbientFactor", "0.000000" },
        { "Engine.DXEngineAudioDevice", "EffectsChannels", "24" },
        { "Engine.DXEngineRenderDevice", "UseMipmapping", "True" },
        { "Engine.DXEngineRenderDevice", "UseVSync", "False" },
        { "Engine.DXEngineRenderDevice", "Description", "Direct3D" },
        { "Engine.DXEngineRenderDevice", "DescFlags", "0" },
    };
    for (size_t i = 0; i < sizeof want / sizeof *want; i++) {
        const char *v = dxl_ini_get(ini, want[i][0], want[i][1]);
        if (!v || strcmp(v, want[i][2]) != 0) fprintf(stderr, "  [%s] %s\n", want[i][0], want[i][1]);
        CHECK_STR(v ? v : "(none)", want[i][2]);
    }
    CHECK(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "UseSpatial") == NULL);
    CHECK(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "Translucency") == NULL);
    CHECK(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "FastTranslucency") == NULL);   /* SoftDrv's */
    dxl_ini_free(ini);

    /* SoftDrv's own, from its section, and none of D3DDrv's. */
    src = "[Engine.Engine]\nGameRenderDevice=softdrv.softwarerenderdevice\n"
          "[SoftDrv.SoftwareRenderDevice]\nFastTranslucency=True\nDetailBias=1.5\nShinySurfaces=False\n";
    ini = dxl_ini_parse(src, strlen(src));
    CHECK_INT(dxl_engine_sections_seed(ini), 57);
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "FastTranslucency"), "True");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "DetailBias"), "1.500000");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "ShinySurfaces"), "False");
    CHECK(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "UseMipmapping") == NULL);
    dxl_ini_free(ini);

    /* Galaxy's names alone: a rate without its Hz, or none of the seven. */
    CHECK_INT(dxl_engine_rate("44100"), 0);
    CHECK_INT(dxl_engine_rate("12345Hz"), 0);
    CHECK_INT(dxl_engine_rate(" 22050Hz"), 0);
    CHECK_INT(dxl_engine_rate("48000HZ trailing"), 48000);
}

/* After: the engine reads its own already, and only the keys missing take
 * its defaults, which it uses for them anyway. */
static void test_engine_read(void) {
    const char *src = "[WinDrv.WindowsClient]\nWindowedViewportX=640\nNoLighting=True\n"
                      "[Engine.DXEngineAudioDevice]\nMusicVolume=178.500000\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_engine_sections_seed(ini);
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "WindowedViewportX"), "1920");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineClient", "NoLighting"), "False");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "MusicVolume"), "178.500000");   /* left as it is */
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineAudioDevice", "SoundVolume"), "200");
    CHECK_STR(dxl_ini_get(ini, "Engine.DXEngineRenderDevice", "UsePrecache"), "False");
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    RUN(test_from_retail);
    RUN(test_as_the_engine_reads);
    RUN(test_engine_read);
TEST_MAIN_END
