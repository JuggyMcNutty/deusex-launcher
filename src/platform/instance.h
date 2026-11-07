/* Single-instance detection and command-line handoff.
 *
 * The original does two separate things that are easy to conflate:
 *
 *   1. CreateMutex("DeusExIsRunning") + ERROR_ALREADY_EXISTS, used ONLY to
 *      decide whether a surviving Running.ini means a crash or a concurrent
 *      instance (dx-reverse-info/launch-flow.md, "Platform seams").
 *   2. FindWindowEx for a WLog window carrying the "IsBrowser" property, then
 *      WM_COPYDATA with the tail of the command line and a 30s timeout
 *      (section 2) -- so a second launch opens a URL in the running game
 *      instead of starting a second copy.
 *
 * Both survive here; only the transport changes, the platform's
 * (platform/posix/instance.c: a lock on a pidfile, a unix socket;
 * platform/win32/instance.c: a named mutex, the original's way, and a named
 * pipe). The
 * protocol is unchanged: one message, the command line. A wait for one is
 * platform/wait.h's.
 */
#ifndef DXL_INSTANCE_H
#define DXL_INSTANCE_H

#include "core/common.h"

/* The longest message forwarded, its bytes: a receiver's buffer holds one
 * more, for the terminating NUL dxl_instance_poll writes. */
#define DXL_HANDOFF_MAX 4096

typedef struct dxl_instance dxl_instance;

/* Becomes the primary instance for id. NULL if another already is. The
 * desktop launcher's id is its home's (app/session.h): one launcher for a
 * home, whatever the install. */
dxl_instance *dxl_instance_acquire(const char *id);
/* The lock alone, with no channel to forward to: an instance nothing should
 * hand a command line to -- the original's -server, whose log window is
 * never marked IsBrowser. */
dxl_instance *dxl_instance_acquire_lock(const char *id);
/* The channel alone, without the lock: a client started beside a launcher
 * that holds it and takes no line (-server's), as the original marks every
 * client's log window IsBrowser. NULL when a launcher listens already, or
 * there is no channel to be had. */
dxl_instance *dxl_instance_listen(const char *id);
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

/* Whether a second start can hand it a command line: an instance with a
 * channel, not the lock alone. */
int dxl_instance_listens(const dxl_instance *inst);
/* Whether it holds the lock: not the channel alone. */
int dxl_instance_locked(const dxl_instance *inst);

#endif
