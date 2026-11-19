/* Where Windows keeps an app's own files (platform/folders.h), each named
 * after the product, DXLauncher:
 *
 *   config  %LOCALAPPDATA%\DXLauncher\config   not the roaming folder: the
 *                                               inis name this machine's paths
 *   data    Saved Games\DXLauncher             the player's saves and shots,
 *                                               Saved Games made where it is
 *                                               missing; an earlier build's
 *                                               %LOCALAPPDATA%\DXLauncher\data
 *                                               kept where it has none
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

/* The folder's path, NULL when Windows has none; its answer into *hr. */
static char *known(REFKNOWNFOLDERID id, DWORD flags, HRESULT *hr) {
    PWSTR w = NULL;
    char *out = NULL;
    *hr = SHGetKnownFolderPath(id, flags, NULL, &w);
    if (*hr == S_OK) out = dxl_u8(w, 1);
    CoTaskMemFree(w);
    return out;
}

int dxl_app_folders_get(const char *app, dxl_app_folders *f, dxl_err *err) {
    memset(f, 0, sizeof *f);
    const char *name = !strcmp(app, "dxlauncher") ? "DXLauncher" : app;
    HRESULT hr;
    char *local = known(&FOLDERID_LocalAppData, KF_FLAG_DEFAULT, &hr);
    if (!local) {
        dxl_err_set(err, "no local app data folder for this user");
        return -1;
    }
    /* The data folder is Saved Games' at every start: made where it is
     * missing; where it cannot be had (moved to a drive that is not there)
     * the start stops, saying so, rather than the data going elsewhere
     * (bugs.md LPORT.12). */
    char *saved = known(&FOLDERID_SavedGames, KF_FLAG_CREATE, &hr);
    if (!saved) {
        dxl_err_set(err,
                    "the Saved Games folder, where the launcher keeps the player's saves, cannot be "
                    "had (0x%08lX): where it is moved to another drive, that drive must be there",
                    (unsigned long)hr);
        free(local);
        return -1;
    }
    f->data_home = dxl_path_join(local, name);
    f->config = dxl_path_join(f->data_home, "config");
    f->state = dxl_path_join(f->data_home, "state");
    f->cache = dxl_path_join(f->data_home, "cache");
    f->data = dxl_path_join(saved, name);
    /* The data folder an earlier build made in the local one, at starts
     * that found no Saved Games, kept while Saved Games has none of the
     * launcher's: the profiles made there name it as their SavePath
     * (bugs.md LPORT.12). */
    char *before = dxl_path_join(f->data_home, "data");
    if (!dxl_path_is_dir(f->data) && dxl_path_is_dir(before)) {
        free(f->data);
        f->data = before;
    } else {
        free(before);
    }
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
