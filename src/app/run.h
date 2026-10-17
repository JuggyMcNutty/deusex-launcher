/* One start of the launcher, from its command line to the game's end:
 *
 *   1. A second start hands its command line to the launcher that runs
 *      (core/instance.h) and ends -- unless the line holds one of the
 *      original's four bypass words (Server, NewWindow, changevideo,
 *      TestRenDev).
 *   2. The folders, launcher.ini, the active profile (app/session.h). No
 *      profile yet: the first run makes one from an install (--game=<dir>,
 *      or the First run screen).
 *   3. The decision (core/policy.h): Home; Recovery after -safe or after a
 *      run that did not end cleanly, whose Running.ini survived; Graphics
 *      after -changevideo. --play and DXL_NO_HOME=1 skip Home, not the
 *      others.
 *   4. Before the game, after the screens, as the original does it: the
 *      settings saved, FirstRun raised to 1100, the crash sentinel made.
 *   5. The game (app/play.h). An engine whose --version lacks the userdir
 *      feature is refused, so nothing ever writes ~/.config/SurrealEngine.
 *   6. The end: a clean one takes the sentinel away and runs a RELAUNCH the
 *      engine passed on; one that is not leaves it, and the next start
 *      opens on Recovery.
 *
 * The launcher's own options start with "--"; every other word is the
 * original's command line, the player's, passed to the engine in
 * --cmdline. The screens are the caller's (dxl_run_ui); with none -- dxl-cli,
 * and deusex-launcher until its window -- Home is skipped and any other
 * screen that is due is a message and DXL_RUN_EXIT_SCREEN.
 */
#ifndef DXL_RUN_H
#define DXL_RUN_H

#include "app/play.h"
#include "app/session.h"
#include "core/config.h"
#include "core/instance.h"
#include "core/lastrun.h"
#include "core/policy.h"
#include "core/sentinel.h"

/* Exit codes besides the game's own status. */
#define DXL_RUN_EXIT_USAGE   2   /* an option the launcher does not know */
#define DXL_RUN_EXIT_SCREEN  3   /* a screen is due and there is no window */
#define DXL_RUN_EXIT_NOGAME  4   /* no install to play, or no engine to play it */

typedef enum {
    DXL_VIEW_NONE,
    DXL_VIEW_HOME,
    DXL_VIEW_FIRST_RUN,
    DXL_VIEW_RECOVERY,
    DXL_VIEW_GRAPHICS
} dxl_view;

typedef struct {
    const char *game;       /* --game=<dir>: the install for a first run, or the profile's now */
    const char *profile;    /* --profile=<id> */
    int play;               /* --play: no Home */
    int dry;                /* --dry-run: decide and say, write nothing */
    int print_command;      /* --print-command: the engine's command line, nothing run */
    int import_only;        /* --import: make the profile from --game, nothing run */
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
    char            message[512];   /* why the run stopped short */
} dxl_run;

typedef struct {
    void *ctx;
    /* A screen: 1 to play, 0 to quit. */
    int  (*view)(void *ctx, dxl_run *r, dxl_view which);
    /* The window down before the game, and back after it. */
    void (*before_play)(void *ctx, dxl_run *r);
    void (*after_play)(void *ctx, dxl_run *r);
} dxl_run_ui;

/* argv as main() has it; stop, set by a signal, stops the game. Returns the
 * exit code. */
int dxl_run_main(int argc, char **argv, const dxl_run_ui *ui, volatile int *stop);

/* The launcher's own options out of argv, the rest joined into *words
 * (caller frees). -1 for one it does not know, named in err. */
int dxl_run_parse(int argc, char **argv, dxl_run_options *opt, char **words, dxl_err *err);

#endif
