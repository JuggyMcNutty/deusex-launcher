/* What a profile's last run left: how it ended, and its log.
 *
 *   <state>/LastRun.ini         [LastRun] Ended, Status, Signal, Ready, Clean,
 *                               Error, Fallback, Start: the run's end,
 *                               written as it ends, and where the game
 *                               started; taken away as the next run begins
 *   <state>/logs/last-run.log   the engine's output, all of it
 *   <state>/logs/previous-run.log   the run's before it
 *
 * Running.ini, the crash sentinel beside them (core/sentinel.h), says that a
 * run did not end cleanly; this says how it ended, for the Recovery screen:
 * the exit code or the signal, the engine's own error line, the log's last
 * lines, and whether the engine was ever up.
 */
#ifndef DXL_LASTRUN_H
#define DXL_LASTRUN_H

#include "core/common.h"

#define DXL_LASTRUN_LOG     "last-run.log"
#define DXL_PREVIOUSRUN_LOG "previous-run.log"

typedef struct {
    int  known;        /* a record was there */
    long ended;        /* seconds since the epoch */
    int  status;       /* the exit code, or 128 + the signal */
    int  signal;       /* what ended it, 0 when it exited (platform/os.h, dxl_end_name) */
    int  ready;        /* the engine said its first map was in */
    int  clean;        /* exit 0 with ready seen */
    int  stopped;      /* the launcher stopped it: its window closed, a signal, a logout */
    int  killed;       /* it took no notice of the stop and was killed */
    char error[512];   /* the engine's "DXEngine error: ..." line, or "" */
    char fallback[512];/* the engine's line where its GPU renderer did not start and its software
                          one drew instead, or "" */
    char start[64];    /* where the game started: a save's ?loadgame=N; "" for its own start */
} dxl_lastrun;

int  dxl_lastrun_load(dxl_lastrun *r, const char *state_dir);
int  dxl_lastrun_save(const dxl_lastrun *r, const char *state_dir, dxl_err *err);
/* As a run begins, its Running.ini made: the run before's record taken
 * away, so that a launcher stopped before this run's end (killed, the
 * session's end, a power cut) leaves none, never that one's as this one's.
 * 0 when there is none now. */
int  dxl_lastrun_clear(const char *state_dir);
/* Whether the record is the last run's to show, for a run that did not end
 * cleanly (unclean: its Running.ini there) or for any: one that says it
 * ended cleanly is not that run's, but an older launcher's record. */
int  dxl_lastrun_shown(const dxl_lastrun *r, int unclean);

/* Before a run: last-run.log becomes previous-run.log. Returns the new
 * log's path; caller frees. */
char *dxl_lastrun_rotate(const char *logs_dir);

/* The engine's error line in a log, without its "DXEngine error: ":
 * 1 when there is one. */
int   dxl_lastrun_error_line(const char *log_path, char *out, size_t size);
/* The engine's line where a GPU renderer did not start and the software one
 * drew in its place ("The Vulkan device did not start (...): the software
 * device draws in its place"), without its log prefix: 1 when there is one. */
int   dxl_lastrun_fallback_line(const char *log_path, char *out, size_t size);
/* The last lines of a log, at most max_lines; caller frees. NULL when
 * there is none. */
char *dxl_lastrun_tail(const char *log_path, int max_lines);

/* One line saying how a run ended: "exit 0", "exit 1: <the error>",
 * "killed by SIGSEGV". */
void dxl_lastrun_describe(const dxl_lastrun *r, char *out, size_t size);

#endif
