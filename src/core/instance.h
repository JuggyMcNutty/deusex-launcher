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
 * Both survive here; only the transport changes. The mark is a read lock
 * every instance holds to its end on the first byte of a file in the
 * runtime folder ($XDG_RUNTIME_DIR, else one of the user's own under /tmp),
 * an open file description's (F_OFD_SETLK), so each instance's is its own:
 * made, as the mutex, by a write lock had only where no other instance
 * holds one, then kept as a read lock. The lock is on the description, not
 * the name: the file stays after the process, and a stale one never reads
 * as live -- nothing is written to the install. A client's channel is a
 * unix socket file of its own in that folder, whose mode (0700) keeps other
 * users' lines out, as the original's WM_COPYDATA stays within the player's
 * desktop; removed at release. A second launch sends to the newest channel
 * that takes its line, as the original's to the log window it finds first,
 * the newest.
 *
 * Both are named for the user alone, never the install, as the original's
 * mutex and IsBrowser window are: a launch of another install, a copy or a
 * mod's folder, forwards to the game running and finds it live.
 *
 * The protocol is unchanged: one message, the command line.
 */
#ifndef DXL_INSTANCE_H
#define DXL_INSTANCE_H

#include "core/common.h"

/* The longest message forwarded, its bytes: a receiver's buffer holds one
 * more, for the terminating NUL dxl_instance_poll writes. */
#define DXL_HANDOFF_MAX 4096

typedef struct dxl_instance dxl_instance;

/* Marks this instance running until it is released, whatever the
 * install, as the original's every instance makes its mutex: *other (NULL:
 * not asked) set to whether another instance had marked itself already,
 * the original's ERROR_ALREADY_EXISTS, asked once, as InitEngine asks it.
 * With listen, a channel of its own too, a client's: the original's
 * IsBrowser log window, never a -server's. NULL when neither can be had. */
dxl_instance *dxl_instance_acquire(int listen, int *other);
void          dxl_instance_release(dxl_instance *inst);

/* Is any instance marked running now? Without marking this one. */
int dxl_instance_other_running(void);

/* Sends message to a client's channel, the newest that takes it. Returns 0
 * on success. The original allows 30s; nothing here should ever take that
 * long, but the timeout exists so a wedged client cannot hang the messenger
 * forever. */
int dxl_instance_forward(const char *message, int timeout_ms,
                         dxl_err *err);

/* Non-blocking. Returns 1 and NUL-terminates buf when a handoff arrived. */
int dxl_instance_poll(dxl_instance *inst, char *buf, size_t size);

/* The channel's descriptor, for a caller that waits on it with poll(); -1
 * when the instance has none. */
int dxl_instance_fd(const dxl_instance *inst);

#endif
