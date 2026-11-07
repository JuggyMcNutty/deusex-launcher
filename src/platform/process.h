/* The processes the launcher starts: the game, which it waits for, and
 * the engine asked what it can do (--version).
 *
 * The original's process becomes the game: its launcher code stays for the
 * game's whole run, holding the single-instance mutex and the IsBrowser
 * window a second launch forwards to, and deletes Running.ini once the game
 * ends cleanly (dx-reverse-info/launch-flow.md section 9). Here the engine is
 * a program of its own, so the launcher starts it as a child and stays: the
 * game's output goes to a log of the launcher's, and a line in each
 * direction runs between the two -- the engine says when it is up, and the
 * launcher hands it what a second launch forwarded (app/play.h). The engine
 * finds its end of that line through DXL_LAUNCHER_FD in its environment; an
 * engine that never answers on it just runs. platform/posix/process.c is
 * POSIX's: a socket pair, the engine's end a descriptor.
 */
#ifndef DXL_PROCESS_H
#define DXL_PROCESS_H

#include "core/common.h"
#include "platform/instance.h"

#include <stdatomic.h>

#define DXL_LAUNCHER_FD_ENV "DXL_LAUNCHER_FD"

/* The platform's: the process, the launcher's end of the line. */
typedef struct dxl_game_os dxl_game_os;

typedef struct {
    dxl_game_os *os;
    long   pid;           /* its process id */
    char  *pending;       /* a line the engine has not finished, of any length */
    size_t pending_len, pending_cap;
    int    status;        /* once ended: the exit code, or 128 + the signal */
    int    signal;        /* once ended: what ended it, not its own exit (platform/os.h,
                           * dxl_end_name), or 0 */
    int    ended;
} dxl_game;

/* Starts argv[0], the program's path, with the words as given -- no shell
 * splits them again, so a quoted word arrives whole -- in workdir. Its
 * output and errors are appended to log_path (NULL: this process's). env is
 * NULL or a NULL-terminated list of changes to this process's environment
 * for the game: "NAME=value" sets one, "NAME" alone takes it out. */
int dxl_game_start_argv(dxl_game *g, char *const argv[], char *const env[], const char *workdir,
                        const char *log_path, dxl_err *err);

/* What dxl_game_wait saw. */
#define DXL_GAME_LINE  1   /* a line from the engine, in line */
#define DXL_GAME_EXTRA 2   /* a second start's line waits on the instance's channel */
#define DXL_GAME_ENDED 4   /* the game ended: g->status */

/* Waits up to ms for any of them (inst NULL: no second start's); 0 when
 * nothing happened. A line comes whole, of any length (the engine's RELAUNCH
 * line can be long), malloc'd into *line: the caller frees it. */
int dxl_game_wait(dxl_game *g, dxl_instance *inst, int ms, char **line);

/* A line to the engine; nothing when it has no line. */
void dxl_game_send(dxl_game *g, const char *line);

/* Asks the game to end, as closing it does: SIGTERM here. */
void dxl_game_stop(dxl_game *g);
/* Ends it at once: SIGKILL here. */
void dxl_game_kill(dxl_game *g);

/* A clean end: the game exited with 0. */
int dxl_game_clean(const dxl_game *g);

/* Lets go of what the platform holds for the game, once it has ended. */
void dxl_game_close(dxl_game *g);

/* Runs argv to its end, at most timeout_ms (then it is killed), its output
 * into out and its errors into errs (NULL: nowhere), each cut at its size
 * and NUL-terminated: the engine's --version. *status is its exit code, or
 * 128 + the signal (Windows: what ended it, platform/os.h). -1 when it
 * could not be run, DXL_RUN_TIMED_OUT when it ran out of time. */
#define DXL_RUN_TIMED_OUT (-2)
int  dxl_run_capture(char *const argv[], int timeout_ms, char *out, size_t size, char *errs, size_t errs_size,
                     int *status, dxl_err *err);
/* The same, given up once *stop is set (NULL: never), within a tenth of a
 * second: the program killed then, and DXL_RUN_STOPPED -- the window's
 * probe as the window goes. */
#define DXL_RUN_STOPPED (-3)
int  dxl_run_capture_until(char *const argv[], int timeout_ms, const atomic_int *stop, char *out, size_t size,
                           char *errs, size_t errs_size, int *status, dxl_err *err);

#endif
