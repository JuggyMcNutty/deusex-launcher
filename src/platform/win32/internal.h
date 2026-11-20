/* What Windows' platform files share among themselves and no one else but
 * their tests: UTF-8 to UTF-16 and back -- the launcher's paths are UTF-8
 * with '/', Windows' wide calls take UTF-16 -- errno from Windows' errors,
 * and the single instance's channel, an event the waits wait on, and its
 * marks, which the game is given. */
#ifndef DXL_WIN32_INTERNAL_H
#define DXL_WIN32_INTERNAL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include "platform/instance.h"

/* s in UTF-16, '/' as '\' when backslashes; NULL for NULL. Caller frees. */
wchar_t *dxl_w(const char *s, int backslashes);
/* w in UTF-8, '\' as '/' when slashes; NULL for NULL. Caller frees. */
char *dxl_u8(const wchar_t *w, int slashes);

/* errno for a Windows error (GetLastError()'s). */
void dxl_win_errno(DWORD error);
/* A Windows error's text, into buf, its line's end off. */
const char *dxl_win_error(DWORD error, char *buf, size_t size);

/* The channel's event, set while something waits on it; NULL when the
 * instance has none. */
HANDLE dxl_instance_event(const dxl_instance *inst);

/* The marks the game is given a handle of, so that each lives as long as
 * the game does, as the original's mutex is the game's process's (bugs.md
 * LPORT.23): the mark of a launcher running, and of a client's game
 * playing; NULL for none. */
HANDLE dxl_instance_mark(const dxl_instance *inst);
HANDLE dxl_instance_playing(const dxl_instance *inst);
/* Marks a game that takes a second start's line playing (on 1), or ended
 * (0): an instance with a pipe's alone. */
void dxl_instance_mark_game(dxl_instance *inst, int on);

/* The class of the launcher's hidden window, which has the session's end
 * (platform/win32/startup.c); the tests find it by it. */
#define DXL_SESSION_CLASS L"DXLauncherSession"

/* The tests' (tests/test_instance_win32.c), NULL but there: called with the
 * pipe's name between its making and the primary's first wait for a
 * sender, where a second start can come and go before that wait. */
extern void (*dxl_instance_test_made)(const wchar_t *pipe);

/* The tests' (tests/test_trash_win32.c), NULL but there: called in
 * SHFileOperationW's place with what the Recycle Bin is asked, to fail a
 * part of it. */
struct _SHFILEOPSTRUCTW;
extern int (*dxl_trash_test_op)(struct _SHFILEOPSTRUCTW *op);

/* A word quoted for a command line as CommandLineToArgvW splits it, onto
 * line's end (its size in wchar_t, NUL-terminated; 0 when it would not
 * fit). */
int dxl_win_quote(wchar_t *line, size_t size, const wchar_t *word);

#endif
