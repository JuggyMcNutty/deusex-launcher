/* Where Windows keeps an app's own files (platform/folders.h), each named
 * after the product, DXLauncher:
 *
 *   config  %LOCALAPPDATA%\DXLauncher\config   not the roaming folder: the
 *                                               inis name this machine's paths
 *   data    Saved Games\DXLauncher             the player's saves and shots
 *   state   %LOCALAPPDATA%\DXLauncher\state
 *   cache   %LOCALAPPDATA%\DXLauncher\cache
 *
 * and the shortcut's icon under %LOCALAPPDATA%\DXLauncher (data_home).
 */
#include "platform/folders.h"
#include "platform/win32/internal.h"
#include "core/paths.h"

#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

static char *known(REFKNOWNFOLDERID id) {
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, NULL, &w) == S_OK) out = dxl_u8(w, 1);
    CoTaskMemFree(w);
    return out;
}

int dxl_app_folders_get(const char *app, dxl_app_folders *f, dxl_err *err) {
    memset(f, 0, sizeof *f);
    const char *name = !strcmp(app, "dxlauncher") ? "DXLauncher" : app;
    char *local = known(&FOLDERID_LocalAppData), *saved = known(&FOLDERID_SavedGames);
    if (!local) {
        dxl_err_set(err, "no local app data folder for this user");
        free(saved);
        return -1;
    }
    f->data_home = dxl_path_join(local, name);
    f->config = dxl_path_join(f->data_home, "config");
    f->state = dxl_path_join(f->data_home, "state");
    f->cache = dxl_path_join(f->data_home, "cache");
    f->data = saved ? dxl_path_join(saved, name) : dxl_path_join(f->data_home, "data");
    free(local);
    free(saved);
    return 0;
}

void dxl_app_folders_free(dxl_app_folders *f) {
    if (!f) return;
    free(f->config);
    free(f->data);
    free(f->state);
    free(f->cache);
    free(f->data_home);
    memset(f, 0, sizeof *f);
}
