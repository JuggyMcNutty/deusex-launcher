/* The original launcher's run, start to end: WinMain and InitEngine, in
 * their order (dx-reverse-info/launch-flow.md).
 *
 *   1. Forwarding: unless the command line holds Server, NewWindow,
 *      changevideo or TestRenDev, a running instance that listens -- the
 *      original's IsBrowser window -- is handed the command line, and this
 *      run ends there.
 *   2. The configuration (<Package>.ini, from Default.ini if missing), the
 *      log, -make (fatal), client or -server.
 *   3. The splash, unless -log, -server or TestRenDev.
 *   5. InitEngine: the single-instance lock, FirstRun, the save migration,
 *      -consolecommand= and -testrendev= (each ends the run), the wizard (its
 *      Cancel ends the run), Running.ini, FirstRun raised to 1100, the CD
 *      check (its Cancel ends the run at once, Running.ini left), the game.
 *   9. The game runs as the launcher's child, the launcher waiting as the
 *      original's process did: the splash closes when the engine says it is
 *      up, and a forwarded command line is handed on. Then, whatever came of
 *      the run, Running.ini is deleted -- unless the game did not end
 *      cleanly, or the run ended at the CD prompt.
 *
 * Where the screens come in, the run calls a dxl_launch_ui: the splash, the
 * wizard, two message boxes. Everything else it does itself.
 */
#ifndef DXL_LAUNCH_RUN_H
#define DXL_LAUNCH_RUN_H

#include "core/config.h"
#include "core/detail.h"
#include "core/localize.h"
#include "core/policy.h"

/* What the wizard is started with, and what it may leave. */
typedef struct {
    dxl_screen   start;        /* where it opens: a policy screen */
    const char  *title;        /* the caption: Startup.int SafeMode, RecoveryMode, FirstTime or Video */
    dxl_config  *config;       /* the game's configuration, which the pages write */
    dxl_loc     *loc;          /* the game's strings */
    const char  *system_dir;   /* the launcher's own folder, the game's System/ */
    const char  *package;      /* DeusEx: names <Package>.ini */
    const char  *exe_path;     /* the launcher, for the detection run and the relaunch */
    const char  *cmdline;      /* the command line, for -nodetect */
    dxl_machine  machine;      /* for the Detail page */
    /* Set by the wizard when SafeOptions' Run! ends it: the flags to start
     * the launcher again with (malloc'd; the launcher frees it). */
    char        *relaunch_flags;
} dxl_wizard_request;

typedef struct {
    void *ctx;
    /* The splash, from this bitmap; NULL shows it again as it was. */
    void (*splash_show)(void *ctx, const char *bmp_path);
    void (*splash_hide)(void *ctx);
    /* The wizard: 1 when it ends in Run, 0 in Cancel -- which is also how it
     * ends after safe mode's relaunch or the web page. */
    int  (*wizard)(void *ctx, dxl_wizard_request *req);
    /* A task-modal box with OK and Cancel: 1 for OK. */
    int  (*ok_cancel)(void *ctx, const char *title, const char *text);
    /* The critical error box. */
    void (*critical)(void *ctx, const char *title, const char *text);
    /* Keeps the windows answering while the launcher waits on the game. */
    void (*pump)(void *ctx);
} dxl_launch_ui;

typedef struct {
    int   exit_code;
    /* Safe mode's relaunch, after everything else: the flags, or NULL. The
     * caller starts the launcher again with them (platform/launch.h). */
    char *relaunch_flags;
    /* Where the run stopped, for tests and the log. */
    const char *ended;
} dxl_launch_result;

/* argv as main() got it; exe_path is the launcher's own path, whose folder
 * is the base directory (the game's System/) and whose name, without an
 * extension, is the package. The game is started as <base>/run-game.sh.
 * *stop, when set (by a signal handler), is passed on to the game as SIGTERM. */
void dxl_launch_run(int argc, char **argv, const char *exe_path, const dxl_launch_ui *ui,
                    volatile int *stop, dxl_launch_result *out);
void dxl_launch_result_free(dxl_launch_result *r);

#endif
