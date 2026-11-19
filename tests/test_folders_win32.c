/* Where the launcher keeps its files on Windows (platform/win32/folders.c):
 * %LOCALAPPDATA%\DXLauncher's config, state and cache, Saved Games\
 * DXLauncher the player's own, Saved Games made where it is missing, an
 * earlier build's data folder in %LOCALAPPDATA% kept (bugs.md LPORT.12); and
 * DXL_HOME's folders, as on every OS, its '\' made '/'. */
#include "test.h"
#include "core/xdg.h"
#include "platform/os.h"

#include <windows.h>
#include <shlobj.h>

static char *known(REFKNOWNFOLDERID id, DWORD flags) {
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(id, flags, NULL, &w) == S_OK) {
        int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
        out = malloc((size_t)n);
        WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL);
        for (char *p = out; *p; p++)
            if (*p == '\\') *p = '/';
    }
    CoTaskMemFree(w);
    return out;
}

static void check_under(const char *got, const char *base, const char *rel) {
    char want[1024];
    snprintf(want, sizeof want, "%s/%s", base, rel);
    CHECK_STR(got, want);
}

static void test_known_folders(void) {
    dxl_env_set("DXL_HOME", NULL);
    char *local = known(&FOLDERID_LocalAppData, 0), *saved = known(&FOLDERID_SavedGames, 0);
    CHECK(local != NULL);
    CHECK(saved != NULL);
    dxl_xdg x;
    dxl_err e;
    CHECK_INT(dxl_xdg_open(&x, &e), 0);
    if (local && saved) {
        check_under(x.config, local, "DXLauncher/config");
        check_under(x.state, local, "DXLauncher/state");
        check_under(x.cache, local, "DXLauncher/cache");
        check_under(x.data_home, local, "DXLauncher");
        check_under(x.data, saved, "DXLauncher");
    }
    dxl_xdg_free(&x);
    free(local);
    free(saved);
}

/* Saved Games moved aside for the test: made again, the data folder in it
 * as before, never %LOCALAPPDATA%'s; but an earlier build's data folder
 * there, made at starts that found no Saved Games, kept while Saved Games
 * has none of the launcher's. Put back after, said where it cannot be. */
static void test_saved_games_missing(void) {
    dxl_env_set("DXL_HOME", NULL);
    char *saved = known(&FOLDERID_SavedGames, KF_FLAG_DONT_VERIFY), *local = known(&FOLDERID_LocalAppData, 0);
    CHECK(saved != NULL);
    CHECK(local != NULL);
    if (!saved || !local) {
        free(saved);
        free(local);
        return;
    }
    char aside[1100], top[1100], before[1100];
    snprintf(aside, sizeof aside, "%s.dxl-test-%lu", saved, (unsigned long)GetCurrentProcessId());
    snprintf(top, sizeof top, "%s/DXLauncher", local);
    snprintf(before, sizeof before, "%s/DXLauncher/data", local);
    wchar_t ws[1100], wa[1100], wt[1100], wb[1100];
    MultiByteToWideChar(CP_UTF8, 0, saved, -1, ws, 1100);
    MultiByteToWideChar(CP_UTF8, 0, aside, -1, wa, 1100);
    MultiByteToWideChar(CP_UTF8, 0, top, -1, wt, 1100);
    MultiByteToWideChar(CP_UTF8, 0, before, -1, wb, 1100);
    int had = GetFileAttributesW(wb) != INVALID_FILE_ATTRIBUTES;
    if (!MoveFileExW(ws, wa, 0)) {
        printf("  Saved Games could not be moved aside (%lu): not tried\n", (unsigned long)GetLastError());
        free(saved);
        free(local);
        return;
    }
    dxl_xdg x, y;
    dxl_err e;
    int rc = dxl_xdg_open(&x, &e);
    DWORD made = GetFileAttributesW(ws);
    /* The earlier build's folder, made for the test where there is none. */
    int top_made = 0, before_made = 0, rc2 = -1;
    if (!had) {
        top_made = CreateDirectoryW(wt, NULL);
        before_made = CreateDirectoryW(wb, NULL);
        if (before_made) rc2 = dxl_xdg_open(&y, &e);
        if (before_made) RemoveDirectoryW(wb);
        if (top_made) RemoveDirectoryW(wt);
    }
    /* What Windows made, its desktop.ini with it, removed; the folder back. */
    wchar_t ini[1200];
    swprintf(ini, 1200, L"%ls\\desktop.ini", ws);
    SetFileAttributesW(ini, FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(ini);
    SetFileAttributesW(ws, FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(ws);
    if (!MoveFileExW(wa, ws, 0)) {
        printf("  Saved Games could not be put back (%lu): it is %s\n", (unsigned long)GetLastError(), aside);
        CHECK(0);
    }
    CHECK_INT(rc, 0);
    CHECK(made != INVALID_FILE_ATTRIBUTES && (made & FILE_ATTRIBUTE_DIRECTORY));
    if (rc == 0) {
        if (had) check_under(x.data, local, "DXLauncher/data");
        else check_under(x.data, saved, "DXLauncher");
        dxl_xdg_free(&x);
    }
    if (had) {
        printf("  %s is there already: the earlier build's folder not made\n", before);
    } else {
        CHECK(before_made);
        CHECK_INT(rc2, 0);
        if (rc2 == 0) {
            check_under(y.data, local, "DXLauncher/data");
            dxl_xdg_free(&y);
        }
    }
    free(saved);
    free(local);
}

static void test_scratch_home(void) {
    dxl_env_set("DXL_HOME", "C:\\dxl test\\home");
    dxl_xdg x;
    dxl_err e;
    CHECK_INT(dxl_xdg_open(&x, &e), 0);
    CHECK_STR(x.config, "C:/dxl test/home/config/dxlauncher");
    CHECK_STR(x.data, "C:/dxl test/home/data/dxlauncher");
    CHECK_STR(x.state, "C:/dxl test/home/state/dxlauncher");
    CHECK_STR(x.cache, "C:/dxl test/home/cache/dxlauncher");
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
}

TEST_MAIN_BEGIN
    RUN(test_known_folders);
    RUN(test_saved_games_missing);
    RUN(test_scratch_home);
TEST_MAIN_END
