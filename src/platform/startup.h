/* A start of either program, where POSIX and Windows differ: what ends it
 * from outside. platform/posix/startup.c is POSIX's: SIGINT, SIGTERM and
 * SIGHUP; a write to a closed pipe is no signal. platform/win32/startup.c
 * Windows': the console's Ctrl+C, Ctrl+Break and its closing.
 */
#ifndef DXL_STARTUP_H
#define DXL_STARTUP_H

/* From here on, an end asked from outside sets *stop and calls wake (NULL:
 * nothing to wake); a wait it breaks ends at once. */
void dxl_startup(volatile int *stop, void (*wake)(void));

/* The program's work is done (the run returned): what waits for the end to
 * be clean -- Windows' closing console, which ends the process as its
 * handler returns -- may let go. Nothing on POSIX. */
void dxl_startup_ended(void);

#ifdef _WIN32
/* A deusex:// link opened by the registered handler ("<program>" "%1")
 * is the raw command line's one quoted argument, which Windows puts there
 * as it is: a '"' in it splits it into more words for the C runtime's
 * parse, taken as options. The link whole, as argv of two (the program's
 * name, then the link; the caller keeps it), when the raw line's first
 * argument is a quoted deusex:// link; else NULL. */
char **dxl_startup_link_argv(char **argv);
#endif

#endif
