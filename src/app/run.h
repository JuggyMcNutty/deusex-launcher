/* One start of the launcher, from its command line to the game's end:
 *
 *   1. A second start hands its command line to the launcher that runs
 *      (platform/instance.h) and ends -- unless the line holds one of the
 *      original's four bypass words (Server, NewWindow, changevideo,
 *      TestRenDev).
 *   2. The folders, launcher.ini, the active profile (app/session.h). No
 *      profile yet: the first run makes one from an install (--game=<dir>,
 *      or the first run's screens). A profile whose install is not there
 *      to play: the window asks for another.
 *   3. The decision (core/policy.h): Home; Recovery after -safe or after a
 *      run that did not end cleanly, whose Running.ini survived; Graphics
 *      after -changevideo. --play and DXL_NO_HOME=1 skip Home, not the
 *      others.
 *   4. Before the game, after the screens, as the original does it: the
 *      settings saved, FirstRun raised to 1100, the crash sentinel made.
 *   5. The game (app/play.h). An engine whose --version lacks the userdir
 *      feature is refused, so nothing ever writes ~/.config/DXEngine.
 *   6. The end: a clean one takes the sentinel away and runs a RELAUNCH the
 *      engine passed on; one that is not leaves it, and the next start
 *      opens on Recovery.
 *
 * The launcher's own options start with "--"; every other word is the
 * original's command line, the player's, passed to the engine in
 * --cmdline. The screens are the caller's (dxl_run_ui): deusex-launcher's
 * window (screens/shell.h). With none -- dxl-cli -- Home is skipped, the
 * first run makes the profile from --game's install, and any other screen
 * that is due is a message and DXL_RUN_EXIT_SCREEN.
 */
#ifndef DXL_RUN_H
#define DXL_RUN_H

#include "app/play.h"
#include "app/session.h"
#include "core/config.h"
#include "core/engine_args.h"
#include "platform/instance.h"
#include "core/lastrun.h"
#include "core/policy.h"
#include "core/renderers.h"
#include "core/sentinel.h"

/* Exit codes besides the game's own status. */
#define DXL_RUN_EXIT_USAGE   2   /* an option the launcher does not know */
#define DXL_RUN_EXIT_SCREEN  3   /* a screen is due and there is no window */
#define DXL_RUN_EXIT_NOGAME  4   /* no install to play, or no engine to play it */

typedef enum {
    DXL_VIEW_NONE,
    DXL_VIEW_HOME,
    DXL_VIEW_FIRST_RUN,
    DXL_VIEW_INSTALL,     /* the profile's install is not there to play: another */
    DXL_VIEW_RECOVERY,
    DXL_VIEW_GRAPHICS
} dxl_view;

typedef struct {
    const char *game;       /* --game=<dir>: the install for a first run, or the profile's now */
    const char *mod;        /* --mod=<dir>: with --import, the profile a mod's (core/mods.h) */
    const char *profile;    /* --profile=<id> */
    int play;               /* --play: no Home */
    int dry;                /* --dry-run: decide and say, write nothing */
    int print_command;      /* --print-command: the engine's command line, nothing run */
    int import_only;        /* --import: make the profile from --game, nothing run */
    int big_screen;         /* --big-screen: the window on the big screen (core/bigscreen.h) */
    const char *join;       /* a deusex://host:port link: its server joined, Multiplayer asking first */
} dxl_run_options;

typedef struct {
    dxl_run_options opt;
    char           *words;      /* the original's command line */
    dxl_session     session;
    dxl_instance   *instance;   /* held to the end; NULL when another launcher holds it */
    dxl_config     *config;     /* the profile's DeusEx.ini and User.ini */
    dxl_sentinel    sentinel;
    dxl_decision    decision;
    dxl_view        view;
    dxl_lastrun     last;       /* the last run's end, for Recovery */
    dxl_play_result played;     /* this run's */
    int             relaunched; /* the words are the game's RELAUNCH line: played at once, no Home */
    /* Where the next game starts, the engine's --url: a save's ?loadgame=N
     * (core/saves.h), the window's Continue or a save's Load; a server's
     * host:port, with ?password= when it asks for one, Multiplayer's Join;
     * "" for the game's own start. One game's: cleared as it ends. */
    char            url[512];
    char            message[512];   /* why the run stopped short */
    /* The GPU as dxl-cli --probe found it, asked once a run, before the
     * first game that would multisample (commit). */
    int             gpu_asked;
    dxl_gpu_probe   gpu;
} dxl_run;

/* A screen no one answered: no window opened for it, or a signal stopped
 * it. Not the player's quit: the run ends as it is, Running.ini left for
 * the next start. */
#define DXL_VIEW_UNANSWERED (-1)

typedef struct {
    void *ctx;
    /* A screen: 1 to play, 0 to quit, or DXL_VIEW_UNANSWERED. */
    int  (*view)(void *ctx, dxl_run *r, dxl_view which);
    /* The window down before the game, and back after it. */
    void (*before_play)(void *ctx, dxl_run *r);
    void (*after_play)(void *ctx, dxl_run *r);
} dxl_run_ui;

/* argv as main() has it; stop, set by a signal, stops the game. Returns the
 * exit code. */
int dxl_run_main(int argc, char **argv, const dxl_run_ui *ui, atomic_int *stop);

/* The profile id the run's from now on, as the window switches to it: its
 * config, crash sentinel and last run the run's (app/session.h's
 * dxl_session_use_profile). */
int dxl_run_use_profile(dxl_run *r, const char *id, dxl_err *err);

/* The launcher's own pad mappings, $XDG_DATA_HOME/dxlauncher/gamecontrollerdb.txt
 * (core/padmap.h): the window's and, with Steam's, the engine's. */
char *dxl_run_pad_db(const dxl_run *r);

/* The launcher's own options out of argv, the rest joined into *words
 * (caller frees). -1 for one it does not know, named in err. */
int dxl_run_parse(int argc, char **argv, dxl_run_options *opt, char **words, dxl_err *err);

/* The engine at path asked for its --version: 0 when it is a DXEngine
 * with the userdir feature, else -1, brief what is wrong in a few words
 * ("needs glibc 2.44, this system has 2.43") and why the whole of it: an
 * engine not there, one this system cannot start and the loader's reason
 * (a library it lacks, a glibc newer than its own), one that is no
 * DXEngine, or one without the userdir feature. */
int dxl_engine_check(const char *path, dxl_engine_version *v, char *brief, size_t brief_size, char *why,
                     size_t size);
/* The same for the session's engine. */
int dxl_run_engine_check(const dxl_session *s, dxl_engine_version *v, char *brief, size_t brief_size, char *why,
                         size_t size);
/* The engine's program in a folder, DXEngine; NULL for none. Caller
 * frees. */
char *dxl_engine_in(const char *dir);
/* Whether the engine at path finds its DXEngine.pk3: beside it, or
 * where the engine looks after (/usr/share/dxengine, the data home's
 * dxengine, ~/.dxengine); on Windows beside it alone, as its engine looks. */
int dxl_engine_pk3_found(const char *path);

#endif
