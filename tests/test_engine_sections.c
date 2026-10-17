#include "test.h"
#include "core/engine_sections.h"

/* Before the engine's sections: each from its retail key, as the engine
 * writes it; a retail key it would not read gives way to its default. */
static void test_from_retail(void) {
    const char *src = "[Engine.Engine]\nGameRenderDevice=D3DDrv.D3DRenderDevice\n"
                      "[WinDrv.WindowsClient]\nStartupFullscreen=True\nWindowedViewportX=640\nWindowedViewportY=480\n"
                      "Brightness=0.6\nMinDesiredFrameRate=1.000000\nDecals=Yes\n"
                      "[Galaxy.GalaxyAudioSubsystem]\nMusicVolume=153\nOutputRate=22050Hz\nUseSurround=False\n"
                      "[D3DDrv.D3DRenderDevice]\nDetailTextures=False\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    CHECK_INT(dxl_engine_sections_seed(ini), 41);
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "StartupFullscreen"), "True");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "WindowedViewportX"), "640");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "Brightness"), "0.600000");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "MinDesiredFrameRate"), "1");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "Decals"), "True");          /* Yes: the default */
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "TextureDetail"), "High");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "MusicVolume"), "153");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "OutputRate"), "22050Hz");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "UseSurround"), "False");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "SpeechVolume"), "255");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealRenderDevice", "DetailTextures"), "False");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealWindowSystem", "WindowSystem"), "SDL2");
    /* Seeded, again: nothing more. */
    CHECK_INT(dxl_engine_sections_seed(ini), 0);
    dxl_ini_free(ini);
}

/* After: the engine reads its own already, and only the keys missing take
 * its defaults, which it uses for them anyway. */
static void test_engine_read(void) {
    const char *src = "[WinDrv.WindowsClient]\nWindowedViewportX=640\n"
                      "[Engine.SurrealAudioDevice]\nMusicVolume=178.500000\n";
    dxl_ini *ini = dxl_ini_parse(src, strlen(src));
    dxl_engine_sections_seed(ini);
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealClient", "WindowedViewportX"), "1920");
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "MusicVolume"), "178.500000");   /* left as it is */
    CHECK_STR(dxl_ini_get(ini, "Engine.SurrealAudioDevice", "SoundVolume"), "200");
    dxl_ini_free(ini);
}

TEST_MAIN_BEGIN
    RUN(test_from_retail);
    RUN(test_engine_read);
TEST_MAIN_END
