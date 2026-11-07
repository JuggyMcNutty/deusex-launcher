/* Single-instance detection and command-line handoff.
 *
 * The original does two separate things that are easy to conflate:
 *
 *   1. CreateMutex("DeusExIsRunning") + ERROR_ALREADY_EXISTS, used ONLY to
 *      decide whether a surviving Running.ini means a crash or a concurrent
 *      instance (dx-reverse-info/launch-flow.md, "Platform seams"). Every
 *      instance makes it and keeps its handle to its end, so the mutex is
 *      there while any instance runs, the first or a later one.
 *   2. FindWindowEx for a WLog window carrying the "IsBrowser" property, then
 *      WM_COPYDATA with the tail of the command line and a 30s timeout
 *      (section 2) -- so a second launch opens a URL in the running game
 *      instead of starting a second copy. Every client's log window carries
 *      the property, whatever runs beside it; a server's never (section 4).
 *
 * Both survive here; only the transport changes, the platform's
 * (platform/posix/instance.c: a lock each launcher shares on a file, a unix
 * socket each client; platform/win32/instance.c: a named mutex each
 * launcher makes, the original's way, and a named pipe each client). The
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

/* Marks this launcher running for id until it is released, as every
 * launcher does, the original's mutex every instance keeps; and opens a
 * channel of its own a second start can hand its command line to, as the
 * original marks every client's log window IsBrowser, whatever runs beside
 * it. NULL when neither can be had. The desktop launcher's id is its
 * home's (app/session.h): the channels of a home, whatever the install. */
dxl_instance *dxl_instance_acquire(const char *id);
/* The mark alone, with no channel to forward to: an instance nothing should
 * hand a command line to -- the original's -server, whose log window is
 * never marked IsBrowser. */
dxl_instance *dxl_instance_acquire_lock(const char *id);
void          dxl_instance_release(dxl_instance *inst);

/* Is a launcher other than inst (NULL: any) marked running for id, now? The
 * Running.ini question, as the original's mutex answers it: a launcher
 * started beside another, which outlives it, live for a later start. */
int dxl_instance_other_running(const dxl_instance *inst, const char *id);

/* Sends message to a client's channel, the newest that takes it, as the
 * original's to the log window it finds first, the newest. Returns 0 on
 * success. The original allows 30s; nothing here should ever take that
 * long, but the timeout exists so a wedged launcher cannot hang the
 * messenger forever. */
int dxl_instance_forward(const char *id, const char *message, int timeout_ms,
                         dxl_err *err);

/* Non-blocking. Returns 1 and NUL-terminates buf when a handoff arrived. */
int dxl_instance_poll(dxl_instance *inst, char *buf, size_t size);

/* Whether a second start can hand it a command line: an instance with the
 * channel, not the mark alone. */
int dxl_instance_listens(const dxl_instance *inst);
/* Whether it marks its launcher running. */
int dxl_instance_locked(const dxl_instance *inst);

#endif
