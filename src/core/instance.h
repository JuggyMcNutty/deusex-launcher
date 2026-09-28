/* Single-instance detection and command-line handoff.
 *
 * The original does two separate things that are easy to conflate:
 *
 *   1. CreateMutex("DeusExIsRunning") + ERROR_ALREADY_EXISTS, used ONLY to
 *      decide whether a surviving Running.ini means a crash or a concurrent
 *      instance (dx-reverse-info/porting-notes.md section 3).
 *   2. FindWindowEx for a WLog window carrying the "IsBrowser" property, then
 *      WM_COPYDATA with the tail of the command line and a 30s timeout
 *      (section 2) -- so a second launch opens a URL in the running game
 *      instead of starting a second copy.
 *
 * Both survive here; only the transport changes. The lock is flock on a
 * pidfile, and the channel is a Linux abstract unix socket, so nothing is left
 * behind on the filesystem if the process dies -- which matters on exFAT,
 * where a stale lock file would be indistinguishable from a live one.
 *
 * The protocol is unchanged: one message, the command line.
 */
#ifndef DXL_INSTANCE_H
#define DXL_INSTANCE_H

#include "core/common.h"

#define DXL_HANDOFF_MAX 4096

typedef struct dxl_instance dxl_instance;

/* Becomes the primary instance for this install. NULL if another already is.
 * id should identify the install (the game directory); different installs must
 * not lock each other out. */
dxl_instance *dxl_instance_acquire(const char *id);
/* The lock alone, with no channel to forward to: an instance nothing should
 * hand a command line to -- the original's -server, whose log window is
 * never marked IsBrowser. */
dxl_instance *dxl_instance_acquire_lock(const char *id);
void          dxl_instance_release(dxl_instance *inst);

/* Is another instance live? Answers without taking the lock, for the
 * Running.ini question. */
int dxl_instance_other_running(const char *id);

/* Sends message to the primary instance. Returns 0 on success. The original
 * allows 30s; nothing here should ever take that long, but the timeout exists
 * so a wedged primary cannot hang the messenger forever. */
int dxl_instance_forward(const char *id, const char *message, int timeout_ms,
                         dxl_err *err);

/* Non-blocking. Returns 1 and NUL-terminates buf when a handoff arrived. */
int dxl_instance_poll(dxl_instance *inst, char *buf, size_t size);

/* The channel's descriptor, for a caller that waits on it with poll(); -1
 * when the instance has none. */
int dxl_instance_fd(const dxl_instance *inst);

#endif
