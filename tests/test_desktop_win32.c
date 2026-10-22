/* The launcher's Start menu entry on Windows (platform/win32/desktop.c):
 * its two shortcuts in the folder DXL_START_MENU gives here, their icon
 * made of the window's PNGs, and the deusex:// links' handler in the
 * user's hive -- the test's Wine prefix's -- each gone again. */
#include "test.h"
#include "scratch.h"
#include "core/paths.h"
#include "platform/desktop.h"
#include "platform/os.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>

#define SCHEME_COMMAND L"Software\\Classes\\deusex\\shell\\open\\command"

/* A shortcut's target and its words, as the shell reads them back, '/'
 * between the target's names. 0, or -1. */
static int read_link(const char *path, char *target, size_t tsize, char *args, size_t asize) {
    IShellLinkW *link = NULL;
    IPersistFile *file = NULL;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link)))
        return -1;
    wchar_t w[1024], t[1024] = L"", a[1024] = L"";
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w, 1024);
    HRESULT hr = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&file);
    if (SUCCEEDED(hr)) hr = file->lpVtbl->Load(file, w, STGM_READ);
    if (SUCCEEDED(hr)) hr = link->lpVtbl->GetPath(link, t, 1024, NULL, 0);
    if (SUCCEEDED(hr)) hr = link->lpVtbl->GetArguments(link, a, 1024);
    if (file) file->lpVtbl->Release(file);
    link->lpVtbl->Release(link);
    if (FAILED(hr)) return -1;
    WideCharToMultiByte(CP_UTF8, 0, t, -1, target, (int)tsize, NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, a, -1, args, (int)asize, NULL, NULL);
    dxl_path_normalize(target);
    return 0;
}

/* The deusex:// handler's command, or "" when there is none. */
static void scheme_command(char *out, size_t size) {
    wchar_t v[1024];
    DWORD n = sizeof v;
    out[0] = '\0';
    if (RegGetValueW(HKEY_CURRENT_USER, SCHEME_COMMAND, NULL, RRF_RT_REG_SZ, NULL, v, &n) == ERROR_SUCCESS)
        WideCharToMultiByte(CP_UTF8, 0, v, -1, out, (int)size, NULL, NULL);
}

/* A shortcut's target is quoted where a command line would split it. */
static void test_exec(void) {
    char *e = dxl_desktop_exec("C:/Games/DXLauncher/deusex-launcher.exe");
    CHECK_STR(e, "C:/Games/DXLauncher/deusex-launcher.exe");
    free(e);
    e = dxl_desktop_exec("C:/Program Files/DXLauncher/deusex-launcher.exe");
    CHECK_STR(e, "\"C:/Program Files/DXLauncher/deusex-launcher.exe\"");
    free(e);
}

static void test_install(void) {
    char *dir = dxl_xstrdup(scratch_dir("desktop"));
    /* The icon's PNGs as the window leaves them: an icon file takes them
     * as they are, any bytes. */
    scratch_write(dir, "home/icons/16.png", "png16");
    scratch_write(dir, "home/icons/256.png", "png256");
    scratch_write(dir, "menu/readme.txt", "");
    scratch_write(dir, "app/deusex-launcher.exe", "x");
    char *home = scratch_path(dir, "home"), *menu = scratch_path(dir, "menu");
    char *program = scratch_path(dir, "app/deusex-launcher.exe");
    dxl_env_set("DXL_START_MENU", menu);
    char command[1200];
    scheme_command(command, sizeof command);
    CHECK_STR(command, "");

    dxl_err err;
    CHECK_INT(dxl_desktop_installed(home), 0);
    CHECK_INT(dxl_desktop_install(home, NULL, program, &err), 0);
    CHECK_INT(dxl_desktop_installed(home), 1);
    char *entry = dxl_desktop_entry_path(home);
    char *big = dxl_path_join(menu, "DXLauncher (big screen).lnk");
    CHECK(strstr(entry, "/menu/DXLauncher.lnk") != NULL);
    char target[1024], args[256];
    CHECK_INT(read_link(entry, target, sizeof target, args, sizeof args), 0);
    CHECK(_stricmp(target, program) == 0);
    CHECK_STR(args, "");
    CHECK_INT(read_link(big, target, sizeof target, args, sizeof args), 0);
    CHECK(_stricmp(target, program) == 0);
    CHECK_STR(args, "--big-screen");

    /* The icon: its header, an entry for each PNG, the PNGs. */
    char *ico = scratch_path(dir, "home/DXLauncher.ico");
    size_t len = 0;
    char *bytes = slurp(ico, &len);
    CHECK_INT((int)len, 6 + 2 * 16 + 5 + 6);
    CHECK(bytes && bytes[2] == 1 && bytes[4] == 2 && !memcmp(bytes + len - 11, "png16png256", 11));
    free(bytes);

    /* deusex:// links opened by the launcher. */
    char want[1200];
    snprintf(want, sizeof want, "\"%s\" \"%%1\"", program);
    for (char *p = want; *p; p++)
        if (*p == '/') *p = '\\';
    scheme_command(command, sizeof command);
    CHECK_STR(command, want);

    CHECK_INT(dxl_desktop_remove(home, &err), 0);
    CHECK_INT(dxl_desktop_installed(home), 0);
    CHECK_INT(dxl_path_exists(big), 0);
    CHECK_INT(dxl_path_exists(ico), 0);
    CHECK(!scratch_exists(dir, "home/icons/16.png"));
    scheme_command(command, sizeof command);
    CHECK_STR(command, "");
    CHECK_INT(dxl_desktop_remove(home, &err), 0);   /* nothing to remove: fine */

    dxl_env_set("DXL_START_MENU", NULL);
    free(ico);
    free(entry);
    free(big);
    free(program);
    free(menu);
    free(home);
    scratch_remove(dir);
    free(dir);
}

TEST_MAIN_BEGIN
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    RUN(test_exec);
    RUN(test_install);
    CoUninitialize();
TEST_MAIN_END
