#include "ui/audio.h"
#include "core/common.h"
#include "core/log.h"

#include <SDL3/SDL.h>
#ifdef DXL_XMP_LITE
#include <libxmp-lite/xmp.h>
#else
#include <xmp.h>
#endif

#include <stdlib.h>
#include <string.h>

#define RATE   44100
#define VOICES 6

const char *const dxl_sound_names[DXL_SOUND_COUNT] = {
    "Menu_Focus", "Menu_Press", "Menu_OK", "Menu_Cancel", "Menu_Slider", "Menu_Activate",
};

struct dxl_audio {
    SDL_AudioDeviceID device;
    SDL_AudioSpec     spec;            /* the streams': 16-bit stereo at RATE */
    SDL_AudioStream  *music;           /* fed by music_more on SDL's audio thread */
    SDL_AudioStream  *voice[VOICES];
    int               next_voice;
    /* Under the music stream's lock: the module and its fade. */
    xmp_context       xmp;
    int               loaded;          /* a module in it, its player started */
    int               playing;         /* and sounding: not faded out */
    long              fade_left, fade_total;   /* frames; 0 for no fade */
    long              frames;                  /* the music's, played */
    /* Each sound as the streams take it. */
    Uint8            *pcm[DXL_SOUND_COUNT];
    int               pcm_len[DXL_SOUND_COUNT];
};

/* SDL asks for more of the music: the module's next frames, faded. */
static void SDLCALL music_more(void *ctx, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    dxl_audio *a = ctx;
    if (!a->playing || additional <= 0) return;
    Sint16 buf[4096];
    while (additional > 0 && a->playing) {
        int n = additional < (int)sizeof buf ? additional : (int)sizeof buf;
        n &= ~3;
        if (n <= 0) break;
        if (xmp_play_buffer(a->xmp, buf, n, 0) != 0) {
            a->playing = 0;
            break;
        }
        if (a->fade_total > 0) {
            int frames = n / 4;
            for (int i = 0; i < frames; i++) {
                float g = a->fade_left > 0 ? (float)a->fade_left / (float)a->fade_total : 0;
                buf[2 * i] = (Sint16)(buf[2 * i] * g);
                buf[2 * i + 1] = (Sint16)(buf[2 * i + 1] * g);
                if (a->fade_left > 0) a->fade_left--;
            }
            if (a->fade_left <= 0) a->playing = 0;
        }
        SDL_PutAudioStreamData(stream, buf, n);
        a->frames += n / 4;
        additional -= n;
    }
}

dxl_audio *dxl_audio_open(void) {
    dxl_audio *a = dxl_xmalloc(sizeof *a);
    memset(a, 0, sizeof *a);
    a->spec = (SDL_AudioSpec){ SDL_AUDIO_S16, 2, RATE };
    a->device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &a->spec);
    if (!a->device) {
        dxl_log("no sound: %s", SDL_GetError());
        free(a);
        return NULL;
    }
    a->music = SDL_CreateAudioStream(&a->spec, &a->spec);
    if (a->music) {
        SDL_SetAudioStreamGetCallback(a->music, music_more, a);
        SDL_BindAudioStream(a->device, a->music);
    }
    for (int i = 0; i < VOICES; i++) {
        a->voice[i] = SDL_CreateAudioStream(&a->spec, &a->spec);
        if (a->voice[i]) SDL_BindAudioStream(a->device, a->voice[i]);
    }
    a->xmp = xmp_create_context();
    return a;
}

void dxl_audio_close(dxl_audio *a) {
    if (!a) return;
    /* The streams first: no callback runs after. */
    if (a->music) SDL_DestroyAudioStream(a->music);
    for (int i = 0; i < VOICES; i++)
        if (a->voice[i]) SDL_DestroyAudioStream(a->voice[i]);
    SDL_CloseAudioDevice(a->device);
    if (a->xmp) {
        if (a->loaded) {
            xmp_end_player(a->xmp);
            xmp_release_module(a->xmp);
        }
        xmp_free_context(a->xmp);
    }
    for (int i = 0; i < DXL_SOUND_COUNT; i++) SDL_free(a->pcm[i]);
    free(a);
}

int dxl_audio_music(dxl_audio *a, const void *module, size_t len) {
    if (!a || !a->music || !a->xmp) return -1;
    SDL_LockAudioStream(a->music);
    if (a->loaded) {
        xmp_end_player(a->xmp);
        xmp_release_module(a->xmp);
        a->loaded = a->playing = 0;
    }
    a->fade_left = a->fade_total = 0;
    int rc = 0;
    if (module) {
        /* libxmp reads the bytes as it loads; its copy is its own. */
        if (xmp_load_module_from_memory(a->xmp, module, (long)len) != 0) {
            rc = -1;
        } else if (xmp_start_player(a->xmp, RATE, 0) != 0) {
            xmp_release_module(a->xmp);
            rc = -1;
        } else {
            a->loaded = a->playing = 1;
        }
    }
    SDL_ClearAudioStream(a->music);
    SDL_UnlockAudioStream(a->music);
    return rc;
}

void dxl_audio_fade(dxl_audio *a, float seconds) {
    if (!a || !a->music) return;
    SDL_LockAudioStream(a->music);
    if (a->playing) a->fade_left = a->fade_total = (long)(seconds * RATE) + 1;
    SDL_UnlockAudioStream(a->music);
}

int dxl_audio_sound(dxl_audio *a, dxl_sound which, const void *wav, size_t len) {
    if (!a || which < 0 || which >= DXL_SOUND_COUNT) return -1;
    if (!wav) {
        SDL_free(a->pcm[which]);
        a->pcm[which] = NULL;
        a->pcm_len[which] = 0;
        return 0;
    }
    SDL_AudioSpec spec;
    Uint8 *buf = NULL, *out = NULL;
    Uint32 blen = 0;
    int olen = 0;
    if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(wav, len), true, &spec, &buf, &blen)) return -1;
    int ok = SDL_ConvertAudioSamples(&spec, buf, (int)blen, &a->spec, &out, &olen);
    SDL_free(buf);
    if (!ok) return -1;
    SDL_free(a->pcm[which]);
    a->pcm[which] = out;
    a->pcm_len[which] = olen;
    return 0;
}

void dxl_audio_play(dxl_audio *a, dxl_sound which) {
    if (!a || which < 0 || which >= DXL_SOUND_COUNT || !a->pcm[which]) return;
    SDL_AudioStream *v = a->voice[a->next_voice];
    a->next_voice = (a->next_voice + 1) % VOICES;
    if (!v) return;
    SDL_ClearAudioStream(v);
    SDL_PutAudioStreamData(v, a->pcm[which], a->pcm_len[which]);
}

long dxl_audio_music_frames(dxl_audio *a) {
    if (!a || !a->music) return 0;
    SDL_LockAudioStream(a->music);
    long n = a->frames;
    SDL_UnlockAudioStream(a->music);
    return n;
}

int dxl_audio_music_playing(dxl_audio *a) {
    if (!a || !a->music) return 0;
    SDL_LockAudioStream(a->music);
    int p = a->playing;
    SDL_UnlockAudioStream(a->music);
    return p;
}

void dxl_audio_volumes(dxl_audio *a, float music, float sounds) {
    if (!a) return;
    if (music < 0) music = 0;
    if (music > 1) music = 1;
    if (sounds < 0) sounds = 0;
    if (sounds > 1) sounds = 1;
    if (a->music) SDL_SetAudioStreamGain(a->music, music);
    for (int i = 0; i < VOICES; i++)
        if (a->voice[i]) SDL_SetAudioStreamGain(a->voice[i], sounds);
}
