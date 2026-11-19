/* A start of either program, where POSIX and Windows differ: what ends it
 * from outside. platform/posix/startup.c is POSIX's: SIGINT, SIGTERM and
 * SIGHUP; a write to a closed pipe is no signal; SIGCHLD as the system has
 * it, whatever the parent left, so a child's end is the launcher's to see;
 * SDL asked for X11 first where Wayland's windows would have no frame.
 */
#ifndef DXL_STARTUP_H
#define DXL_STARTUP_H

#include <stdatomic.h>

/* From here on, an end asked from outside sets *stop and calls wake (NULL:
 * nothing to wake); a wait it breaks ends at once. An atomic, which a
 * signal's handler may set, and Windows' console handler on a thread of its
 * own. */
void dxl_startup(atomic_int *stop, void (*wake)(void));

/* The program's work is done (the run returned): what waits for the end to
 * be clean -- Windows' closing console, which ends the process as its
 * handler returns -- may let go. Nothing on POSIX. */
void dxl_startup_ended(void);

/* Whether SDL is to be asked for X11 before Wayland: a Wayland session with
 * X11 at hand (XWayland) and no video driver chosen by the player, whose
 * compositor draws no window's frame itself (decorates: 1 it does, 0 it
 * does not, as GNOME's does not, -1 it cannot be asked), and libdecor, by
 * that library's name, not to be opened, with which SDL3 draws the frame
 * where the compositor draws none. Without either the window has no title
 * bar, no close button, no move or resize by the mouse (bugs.md LPORT.18).
 * POSIX's alone. */
int dxl_startup_x11_first(const char *libdecor, int (*decorates)(void));

#endif
