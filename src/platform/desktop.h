/* The launcher's entry in the desktop's app menu: a freedesktop.org desktop
 * entry, <data home>/applications/<DXL_APP_ID>.desktop, made from the
 * app's data/<DXL_APP_ID>.desktop.in with the launcher's own path in Exec;
 * and its icon, <data home>/icons/hicolor/<n>x<n>/apps/<DXL_APP_ID>.png
 * (drawn by ui/icon.h). The launcher's window has the same id, so the
 * desktop puts the two together.
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

/* Exec's value for program: quoted and escaped as the spec's Exec key and
 * string type have it. Caller frees. */
char *dxl_desktop_exec(const char *program);

/* The entry from the template file, @EXEC@ the program's Exec; then, and
 * as it goes, the applications folder's MimeType cache made again where
 * the desktop has update-desktop-database, for the deusex:// links the
 * entry handles. */
int  dxl_desktop_install(const char *data_home, const char *template_path, const char *program, dxl_err *err);
/* The entry and the icons, gone. */
int  dxl_desktop_remove(const char *data_home, dxl_err *err);
int  dxl_desktop_installed(const char *data_home);

#endif
