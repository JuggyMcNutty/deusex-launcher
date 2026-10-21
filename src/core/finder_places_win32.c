/* Where the game is on Windows, in this order: Steam's libraries -- the
 * Steam the registry names, and each library its libraryfolders.vdf lists
 * -- GOG's games (GOG.com's keys, GOG Galaxy's and its installers'), the
 * original installer's own key (Unreal Technology's Installed Apps, the
 * CD's and the Steam and GOG releases'), Heroic's GOG games (its
 * gog_store/installed.json in %APPDATA%, and each folder in
 * %USERPROFILE%\Games\Heroic), and on each fixed or removable drive where
 * Windows' installers put the game. No folder is walked down blindly: a
 * drive's is too deep to. The looking is core/finder.c's.
 */
#include "core/finder_places.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "platform/win32/internal.h"

#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FOUND_STEAM    "in Steam's library"
#define FOUND_GOG      "in GOG's games"
#define FOUND_HEROIC   "in Heroic's games"
#define FOUND_SETUP    "where its installer put it"
#define FOUND_DRIVE    "on a drive"

/* A string value of the registry's, as a path; NULL when there is none.
 * Caller frees. */
static char *reg_path(HKEY root, const wchar_t *sub, const wchar_t *name) {
    wchar_t buf[2048];
    DWORD size = sizeof buf;
    if (RegGetValueW(root, sub, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, buf, &size) != ERROR_SUCCESS)
        return NULL;
    return dxl_u8(buf, 1);
}

static char *known(REFKNOWNFOLDERID id) {
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, NULL, &w) == S_OK) out = dxl_u8(w, 1);
    CoTaskMemFree(w);
    return out;
}

/* This machine's own places: its registry and its drives. A home given (a
 * test's) has none of them, as core/finder.h has it; media is then the
 * drives' tops to look on, if any. */
static const char *const own_drives[] = { NULL };

void dxl_finder_setup(finder *f, const dxl_finder_where *w) {
    int own = !w->home;
    f->home = w->home ? w->home : dxl_fs_home();
    f->config = w->config_home ? w->config_home : own ? (f->own[0] = known(&FOLDERID_RoamingAppData)) : NULL;
    f->data = w->data_home ? w->data_home : own ? (f->own[1] = known(&FOLDERID_LocalAppData)) : NULL;
    f->wineprefix = NULL;
    f->media = w->media ? w->media : own ? own_drives : NULL;
    f->mounts = NULL;
}

/* Each Steam the registry names, its libraries. */
static void steams(finder *f) {
    static const struct {
        HKEY root;
        const wchar_t *sub, *name;
    } keys[] = {
        { HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath" },
    };
    for (size_t i = 0; i < sizeof keys / sizeof *keys && !dxl_finder_stopped(f); i++) {
        char *root = reg_path(keys[i].root, keys[i].sub, keys[i].name);
        if (root) dxl_finder_steam(f, FOUND_STEAM, root, NULL);
        free(root);
    }
}

/* Each game GOG's keys list, by its "path". */
static void gog(finder *f) {
    static const wchar_t *const subs[] = { L"SOFTWARE\\WOW6432Node\\GOG.com\\Games", L"SOFTWARE\\GOG.com\\Games" };
    for (size_t i = 0; i < sizeof subs / sizeof *subs && !dxl_finder_stopped(f); i++) {
        HKEY k;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subs[i], 0, KEY_READ, &k) != ERROR_SUCCESS) continue;
        wchar_t name[256];
        for (DWORD j = 0; !dxl_finder_stopped(f); j++) {
            DWORD len = sizeof name / sizeof *name;
            if (RegEnumKeyExW(k, j, name, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
            char *path = reg_path(k, name, L"path");
            if (path) dxl_finder_add(f, FOUND_GOG, path, 0);
            free(path);
        }
        RegCloseKey(k);
    }
}

/* The original installer's key: where it put the game, its "Folder". */
static void installed(finder *f) {
    static const struct {
        HKEY root;
        const wchar_t *sub;
    } keys[] = {
        { HKEY_CURRENT_USER, L"SOFTWARE\\Unreal Technology\\Installed Apps\\Deus Ex" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Unreal Technology\\Installed Apps\\Deus Ex" },
        { HKEY_LOCAL_MACHINE, L"SOFTWARE\\Unreal Technology\\Installed Apps\\Deus Ex" },
    };
    for (size_t i = 0; i < sizeof keys / sizeof *keys && !dxl_finder_stopped(f); i++) {
        char *folder = reg_path(keys[i].root, keys[i].sub, L"Folder");
        if (folder) dxl_finder_add(f, FOUND_SETUP, folder, 0);
        free(folder);
    }
}

/* Each fixed or removable drive, where Windows' installers put the game:
 * this machine's, or the tops a test gives. */
static void drives(finder *f) {
    if (f->media != own_drives) {
        for (size_t i = 0; f->media[i] && !dxl_finder_stopped(f); i++) dxl_finder_drive(f, FOUND_DRIVE, f->media[i]);
        return;
    }
    char **roots = dxl_fs_roots();
    for (size_t i = 0; roots && roots[i] && !dxl_finder_stopped(f); i++) {
        wchar_t *w = dxl_w(roots[i], 1);
        UINT type = GetDriveTypeW(w);
        free(w);
        if (type == DRIVE_FIXED || type == DRIVE_REMOVABLE) dxl_finder_drive(f, FOUND_DRIVE, roots[i]);
    }
    dxl_fs_roots_free(roots);
}

void dxl_finder_places(finder *f) {
    if (f->media == own_drives) {
        steams(f);
        gog(f);
        installed(f);
    }
    if (f->config) dxl_finder_heroic(f, f->config);
    if (f->home && *f->home) {
        char *p = dxl_path_join(f->home, "Games/Heroic");
        dxl_finder_add_children(f, FOUND_HEROIC, p);
        free(p);
    }
    if (f->media) drives(f);
}
