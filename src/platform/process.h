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
 * engine that never answers on it just runs.
 */
#ifndef DXL_PROCESS_H
#define DXL_PROCESS_H

#include "core/common.h"

#include <sys/types.h>

#define DXL_LAUNCHER_FD_ENV "DXL_LAUNCHER_FD"

typedef struct {
    pid_t  pid;
    int    channel;       /* the launcher's end of the line, or -1 */
    char   pending[1024]; /* a line the engine has not finished */
    size_t pending_len;
    int    status;        /* once ended: the exit code, or 128 + the signal */
    int    signal;        /* once ended: the signal that ended it, or 0 */
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
#define DXL_GAME_EXTRA 2   /* extra_fd is readable */
#define DXL_GAME_ENDED 4   /* the game ended: g->status */

/* Waits up to ms for any of them; 0 when nothing happened. */
int dxl_game_wait(dxl_game *g, int extra_fd, int ms, char *line, size_t size);

/* A line to the engine; nothing when it has no line. */
void dxl_game_send(dxl_game *g, const char *line);

/* Passes a signal on to the game. */
void dxl_game_signal(dxl_game *g, int sig);

/* A clean end: the game exited with 0. */
int dxl_game_clean(const dxl_game *g);

/* Opens a URL with the desktop's handler, not waiting for it -- the
 * original's ShellExecute of a web page. */
int  dxl_open_url(const char *url);

/* Runs argv to its end, at most timeout_ms (then it is killed), its output
 * into out and its errors into errs (NULL: nowhere), each cut at its size
 * and NUL-terminated: the engine's --version. *status is its exit code, or
 * 128 + the signal. -1 when it could not be run or ran out of time. */
int  dxl_run_capture(char *const argv[], int timeout_ms, char *out, size_t size, char *errs, size_t errs_size,
                     int *status, dxl_err *err);

#endif
