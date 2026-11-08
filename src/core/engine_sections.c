#include "core/engine_sections.h"
#include "core/common.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* How the engine reads a key's text (DXEngine's Package/IniProperty.cpp and
 * the U*Device classes' LoadProperties), each written back as it writes
 * its type. */
enum {
    BOOL,       /* the first token, 1 or True, 0 or False in any case (dxl_class_bool) */
    INT,        /* as std::stoi: blanks, a sign, digits, the rest passed over, an int's range */
    BYTE,       /* the same, cut to a byte: 300 is 44 */
    LATENCY,    /* the same, held to 10 to 250 ms, as Galaxy holds it */
    FLOAT,      /* any text by strtod, an empty one 0, written with six decimals */
    RATE,       /* one of Galaxy's seven names (dxl_engine_rate) */
    DETAIL,     /* the first token one of High, Medium and Low in any case; "Ultra Low", two
                 * words, no text reaches */
    TEXT        /* as it is */
};

#define CLIENT "Engine.DXEngineClient"
#define AUDIO  "Engine.DXEngineAudioDevice"
#define RENDER "Engine.DXEngineRenderDevice"

/* Whose: every render device's, or D3DDrv's or SoftDrv's alone, as
 * [Engine.Engine] GameRenderDevice names it. */
enum { ANY, D3D, SOFT };

/* The engine's keys in the order its SaveConfig writes them, and its
 * built-in defaults (UDXEngineClient.h, UDXEngineAudioDevice.h,
 * UDXEngineRenderDevice.h); after each class's own, the original's config
 * it keeps and has no use for, each its type's nothing until an ini names
 * it (UDXEngineClient::DefineExtra, the devices' LoadProperties). */
static const struct {
    const char *section, *key;
    int type;
    const char *def;
    int device;
} keys[] = {
    { CLIENT, "StartupFullscreen", BOOL, "False", ANY },
    { CLIENT, "WindowedViewportX", INT, "1920", ANY },
    { CLIENT, "WindowedViewportY", INT, "1080", ANY },
    { CLIENT, "WindowedColorBits", INT, "32", ANY },
    { CLIENT, "FullscreenViewportX", INT, "0", ANY },
    { CLIENT, "FullscreenViewportY", INT, "0", ANY },
    { CLIENT, "FullscreenColorBits", INT, "32", ANY },
    { CLIENT, "Brightness", FLOAT, "0.500000", ANY },
    { CLIENT, "UseJoystick", BOOL, "False", ANY },
    { CLIENT, "UseDirectInput", BOOL, "True", ANY },
    { CLIENT, "MinDesiredFrameRate", FLOAT, "200.000000", ANY },
    { CLIENT, "ScreenFlashes", BOOL, "True", ANY },
    { CLIENT, "Decals", BOOL, "True", ANY },
    { CLIENT, "NoDynamicLights", BOOL, "False", ANY },
    { CLIENT, "NoLighting", BOOL, "False", ANY },
    { CLIENT, "TextureDetail", DETAIL, "High", ANY },
    { CLIENT, "SkinDetail", DETAIL, "High", ANY },
    { CLIENT, "MipFactor", FLOAT, "0.000000", ANY },
    { CLIENT, "CaptureMouse", BOOL, "False", ANY },
    { CLIENT, "CurvedSurfaces", BOOL, "False", ANY },
    { CLIENT, "UseDirectDraw", BOOL, "False", ANY },
    { CLIENT, "SlowVideoBuffering", BOOL, "False", ANY },
    { CLIENT, "DeadZoneXYZ", BOOL, "False", ANY },
    { CLIENT, "DeadZoneRUV", BOOL, "False", ANY },
    { CLIENT, "InvertVertical", BOOL, "False", ANY },
    { CLIENT, "ScaleXYZ", FLOAT, "0.000000", ANY },
    { CLIENT, "ScaleRUV", FLOAT, "0.000000", ANY },
    { AUDIO, "UseFilter", BOOL, "True", ANY },
    { AUDIO, "UseSurround", BOOL, "True", ANY },
    { AUDIO, "UseStereo", BOOL, "True", ANY },
    { AUDIO, "UseCDMusic", BOOL, "False", ANY },
    { AUDIO, "UseDigitalMusic", BOOL, "True", ANY },
    { AUDIO, "UseReverb", BOOL, "True", ANY },
    { AUDIO, "Use3dHardware", BOOL, "True", ANY },
    { AUDIO, "LowSoundQuality", BOOL, "False", ANY },
    { AUDIO, "ReverseStereo", BOOL, "False", ANY },
    { AUDIO, "Latency", LATENCY, "40", ANY },
    { AUDIO, "OutputRate", RATE, "44100Hz", ANY },
    { AUDIO, "EffectsChannels", INT, "16", ANY },
    { AUDIO, "MusicVolume", BYTE, "160", ANY },
    { AUDIO, "SoundVolume", BYTE, "200", ANY },
    { AUDIO, "SpeechVolume", BYTE, "255", ANY },
    { AUDIO, "AmbientFactor", FLOAT, "0.700000", ANY },
    { AUDIO, "DopplerSpeed", FLOAT, "6500.000000", ANY },
    { AUDIO, "UseDirectSound", BOOL, "False", ANY },
    { RENDER, "VolumetricLighting", BOOL, "True", ANY },
    { RENDER, "ShinySurfaces", BOOL, "True", ANY },
    { RENDER, "Coronas", BOOL, "True", ANY },
    { RENDER, "HighDetailActors", BOOL, "True", ANY },
    { RENDER, "DetailTextures", BOOL, "True", ANY },
    { RENDER, "UsePrecache", BOOL, "False", ANY },
    { RENDER, "Description", TEXT, "", ANY },
    { RENDER, "DescFlags", INT, "0", ANY },
    { RENDER, "UseMipmapping", BOOL, "False", D3D },
    { RENDER, "UseTrilinear", BOOL, "False", D3D },
    { RENDER, "UseMultitexture", BOOL, "False", D3D },
    { RENDER, "UsePalettes", BOOL, "False", D3D },
    { RENDER, "UseGammaCorrection", BOOL, "False", D3D },
    { RENDER, "Use3dfx", BOOL, "False", D3D },
    { RENDER, "UseTripleBuffering", BOOL, "False", D3D },
    { RENDER, "UseVSync", BOOL, "False", D3D },
    { RENDER, "UseVideoMemoryVB", BOOL, "False", D3D },
    { RENDER, "UseAGPTextures", BOOL, "False", D3D },
    { RENDER, "UseVertexFog", BOOL, "False", D3D },
    { RENDER, "dwVendorId", INT, "0", D3D },
    { RENDER, "dwDeviceId", INT, "0", D3D },
    { RENDER, "HighResTextureSmooth", BOOL, "False", SOFT },
    { RENDER, "LowResTextureSmooth", BOOL, "False", SOFT },
    { RENDER, "FastTranslucency", BOOL, "False", SOFT },
    { RENDER, "DetailBias", FLOAT, "0.000000", SOFT },
};

int dxl_engine_rate(const char *v) {
    static const int rates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000 };
    char t[16], name[16];
    if (!dxl_first_token(v, t, sizeof t)) return 0;
    for (size_t i = 0; i < sizeof rates / sizeof *rates; i++) {
        snprintf(name, sizeof name, "%dHz", rates[i]);
        if (!dxl_stricmp(t, name)) return rates[i];
    }
    return 0;
}

/* A float as the engine writes one (StrTools::float_text): six decimals, a
 * seventh of 5 or more rounding up, as the original's runtime prints. */
static void float_text(double value, char *out, size_t size) {
    if (isnan(value)) {
        snprintf(out, size, "%s", signbit(value) ? "-1.#IND00" : "1.#QNAN0");
        return;
    }
    if (isinf(value)) {
        snprintf(out, size, "%s", value < 0 ? "-1.#INF00" : "1.#INF00");
        return;
    }
    char digits[400];
    snprintf(digits, sizeof digits, "%.40f", fabs(value));
    char *point = strchr(digits, '.');
    int up = point[7] >= '5';
    /* The whole part and six decimals, one run of digits, rounded. */
    memmove(point, point + 1, 6);
    point[6] = '\0';
    size_t n = strlen(digits);
    if (up) {
        size_t i = n;
        while (i > 0 && digits[i - 1] == '9') digits[--i] = '0';
        if (i > 0) {
            digits[i - 1]++;
        } else {
            memmove(digits + 1, digits, n + 1);
            digits[0] = '1';
            n++;
        }
    }
    snprintf(out, size, "%s%.*s.%s", signbit(value) ? "-" : "", (int)(n - 6), digits, digits + n - 6);
}

/* A whole number as std::stoi reads one; 0 when it reads none. */
static int stoi_of(const char *v, long *out) {
    char *end;
    errno = 0;
    long n = strtol(v, &end, 10);
    if (end == v || errno == ERANGE || n < INT_MIN || n > INT_MAX) return 0;
    *out = n;
    return 1;
}

/* A retail value as the engine reads it, written as the engine would write
 * it back, or 0 when the engine would keep its default. */
static int as_read(int type, const char *v, char *out, size_t size) {
    static const char *const details[] = { "High", "Medium", "Low" };
    long n;
    char t[16];
    switch (type) {
    case BOOL: {
        int b = dxl_class_bool(v);
        if (b < 0) return 0;
        snprintf(out, size, "%s", b ? "True" : "False");
        return 1;
    }
    case INT:
    case BYTE:
    case LATENCY:
        if (!stoi_of(v, &n)) return 0;
        if (type == BYTE) n = (unsigned char)n;
        if (type == LATENCY) n = n < 10 ? 10 : n > 250 ? 250 : n;
        snprintf(out, size, "%ld", n);
        return 1;
    case FLOAT:
        float_text((float)strtod(v, NULL), out, size);
        return 1;
    case RATE:
        if (!(n = dxl_engine_rate(v))) return 0;
        snprintf(out, size, "%ldHz", n);
        return 1;
    case DETAIL:
        if (!dxl_first_token(v, t, sizeof t)) return 0;
        for (size_t i = 0; i < sizeof details / sizeof *details; i++)
            if (!dxl_stricmp(t, details[i])) {
                snprintf(out, size, "%s", details[i]);
                return 1;
            }
        return 0;
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
    int whose = !dxl_stricmp(render, "D3DDrv.D3DRenderDevice") ? D3D : !dxl_stricmp(render, "SoftDrv.SoftwareRenderDevice") ? SOFT : ANY;
    for (size_t i = 0; i < sizeof keys / sizeof *keys; i++) {
        if (keys[i].device != ANY && keys[i].device != whose) continue;
        if (dxl_ini_get(system, keys[i].section, keys[i].key)) continue;
        char v[256];
        const char *retail = !strcmp(keys[i].section, CLIENT) ? "WinDrv.WindowsClient"
                           : !strcmp(keys[i].section, AUDIO) ? "Galaxy.GalaxyAudioSubsystem" : render;
        const char *was = engine ? NULL : dxl_ini_get(system, retail, keys[i].key);
        if (!was || !as_read(keys[i].type, was, v, sizeof v)) snprintf(v, sizeof v, "%s", keys[i].def);
        dxl_ini_set(system, keys[i].section, keys[i].key, v);
        added++;
    }
    return added;
}
