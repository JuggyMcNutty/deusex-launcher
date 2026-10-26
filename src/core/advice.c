#include "core/advice.h"
#include "platform/os.h"

#include <string.h>

/* The engine's messages, as its error line begins. */
static const struct {
    const char *starts;
    dxl_advice  advice;
} known[] = {
    { "Failed to initialize OpenAL",
      { "The engine found no sound device it could open: OpenAL Soft reaches the desktop's sound through "
        "PulseAudio or PipeWire, and without either it tries ALSA, which a sound server holds. Play once "
        "without sound, or start one of them.",
        DXL_FIX_NOSOUND } },
    { "Could not find package",
      { "A package the game needs is not in its folder: the install is incomplete. Check it, or show the "
        "launcher another copy.",
        DXL_FIX_INSTALL } },
    { "Could not open DXEngine.pk3",
      { "The engine's own files are not beside it in the launcher's folder: stage or install DXLauncher "
        "again.",
        DXL_FIX_APP } },
};

/* The engine's own way round a renderer that did not start: its software
 * one, a clean run the slower for it. */
static const dxl_advice fell_back = {
    "The renderer chosen did not start here, so the game drew with the software renderer, which needs no "
    "GPU driver. Choose it, or another renderer, here or on Graphics.",
    DXL_FIX_RENDERER,
};

static const dxl_advice save_failed = {
    "The save it started on did not load. Another of the profile's saves may, or Play starts the game itself.",
    DXL_FIX_SAVES,
};

static const dxl_advice crashed = {
    "The engine crashed. If it does so again at the same place, try another renderer, or a run without "
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

static const dxl_advice stopped_by_launcher = {
    "The launcher stopped the game -- its window closed, a signal, a logout -- before the game had quit: "
    "nothing went wrong in the game, but its settings and the last save may not have been written.",
    DXL_FIX_NONE,
};

const dxl_advice *dxl_advice_for(const dxl_lastrun *l) {
    if (!l || !l->known) return NULL;
    if (l->clean) return l->fallback[0] ? &fell_back : NULL;
    for (size_t i = 0; i < sizeof known / sizeof *known; i++)
        if (strncmp(l->error, known[i].starts, strlen(known[i].starts)) == 0) return &known[i].advice;
    if (dxl_end_crashed(l->signal)) return &crashed;
    if (l->stopped || l->killed) return &stopped_by_launcher;
    if (l->status == 0 && !l->ready) return &no_game;
    /* Started on a save, stopped by an error the engine names before it was
     * up: the save's level is what did not load. */
    if (strstr(l->start, "loadgame") && !l->ready && l->error[0]) return &save_failed;
    if (l->error[0] || l->status != 0) return &stopped;
    return l->fallback[0] ? &fell_back : NULL;
}
