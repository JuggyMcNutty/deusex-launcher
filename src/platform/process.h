/* The processes the launcher starts: the game, which it waits for, and
 * the ones it lets go.
 *
 * The original's process becomes the game: its launcher code stays for the
 * game's whole run, holding the single-instance mutex and the IsBrowser
 * window a second launch forwards to, and deletes Running.ini once the game
 * ends cleanly (dx-reverse-info/launch-flow.md section 9). Here the engine is
 * a program of its own, so the launcher starts it as a child and stays: the
 * game's output goes to the launcher's log, and a line in each direction
 * runs between the two -- the engine says when it is up (the splash then
 * closes, as the original's does once the engine exists), and the launcher
 * hands it what a second launch forwarded. The engine finds its end of that
 * line through DXL_LAUNCHER_FD in its environment; an engine that never
 * answers on it just runs.
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
    int    ended;
} dxl_game;

/* Starts exe with flags split into its arguments, in workdir. Its output and
 * errors are appended to log_path (NULL: this process's). */
int dxl_game_start(dxl_game *g, const char *exe, const char *flags, const char *workdir,
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

/* Starts exe with flags in workdir and lets it go (the Renderer page's
 * detection run). Reaped by dxl_child_reap. */
int  dxl_child_start(const char *exe, const char *flags, const char *workdir, pid_t *pid,
                     dxl_err *err);
/* 1 when the child has ended (and is gone), 0 while it runs. */
int  dxl_child_reap(pid_t pid);

/* Opens a URL with the desktop's handler, not waiting for it -- the
 * original's ShellExecute of a web page. */
int  dxl_open_url(const char *url);

#endif
