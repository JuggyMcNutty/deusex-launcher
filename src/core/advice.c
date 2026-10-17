#include "core/advice.h"

#include <signal.h>
#include <string.h>

/* The engine's messages, as its error line begins. */
static const struct {
    const char *starts;
    dxl_advice  advice;
} known[] = {
    { "Could not create vulkan renderer",
      { "The Vulkan renderer did not start here. The OpenGL one may: choose it here or on Graphics.",
        DXL_FIX_RENDERER } },
    { "Failed to initialize OpenAL",
      { "The engine found no sound device it could open: OpenAL Soft reaches the desktop's sound through "
        "PulseAudio or PipeWire, and without either it tries ALSA, which a sound server holds. Play once "
        "without sound, or start one of them.",
        DXL_FIX_NOSOUND } },
    { "Could not find package",
      { "A package the game needs is not in its folder: the install is incomplete. Check it, or show the "
        "launcher another copy.",
        DXL_FIX_INSTALL } },
    { "Could not open SurrealEngine.pk3",
      { "The engine's own files are not beside it in the launcher's folder: stage or install DXLauncher "
        "again.",
        DXL_FIX_APP } },
};

static const dxl_advice crashed = {
    "The engine crashed. If it does so again at the same place, try the other renderer, or a run without "
    "sound to rule the audio out; the end of its log may say more.",
    DXL_FIX_RENDERER,
};

static const dxl_advice no_game = {
    "The engine did not take the game's folder for Deus Ex. Check the install, or show the launcher "
    "another copy.",
    DXL_FIX_INSTALL,
};

static const dxl_advice stopped = {
    "The engine stopped with an error: the end of its log says what it was doing.",
    DXL_FIX_NONE,
};

const dxl_advice *dxl_advice_for(const dxl_lastrun *l) {
    if (!l || !l->known || l->clean) return NULL;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (strncmp(l->error, known[i].starts, strlen(known[i].starts)) == 0) return &known[i].advice;
    if (l->signal == SIGSEGV || l->signal == SIGBUS || l->signal == SIGABRT || l->signal == SIGILL ||
        l->signal == SIGFPE)
        return &crashed;
    if (l->status == 0 && !l->ready) return &no_game;
    if (l->error[0] || l->status != 0) return &stopped;
    return NULL;
}
