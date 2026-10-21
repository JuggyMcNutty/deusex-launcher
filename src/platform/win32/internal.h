/* What Windows' platform files share among themselves and no one else:
 * UTF-8 to UTF-16 and back -- the launcher's paths are UTF-8 with '/',
 * Windows' wide calls take UTF-16 -- errno from Windows' errors, and the
 * single instance's channel, an event the waits wait on. */
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

/* A word quoted for a command line as CommandLineToArgvW splits it, onto
 * line's end (its size in wchar_t, NUL-terminated; 0 when it would not
 * fit). */
int dxl_win_quote(wchar_t *line, size_t size, const wchar_t *word);

#endif
