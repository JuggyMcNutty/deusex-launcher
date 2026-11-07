/* One run of the game: the engine started as the launcher's child, and the
 * launcher waiting on it, as the original's process stays with its engine
 * (dx-reverse-info/launch-flow.md section 9).
 *
 * The two talk over a line, the engine's end DXL_LAUNCHER_FD in its
 * environment (platform/process.h). The engine says "hello" as it starts,
 * "ready" once its first map is in, and "relaunch <line>" for the
 * original's RELAUNCH before it exits; the launcher hands it "TakeFocus"
 * and "Open <url>" for a line a second launch forwarded once it is ready
 * (DXEngine's ENGINE.md, running it). The wait is on the line and the
 * instance's channel, woken once a second besides: the launcher costs next
 * to nothing while the game runs.
 *
 * A clean end is exit 0 with ready seen: an engine that ends before its
 * first map is in played nothing, whatever its exit. One that finds no
 * game exits 1, its error line "DXEngine error: No UE1 game found in the
 * given folder" (core/lastrun.h).
 */
#ifndef DXL_PLAY_H
#define DXL_PLAY_H

#include "core/common.h"
#include "platform/instance.h"

typedef struct {
    char *const  *argv;      /* the engine's words (core/engine_args.h) */
    char *const  *env;       /* changes to the environment, or NULL */
    const char   *workdir;   /* the game's System folder */
    const char   *log_path;  /* the engine's output and errors */
    dxl_instance *instance;  /* a second launch's lines, or NULL */
    volatile int *stop;      /* set by a signal: the game is stopped (dxl_game_stop, then dxl_game_kill) */
    /* Told as the engine says it is up; may be NULL. */
    void (*on_ready)(void *ctx);
    void *ctx;
} dxl_play_request;

typedef struct {
    int   started;     /* the engine started at all */
    int   status;      /* its exit code, or 128 + the signal */
    int   signal;      /* what ended it, not its own exit, or 0 (platform/os.h) */
    int   hello;
    int   ready;
    int   clean;       /* exit 0 and ready */
    int   stopped;     /* the launcher stopped it */
    int   killed;      /* it took no notice of the stop: killed */
    char *relaunch;    /* the RELAUNCH it passed on, or NULL */
    char  error[256];  /* why it did not start */
} dxl_play_result;

/* How long a game the launcher stops has to end before it is killed:
 * DXEngine quits on SIGTERM at its next frame, so only a stuck one is (its
 * ENGINE.md, running it). */
#define DXL_PLAY_KILL_AFTER_MS 5000

void dxl_play(const dxl_play_request *req, dxl_play_result *out);
void dxl_play_result_free(dxl_play_result *r);

#endif
