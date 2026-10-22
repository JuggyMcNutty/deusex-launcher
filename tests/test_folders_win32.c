/* Where the launcher keeps its files on Windows (platform/win32/folders.c):
 * %LOCALAPPDATA%\DXLauncher's config, state and cache, Saved Games\
 * DXLauncher the player's own; and DXL_HOME's folders, as on every OS, its
 * '\' made '/'. */
#include "test.h"
#include "core/xdg.h"
#include "platform/os.h"

#include <windows.h>
#include <shlobj.h>

static char *known(REFKNOWNFOLDERID id) {
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(id, KF_FLAG_DEFAULT, NULL, &w) == S_OK) {
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
    char *local = known(&FOLDERID_LocalAppData), *saved = known(&FOLDERID_SavedGames);
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
    RUN(test_scratch_home);
TEST_MAIN_END
