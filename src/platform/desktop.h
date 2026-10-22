/* The launcher's entry in the desktop's app menu, the deusex:// links'
 * handler. On POSIX (platform/posix/desktop.c) a freedesktop.org desktop
 * entry, <data home>/applications/<DXL_APP_ID>.desktop, made from the
 * app's data/<DXL_APP_ID>.desktop.in with the launcher's own path in Exec;
 * and its icon, <data home>/icons/hicolor/<n>x<n>/apps/<DXL_APP_ID>.png
 * (drawn by ui/icon.h). The launcher's window has the same id, so the
 * desktop puts the two together. On Windows (platform/win32/desktop.c) two
 * Start menu shortcuts, their icon made of the same PNGs, and the handler
 * in the user's registry: no template.
 */
#ifndef DXL_DESKTOP_H
#define DXL_DESKTOP_H

#include "core/common.h"

#define DXL_APP_ID "io.github.juggymcnutty.DXLauncher"

/* The icon sizes written. */
extern const int dxl_desktop_icon_sizes[];
extern const int dxl_desktop_icon_count;

/* Caller frees each. */
char *dxl_desktop_entry_path(const char *data_home);
char *dxl_desktop_icon_path(const char *data_home, int px);

/* Exec's value for program: on POSIX quoted and escaped as the spec's Exec
 * key and string type have it; on Windows quoted where it needs. Caller
 * frees. */
char *dxl_desktop_exec(const char *program);

/* The entry: on POSIX from the template file, @EXEC@ the program's Exec,
 * then the applications folder's MimeType cache made again where the
 * desktop has update-desktop-database, for the deusex:// links the entry
 * handles; on Windows the shortcuts, their icon and the links' handler, no
 * template. */
int  dxl_desktop_install(const char *data_home, const char *template_path, const char *program, dxl_err *err);
/* The entry and the icons, gone. */
int  dxl_desktop_remove(const char *data_home, dxl_err *err);
int  dxl_desktop_installed(const char *data_home);

/* The menu the entry goes in, as the window names it: "the app menu" here,
 * "the Start menu" on Windows. */
#ifdef _WIN32
#define DXL_DESKTOP_MENU "the Start menu"
#else
#define DXL_DESKTOP_MENU "the app menu"
#endif

#endif
