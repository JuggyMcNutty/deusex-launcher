/* The launcher's own sound: the game's title music -- the IT module the
 * launcher takes out of Music/Title_Music.umx, played by libxmp from its
 * first order, looped, faded out as the game starts -- and the game's menu
 * sounds, its WAVs, on six voices. SDL's audio streams mix them on the
 * default output. Volumes from 0 to 1, the launcher's own (launcher.ini's
 * MusicVolume and SoundVolume). No output to open: silence.
 */
#ifndef DXL_UI_AUDIO_H
#define DXL_UI_AUDIO_H

#include <stddef.h>

typedef struct dxl_audio dxl_audio;

/* The game's six menu sounds, DeusExSounds.u's Menu_* by these names. */
typedef enum {
    DXL_SOUND_FOCUS,      /* Menu_Focus: the focus moved */
    DXL_SOUND_PRESS,      /* Menu_Press: a control chosen */
    DXL_SOUND_OK,         /* Menu_OK: play */
    DXL_SOUND_CANCEL,     /* Menu_Cancel: back */
    DXL_SOUND_SLIDER,     /* Menu_Slider: a value stepped */
    DXL_SOUND_ACTIVATE,   /* Menu_Activate: a dialog up */
    DXL_SOUND_COUNT
} dxl_sound;

extern const char *const dxl_sound_names[DXL_SOUND_COUNT];

/* The default output opened (SDL's audio subsystem up); NULL when none
 * opens. */
dxl_audio *dxl_audio_open(void);
void       dxl_audio_close(dxl_audio *a);

/* A module's bytes (copied) played from its first order, looped; 0, or -1
 * for bytes libxmp does not play. NULL stops the music. */
int  dxl_audio_music(dxl_audio *a, const void *module, size_t len);
/* The music faded to silence over seconds, then stopped. */
void dxl_audio_fade(dxl_audio *a, float seconds);
/* A sound's WAV bytes, kept for playing: 0, or -1 for none SDL reads. */
int  dxl_audio_sound(dxl_audio *a, dxl_sound which, const void *wav, size_t len);
/* Played once, on the voice played longest ago. */
void dxl_audio_play(dxl_audio *a, dxl_sound which);
void dxl_audio_volumes(dxl_audio *a, float music, float sounds);
/* The music's frames played so far, and whether it sounds still. */
long dxl_audio_music_frames(dxl_audio *a);
int  dxl_audio_music_playing(dxl_audio *a);

#endif
