/* The launcher's entry in Windows' Start menu (platform/desktop.h): two
 * shortcuts in the user's Programs folder, DXLauncher.lnk and
 * DXLauncher (big screen).lnk, the second with --big-screen; their icon, a
 * DXLauncher.ico under data_home made of the PNGs ui/icon.h draws at each
 * size, packed as the programs' own is (core/ico.h); and the deusex:// links'
 * handler, the launcher, under HKEY_CURRENT_USER\Software\Classes\deusex.
 * The template is the desktop's alone: Windows has none.
 */
#include "platform/desktop.h"
#include "platform/win32/internal.h"
#include "core/common.h"
#include "core/ico.h"
#include "core/paths.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int dxl_desktop_icon_sizes[] = { DXL_ICO_SIZES };
const int dxl_desktop_icon_count = (int)(sizeof dxl_desktop_icon_sizes / sizeof *dxl_desktop_icon_sizes);

#define SCHEME_KEY L"Software\\Classes\\deusex"

/* The Start menu's Programs folder, or the folder under data_home a test
 * gives it (DXL_START_MENU). Caller frees. */
static char *programs(void) {
    const char *t = getenv("DXL_START_MENU");
    if (t && *t) return dxl_xstrdup(t);
    PWSTR w = NULL;
    char *out = NULL;
    if (SHGetKnownFolderPath(&FOLDERID_Programs, KF_FLAG_CREATE, NULL, &w) == S_OK) out = dxl_u8(w, 1);
    CoTaskMemFree(w);
    return out;
}

static char *shortcut(const char *leaf) {
    char *dir = programs();
    char *p = dir ? dxl_path_join(dir, leaf) : NULL;
    free(dir);
    return p;
}

char *dxl_desktop_entry_path(const char *data_home) {
    (void)data_home;
    char *p = shortcut("DXLauncher.lnk");
    return p ? p : dxl_xstrdup("DXLauncher.lnk");
}

char *dxl_desktop_icon_path(const char *data_home, int px) {
    char leaf[64];
    snprintf(leaf, sizeof leaf, "icons/%d.png", px);
    return dxl_path_join(data_home, leaf);
}

/* A shortcut's target as a command line takes it: quoted where it needs. */
char *dxl_desktop_exec(const char *program) {
    size_t n = strlen(program) + 3;
    char *out = dxl_xmalloc(n);
    snprintf(out, n, strpbrk(program, " \t") ? "\"%s\"" : "%s", program);
    return out;
}

/* The PNGs as one icon file (core/ico.h), those there: 0, or -1. */
static int write_ico(const char *data_home, const char *ico, dxl_err *err) {
    dxl_buf pngs[8];
    dxl_ico_image images[8];
    int count = 0;
    for (int i = 0; i < dxl_desktop_icon_count && count < 8; i++) {
        char *png = dxl_desktop_icon_path(data_home, dxl_desktop_icon_sizes[i]);
        FILE *f = fopen(png, "rb");
        free(png);
        if (!f) continue;
        dxl_buf *b = &pngs[count];
        dxl_buf_init(b);
        char chunk[16384];
        size_t got;
        while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(b, chunk, got);
        fclose(f);
        images[count] = (dxl_ico_image){ dxl_desktop_icon_sizes[i], b->data, b->len };
        count++;
    }
    dxl_buf out;
    dxl_buf_init(&out);
    int rc = -1;
    if (!count) dxl_err_set(err, "no icon drawn under %s", data_home);
    else if (dxl_ico_pack(images, count, &out) != 0) dxl_err_set(err, "the icon under %s cannot be packed", data_home);
    else rc = dxl_path_write_atomic(ico, out.data, out.len, err);
    for (int i = 0; i < count; i++) dxl_buf_free(&pngs[i]);
    dxl_buf_free(&out);
    return rc;
}

/* One shortcut to program, with args, its icon ico. */
static int make_link(const char *path, const char *program, const wchar_t *args, const char *ico, dxl_err *err) {
    IShellLinkW *link = NULL;
    IPersistFile *file = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
    if (FAILED(hr)) {
        dxl_err_set(err, "no shell link here (0x%08lX)", (unsigned long)hr);
        return -1;
    }
    wchar_t *target = dxl_w(program, 1), *icon = dxl_w(ico, 1), *where = dxl_w(path, 1);
    char *dir = dxl_path_dirname(program);
    wchar_t *wdir = dxl_w(dir, 1);
    link->lpVtbl->SetPath(link, target);
    link->lpVtbl->SetArguments(link, args);
    link->lpVtbl->SetWorkingDirectory(link, wdir);
    link->lpVtbl->SetIconLocation(link, icon, 0);
    link->lpVtbl->SetDescription(link, L"Deus Ex, by DXLauncher");
    hr = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&file);
    if (SUCCEEDED(hr)) {
        hr = file->lpVtbl->Save(file, where, TRUE);
        file->lpVtbl->Release(file);
    }
    link->lpVtbl->Release(link);
    if (FAILED(hr)) dxl_err_set(err, "cannot write %s (0x%08lX)", path, (unsigned long)hr);
    free(target);
    free(icon);
    free(where);
    free(wdir);
    free(dir);
    return FAILED(hr) ? -1 : 0;
}

static LONG set_value(HKEY key, const wchar_t *sub, const wchar_t *name, const wchar_t *value) {
    HKEY k;
    LONG rc = RegCreateKeyExW(key, sub, 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL);
    if (rc != ERROR_SUCCESS) return rc;
    rc = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)value, (DWORD)((wcslen(value) + 1) * sizeof *value));
    RegCloseKey(k);
    return rc;
}

/* deusex:// opened by program: "<program>" "%1". */
static int register_scheme(const char *program, const char *ico, dxl_err *err) {
    wchar_t *p = dxl_w(program, 1), *i = dxl_w(ico, 1);
    size_t n = wcslen(p) + 16;
    wchar_t *command = dxl_xmalloc(n * sizeof *command);
    swprintf(command, n, L"\"%ls\" \"%%1\"", p);
    LONG rc = set_value(HKEY_CURRENT_USER, SCHEME_KEY, NULL, L"URL:Deus Ex");
    if (rc == ERROR_SUCCESS) rc = set_value(HKEY_CURRENT_USER, SCHEME_KEY, L"URL Protocol", L"");
    if (rc == ERROR_SUCCESS) rc = set_value(HKEY_CURRENT_USER, SCHEME_KEY L"\\DefaultIcon", NULL, i);
    if (rc == ERROR_SUCCESS) rc = set_value(HKEY_CURRENT_USER, SCHEME_KEY L"\\shell\\open\\command", NULL, command);
    free(command);
    free(p);
    free(i);
    if (rc != ERROR_SUCCESS) {
        char why[256];
        dxl_err_set(err, "cannot register deusex:// links: %s", dxl_win_error((DWORD)rc, why, sizeof why));
        return -1;
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

int dxl_desktop_install(const char *data_home, const char *template_path, const char *program, dxl_err *err) {
    (void)template_path;
    char *ico = dxl_path_join(data_home, "DXLauncher.ico");
    char *main_link = shortcut("DXLauncher.lnk"), *big_link = shortcut("DXLauncher (big screen).lnk");
    HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    int rc = -1;
    if (!main_link || !big_link) {
        dxl_err_set(err, "no Start menu folder for this user");
    } else if (write_ico(data_home, ico, err) == 0 && make_link(main_link, program, L"", ico, err) == 0 &&
               make_link(big_link, program, L"--big-screen", ico, err) == 0 &&
               register_scheme(program, ico, err) == 0) {
        rc = 0;
    }
    if (SUCCEEDED(co)) CoUninitialize();
    free(ico);
    free(main_link);
    free(big_link);
    return rc;
}

static void remove_file(const char *path) {
    if (!path) return;
    wchar_t *w = dxl_w(path, 1);
    DeleteFileW(w);
    free(w);
}

int dxl_desktop_remove(const char *data_home, dxl_err *err) {
    (void)err;
    char *main_link = shortcut("DXLauncher.lnk"), *big_link = shortcut("DXLauncher (big screen).lnk");
    remove_file(main_link);
    remove_file(big_link);
    free(main_link);
    free(big_link);
    char *ico = dxl_path_join(data_home, "DXLauncher.ico");
    remove_file(ico);
    free(ico);
    for (int i = 0; i < dxl_desktop_icon_count; i++) {
        char *png = dxl_desktop_icon_path(data_home, dxl_desktop_icon_sizes[i]);
        remove_file(png);
        free(png);
    }
    RegDeleteTreeW(HKEY_CURRENT_USER, SCHEME_KEY);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    return 0;
}

int dxl_desktop_installed(const char *data_home, const char *program) {
    char *p = dxl_desktop_entry_path(data_home);
    if (!dxl_path_exists(p)) {
        free(p);
        return DXL_DESKTOP_NONE;
    }
    /* The shortcut's target, as the shell reads it back: program's, the
     * letter case aside, or another's. */
    int state = DXL_DESKTOP_OTHER;
    HRESULT co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IShellLinkW *link = NULL;
    IPersistFile *file = NULL;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link))) {
        wchar_t *where = dxl_w(p, 1), *want = dxl_w(program, 1), target[1024] = L"";
        HRESULT hr = link->lpVtbl->QueryInterface(link, &IID_IPersistFile, (void **)&file);
        if (SUCCEEDED(hr)) hr = file->lpVtbl->Load(file, where, STGM_READ);
        if (SUCCEEDED(hr)) hr = link->lpVtbl->GetPath(link, target, 1024, NULL, 0);
        if (SUCCEEDED(hr) && _wcsicmp(target, want) == 0) state = DXL_DESKTOP_OURS;
        if (file) file->lpVtbl->Release(file);
        link->lpVtbl->Release(link);
        free(where);
        free(want);
    }
    if (SUCCEEDED(co)) CoUninitialize();
    free(p);
    return state;
}
