/* Logging.
 *
 * The original opens a WLog window on every launch and writes DeusEx.log.
 * Here the window is gone (it is a Win32 widget), but a file stays, written
 * afresh each start, as the original's is (dxl_log_open_new): the desktop
 * launcher's is launcher.log in its state folder, and the engine's own
 * output goes to the profile's run log beside it (core/lastrun.h).
 */
#ifndef DXL_LOG_H
#define DXL_LOG_H

#include "core/common.h"

/* path may be NULL for stderr only. Appends, keeping the file across runs. */
void dxl_log_open(const char *path);
/* The same, starting the file afresh -- as the original's own log is, each
 * run. */
void dxl_log_open_new(const char *path);
void dxl_log_close(void);
void dxl_log(const char *fmt, ...);
/* Mirrors to stderr as well as the file. On by default. */
void dxl_log_set_echo(int on);

/* Turns the file side off entirely, so dxl_log_open becomes a no-op. --dry-run
 * uses this: a run that promises to write nothing must not create a log just
 * to say so. */
void dxl_log_set_to_file(int on);

#endif
