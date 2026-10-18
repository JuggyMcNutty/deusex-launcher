#include "core/engine_sections.h"
#include "core/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { BOOL, INT, FLOAT, HZ, TEXT };

#define CLIENT "Engine.DXEngineClient"
#define AUDIO  "Engine.DXEngineAudioDevice"
#define RENDER "Engine.DXEngineRenderDevice"

/* The engine's keys in the order its SaveConfig writes them, and its
 * built-in defaults (UDXEngineClient.h, UDXEngineAudioDevice.h,
 * UDXEngineRenderDevice.h). */
static const struct {
    const char *section, *key;
    int type;
    const char *def;
} keys[] = {
    { CLIENT, "StartupFullscreen", BOOL, "False" },
    { CLIENT, "WindowedViewportX", INT, "1920" },
    { CLIENT, "WindowedViewportY", INT, "1080" },
    { CLIENT, "WindowedColorBits", INT, "32" },
    { CLIENT, "FullscreenViewportX", INT, "0" },
    { CLIENT, "FullscreenViewportY", INT, "0" },
    { CLIENT, "FullscreenColorBits", INT, "32" },
    { CLIENT, "Brightness", FLOAT, "0.500000" },
    { CLIENT, "UseJoystick", BOOL, "False" },
    { CLIENT, "UseDirectInput", BOOL, "True" },
    { CLIENT, "MinDesiredFrameRate", INT, "200" },
    { CLIENT, "ScreenFlashes", BOOL, "True" },
    { CLIENT, "Decals", BOOL, "True" },
    { CLIENT, "NoDynamicLights", BOOL, "False" },
    { CLIENT, "TextureDetail", TEXT, "High" },
    { CLIENT, "SkinDetail", TEXT, "High" },
    { AUDIO, "UseFilter", BOOL, "True" },
    { AUDIO, "UseSurround", BOOL, "True" },
    { AUDIO, "UseStereo", BOOL, "True" },
    { AUDIO, "UseCDMusic", BOOL, "False" },
    { AUDIO, "UseDigitalMusic", BOOL, "True" },
    { AUDIO, "UseSpatial", BOOL, "True" },
    { AUDIO, "UseReverb", BOOL, "True" },
    { AUDIO, "Use3dHardware", BOOL, "True" },
    { AUDIO, "LowSoundQuality", BOOL, "False" },
    { AUDIO, "ReverseStereo", BOOL, "False" },
    { AUDIO, "Latency", INT, "40" },
    { AUDIO, "OutputRate", HZ, "44100Hz" },
    { AUDIO, "EffectsChannels", INT, "16" },
    { AUDIO, "MusicVolume", INT, "160" },
    { AUDIO, "SoundVolume", INT, "200" },
    { AUDIO, "SpeechVolume", INT, "255" },
    { AUDIO, "AmbientFactor", FLOAT, "0.700000" },
    { AUDIO, "DopplerSpeed", FLOAT, "6500.000000" },
    { RENDER, "Translucency", BOOL, "True" },
    { RENDER, "VolumetricLighting", BOOL, "True" },
    { RENDER, "ShinySurfaces", BOOL, "True" },
    { RENDER, "Coronas", BOOL, "True" },
    { RENDER, "HighDetailActors", BOOL, "True" },
    { RENDER, "DetailTextures", BOOL, "True" },
};

/* A retail value as the engine would write it back, or 0 when the engine
 * would not read it. */
static int as_written(int type, const char *v, char *out, size_t size) {
    char *end;
    switch (type) {
    case BOOL:
        if (!strcmp(v, "True") || !strcmp(v, "true") || !strcmp(v, "1")) snprintf(out, size, "True");
        else if (!strcmp(v, "False") || !strcmp(v, "false") || !strcmp(v, "0")) snprintf(out, size, "False");
        else return 0;
        return 1;
    case INT: {
        long n = strtol(v, &end, 10);
        if (end == v) return 0;
        snprintf(out, size, "%ld", n);
        return 1;
    }
    case FLOAT: {
        double n = strtod(v, &end);
        if (end == v) return 0;
        snprintf(out, size, "%f", n);
        return 1;
    }
    case HZ: {
        const char *hz = strstr(v, "Hz");
        if (!hz) return 0;
        long n = strtol(v, &end, 10);
        if (end == v) return 0;
        snprintf(out, size, "%ldHz", n);
        return 1;
    }
    default:
        snprintf(out, size, "%s", v);
        return 1;
    }
}

int dxl_engine_sections_seed(dxl_ini *system) {
    int engine = dxl_ini_pairs(system, AUDIO, NULL, NULL, 0) > 0, added = 0;
    const char *device = dxl_ini_get(system, "Engine.Engine", "GameRenderDevice");
    char render[128];
    snprintf(render, sizeof render, "%s", device && *device ? device : "D3DDrv.D3DRenderDevice");
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
        if (dxl_ini_get(system, keys[i].section, keys[i].key)) continue;
        char v[256];
        const char *retail = !strcmp(keys[i].section, CLIENT) ? "WinDrv.WindowsClient"
                           : !strcmp(keys[i].section, AUDIO) ? "Galaxy.GalaxyAudioSubsystem" : render;
        const char *was = engine ? NULL : dxl_ini_get(system, retail, keys[i].key);
        if (!was || !as_written(keys[i].type, was, v, sizeof v)) snprintf(v, sizeof v, "%s", keys[i].def);
        dxl_ini_set(system, keys[i].section, keys[i].key, v);
        added++;
    }
    if (!dxl_ini_get(system, "Engine.DXEngineWindowSystem", "WindowSystem")) {
        dxl_ini_set(system, "Engine.DXEngineWindowSystem", "WindowSystem", "SDL2");
        added++;
    }
    return added;
}
