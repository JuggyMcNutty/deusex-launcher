/* The window's sound on SDL's dummy output: a WAV the test makes taken
 * and played, bytes that are none refused; with an install in gamefiles/,
 * its title music played, and faded to its end. */
#include "test.h"
#include "core/paths.h"
#include "core/upkg_assets.h"
#include "ui/audio.h"

#include <SDL3/SDL.h>

#ifndef DXL_GAMEFILES
#define DXL_GAMEFILES "gamefiles/System"
#endif

/* A WAV: 8-bit mono at 22050 Hz, a short ramp. */
static size_t make_wav(uint8_t *out, size_t samples) {
    uint32_t data = (uint32_t)samples, riff = 36 + data;
    uint8_t h[44] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
                      0x22, 0x56, 0, 0, 0x22, 0x56, 0, 0, 1, 0, 8, 0, 'd', 'a', 't', 'a', 0, 0, 0, 0 };
    memcpy(h + 4, &riff, 4);
    memcpy(h + 40, &data, 4);
    memcpy(out, h, 44);
    for (size_t i = 0; i < samples; i++) out[44 + i] = (uint8_t)(128 + (i % 64) - 32);
    return 44 + samples;
}

static void test_sounds(void) {
    dxl_audio *a = dxl_audio_open();
    CHECK(a != NULL);
    if (!a) return;
    static uint8_t wav[44 + 2000];
    size_t n = make_wav(wav, 2000);
    CHECK_INT(dxl_audio_sound(a, DXL_SOUND_FOCUS, wav, n), 0);
    dxl_audio_play(a, DXL_SOUND_FOCUS);
    dxl_audio_play(a, DXL_SOUND_PRESS);   /* none kept for it: nothing */
    CHECK_INT(dxl_audio_sound(a, DXL_SOUND_OK, "not a wav at all", 16), -1);
    CHECK_INT(dxl_audio_music(a, "not a module", 12), -1);
    CHECK_INT(dxl_audio_music_playing(a), 0);
    dxl_audio_volumes(a, 2, -1);
    dxl_audio_close(a);
}

static void test_title_music(void) {
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    char *umx = dxl_path_join(root, "Music/Title_Music.umx");
    free(root);
    dxl_upkg p;
    dxl_err e;
    if (dxl_upkg_load(&p, umx, &e) != 0) {
        printf("    (no install: the title music not tried)\n");
        free(umx);
        return;
    }
    free(umx);
    size_t len;
    char type[16];
    uint8_t *it = dxl_upkg_file(&p, dxl_upkg_find(&p, "Music", "Title_Music"), &len, type, sizeof type, &e);
    dxl_upkg_free(&p);
    CHECK(it != NULL);
    dxl_audio *a = dxl_audio_open();
    CHECK(a != NULL);
    if (a && it) {
        CHECK_INT(dxl_audio_music(a, it, len), 0);
        SDL_Delay(300);
        long played = dxl_audio_music_frames(a);
        CHECK(played > 0);
        CHECK_INT(dxl_audio_music_playing(a), 1);
        dxl_audio_fade(a, 0.1f);
        SDL_Delay(500);
        CHECK_INT(dxl_audio_music_playing(a), 0);
        long after = dxl_audio_music_frames(a);
        SDL_Delay(200);
        CHECK(dxl_audio_music_frames(a) == after);
    }
    dxl_audio_close(a);
    free(it);
}

TEST_MAIN_BEGIN
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return 1;
    }
    RUN(test_sounds);
    RUN(test_title_music);
    SDL_Quit();
TEST_MAIN_END
