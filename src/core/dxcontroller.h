/* DXController, dsgls's mod for the pad (github.com/dsgls/DXController,
 * GPLv3+): its scripts, DXController.u, the author's own build, ship in the
 * app's data/dxcontroller beside a patch (core/patch.h) that makes the
 * game's DeusEx.u, on the player's machine from the player's own stock
 * copy, into the one the mod's release carries, with its hooks. A profile
 * plays with it while its option is on and it can be (dxl_dxc_able): the
 * start puts the two packages first in the profile's paths, names the
 * mod's console and root window, binds the pad as the mod's own launcher
 * does, and switches the engine's pad to the mod's mode (Settings.json's
 * Gamepad.DXController); with it off, at a multiplayer start, and after
 * every game, all of that is undone, the player's own as it was. What a
 * start changed is kept in profile.ini first, so a kill at any step leaves
 * a profile that starts and that the next start mends. docs/LAUNCHER.md,
 * DXController. */
#ifndef DXL_DXCONTROLLER_H
#define DXL_DXCONTROLLER_H

#include "core/common.h"
#include "core/config.h"
#include "core/ini.h"
#include "core/profile.h"

/* The word of an engine's --version whose pad takes the mod's mode. */
#define DXL_DXC_FEATURE  "dxcontroller-pad"
/* The app's data/<this>, and the launcher's data home's <this>/<made>. */
#define DXL_DXC_FOLDER   "dxcontroller"
#define DXL_DXC_PATCH    "DeusEx-v1.6.1.dxlpatch"
#define DXL_DXC_PACKAGE  "DXController.u"
#define DXL_DXC_CONSOLE  "DXController.ControllerConsole"
#define DXL_DXC_ROOT     "DXController.ControllerRootWindow"
/* profile.ini: the option ([Profile], on unless False), and a start's
 * failure to make the patched file, said on the Controller page. */
#define DXL_DXC_OPTION   "DXController"
#define DXL_DXC_PROBLEM  "DXControllerProblem"

typedef struct {
    dxl_config  *config;        /* the profile's DeusEx.ini and User.ini, open */
    dxl_profile *profile;
    const char  *app_data;      /* the app's data/ */
    const char  *data_home;     /* the launcher's data home, where the patched file is made */
    const char  *game_system;   /* the profile's install's System */
    int          engine_has;    /* the engine's --version names DXL_DXC_FEATURE */
} dxl_dxc_place;

/* The option: on unless the profile says otherwise. */
int dxl_dxc_option(const dxl_profile *p);

/* Whether it can be on for this profile: 1, or 0 with why ("A mod's
 * profile plays the mod alone." ...): a profile of the game itself, an
 * engine with the feature, the app's two files, the player's DeusEx.u the
 * one the patch is for. The player's file's SHA-1 is kept while its size
 * and time are. */
int dxl_dxc_able(const dxl_dxc_place *w, char *why, size_t size);

/* The folder the patched DeusEx.u is in, made (again) where it is missing
 * or not the patch's result: <data home>/dxcontroller/<source>-<result>,
 * each the first 12 digits of its SHA-1. Caller frees; NULL, with why. */
char *dxl_dxc_make(const dxl_dxc_place *w, dxl_err *err);

/* A start's: on (1) where wanted and able and the patched file made, the
 * changes kept in profile.ini (written at once) and made in the open inis
 * (the caller's dxl_config_save writes them, then dxl_dxc_pad); else
 * everything undone (dxl_dxc_rest) and 0, or -1 when it was wanted and able
 * and failed, why said and kept for the Controller page (DXL_DXC_PROBLEM).
 * multiplayer: a game that joins or hosts runs without it, 2. */
#define DXL_DXC_MULTIPLAYER 2
int dxl_dxc_start(const dxl_dxc_place *w, int multiplayer, char *why, size_t size);

/* After an on start's inis are saved: the engine's pad in the mod's mode
 * (Settings.json's Gamepad.DXController, written where it changes), saved
 * 1; or in its own where the save failed (saved 0), the game then playing
 * with the inis as they were. 0, or -1 with why. */
int dxl_dxc_pad(const dxl_dxc_place *w, int saved, dxl_err *err);

/* Everything a start changed undone, the inis written, then Settings.json,
 * then profile.ini forgetting it: 1 when something was, 0 when nothing
 * was to, -1 with why when a file could not be written (what it kept is
 * kept, for the next try). */
int dxl_dxc_rest(const dxl_dxc_place *w, dxl_err *err);

/* Whether a run's start joins or hosts a multiplayer game: the engine's
 * URL a server's address (the launcher's join), or the original's words a
 * dedicated server (-server), a listen server (?listen) or an address. */
int dxl_dxc_multiplayer(const char *url, const char *words);

/* The pad's bindings a start writes in [Engine.Input], as the mod's own
 * launcher has them: the key, and what it is bound to ("" for none: the
 * mod's console takes those itself). */
typedef struct {
    const char *key, *binding;
} dxl_dxc_binding;
size_t                 dxl_dxc_binding_count(void);
const dxl_dxc_binding *dxl_dxc_binding_at(size_t i);

#endif
