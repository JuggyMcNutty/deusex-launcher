#include "core/advice.h"
#include "platform/os.h"

#include <string.h>

/* The engine's messages, as its error line begins. */
static const struct {
    const char *starts;
    dxl_advice  advice;
    int         path;   /* 1: only with a path after it, the GPU's "Could not ..." no file's */
} known[] = {
    { "Failed to initialize OpenAL",
      { "The engine found no sound device it could open: OpenAL Soft reaches the desktop's sound through "
        "PulseAudio or PipeWire, and without either it tries ALSA, which a sound server holds. Play once "
        "without sound, or start one of them.",
        DXL_FIX_NOSOUND }, 0 },
    { "No UE1 game found",
      { "The engine did not take the game's folder for Deus Ex. Check the install, or show the launcher "
        "another copy.",
        DXL_FIX_INSTALL }, 0 },
    { "Could not find package",
      { "A package the game needs is not in its folder: the install is incomplete. Check it, or show the "
        "launcher another copy.",
        DXL_FIX_INSTALL }, 0 },
    { "Script execution error: Unknown native function",
      { "The game called native code the engine does not have. A mod with native code of its own -- a DLL of "
        "its own beside its packages -- does not play on DXEngine; the game itself, and a mod without, do.",
        DXL_FIX_NONE }, 0 },
    { "Could not open DXEngine.pk3",
      { "The engine's own files are not beside it in the launcher's folder: stage or install DXLauncher "
        "again.",
        DXL_FIX_APP }, 0 },
    /* Utils/File.cpp's, a file or a folder by its path. */
    { "Could not open ",
      { "The engine could not open a file it needs, named in its error: one that is gone, or that another "
        "program holds. Check the place it names.",
        DXL_FIX_NONE }, 1 },
    { "Could not create",
      { "The engine could not write a file or make a folder it needs, named in its error: a folder that is "
        "not yours to write, or a full disk. Check the place it names.",
        DXL_FIX_NONE }, 1 },
};

/* The advice for an error the engine names, by how its text begins; NULL
 * for one not known. */
static const dxl_advice *known_for(const char *text) {
    for (size_t i = 0; i < sizeof known / sizeof *known; i++) {
        size_t n = strlen(known[i].starts);
        if (strncmp(text, known[i].starts, n) != 0) continue;
        if (known[i].path && !strpbrk(text + n, "/\\")) continue;
        return &known[i].advice;
    }
    return NULL;
}

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

static const dxl_advice stopped = {
    "The engine stopped with an error: the end of its log says what it was doing.",
    DXL_FIX_NONE,
};

static const dxl_advice stopped_by_launcher = {
    "The launcher stopped the game -- its window closed, a signal, a logout -- before the game had quit: "
    "nothing went wrong in the game, but its settings and the last save may not have been written.",
    DXL_FIX_NONE,
};

/* SIGKILL, the launcher not stopping it: a kill from outside, which the
 * game could not answer, its log silent on it. */
static const dxl_advice killed_outside = {
    "The game was killed from outside, a kill it could not answer: by the system when memory ran out, or by "
    "a Force Quit or a kill. Its log cannot say why; its settings, and the game since its last save, were not "
    "written.",
    DXL_FIX_NONE,
};

const dxl_advice *dxl_advice_for(const dxl_lastrun *l) {
    if (!l || !l->known) return NULL;
    /* The audio device did not start and the game played on silent, a
     * clean run too: the advice of the error it names (OpenAL's). */
    const dxl_advice *silent = l->audio[0] ? known_for(l->audio) : NULL;
    if (l->clean) return l->fallback[0] ? &fell_back : silent;
    const dxl_advice *named = known_for(l->error);
    if (named) return named;
    if (dxl_end_crashed(l->signal)) return &crashed;
    if (l->stopped || l->killed) return &stopped_by_launcher;
    if (dxl_end_killed(l->signal)) return &killed_outside;
    /* Started on a save, stopped by an error the engine names before it was
     * up: the save's level is what did not load. */
    if (strstr(l->start, "loadgame") && !l->ready && l->error[0]) return &save_failed;
    if (l->error[0] || l->status != 0) return &stopped;
    return l->fallback[0] ? &fell_back : silent;
}
