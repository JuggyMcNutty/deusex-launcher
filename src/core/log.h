/* Logging.
 *
 * The original opens a WLog window on every launch and writes DeusEx.log.
 * Here the window is gone (it is a Win32 widget, and on a 1280x720 handheld
 * it would be in the way), but the file stays: the on-device verification
 * matrix reads it, and it is the only way to see what happened after the
 * launcher has exec'd into something else.
 */
#ifndef DXL_LOG_H
#define DXL_LOG_H

#include "core/common.h"

/* path may be NULL for stderr only. Appends; never truncates an existing log
 * mid-session, since a safe-mode relaunch continues the same story. */
void dxl_log_open(const char *path);
void dxl_log_close(void);
void dxl_log(const char *fmt, ...);
/* Mirrors to stderr as well as the file. On by default. */
void dxl_log_set_echo(int on);

/* Turns the file side off entirely, so dxl_log_open becomes a no-op. --dry-run
 * uses this: a run that promises to write nothing must not create a log in
 * someone's game directory just to say so. */
void dxl_log_set_to_file(int on);

#endif
