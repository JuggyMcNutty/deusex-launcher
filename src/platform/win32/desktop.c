/* The launcher's entry in Windows' Start menu (platform/desktop.h): two
 * shortcuts in the user's Programs folder, DXLauncher.lnk and
 * DXLauncher (big screen).lnk, the second with --big-screen; their icon, a
 * DXLauncher.ico under data_home made of the PNGs ui/icon.h draws at each
 * size (an icon file holds PNGs as they are); and the deusex:// links'
 * handler, the launcher, under HKEY_CURRENT_USER\Software\Classes\deusex.
 * The template is the desktop's alone: Windows has none.
 */
#include "platform/desktop.h"
#include "platform/win32/internal.h"
#include "core/common.h"
#include "core/paths.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int dxl_desktop_icon_sizes[] = { 16, 32, 48, 64, 128, 256 };
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

/* The PNGs as one icon file: its header, a directory entry each, the
 * images. 0, or -1. */
static int write_ico(const char *data_home, const char *ico, dxl_err *err) {
    dxl_buf images, head;
    dxl_buf_init(&images);
    dxl_buf_init(&head);
    int count = 0;
    unsigned char dir[16 * 8];
    for (int i = 0; i < dxl_desktop_icon_count && count < 8; i++) {
        char *png = dxl_desktop_icon_path(data_home, dxl_desktop_icon_sizes[i]);
        size_t len = 0;
        FILE *f = fopen(png, "rb");
        free(png);
        if (!f) continue;
        char chunk[16384];
        size_t got, start = images.len;
        while ((got = fread(chunk, 1, sizeof chunk, f)) > 0) dxl_buf_add(&images, chunk, got);
        fclose(f);
        len = images.len - start;
        unsigned char *e = dir + 16 * count;
        int px = dxl_desktop_icon_sizes[i];
        memset(e, 0, 16);
        e[0] = (unsigned char)(px >= 256 ? 0 : px);   /* 0 means 256 */
        e[1] = (unsigned char)(px >= 256 ? 0 : px);
        e[4] = 1;                                     /* planes */
        e[6] = 32;                                    /* bits */
        for (int b = 0; b < 4; b++) e[8 + b] = (unsigned char)(len >> (8 * b));
        e[12] = (unsigned char)start;                 /* fixed up below */
        e[13] = (unsigned char)(start >> 8);
        e[14] = (unsigned char)(start >> 16);
        e[15] = (unsigned char)(start >> 24);
        count++;
    }
    if (!count) {
        dxl_buf_free(&images);
        dxl_err_set(err, "no icon drawn under %s", data_home);
        return -1;
    }
    unsigned char header[6] = { 0, 0, 1, 0, (unsigned char)count, 0 };
    dxl_buf_add(&head, (const char *)header, 6);
    uint32_t base = 6 + 16 * (uint32_t)count;
    for (int i = 0; i < count; i++) {
        unsigned char *e = dir + 16 * i;
        uint32_t off = (uint32_t)e[12] | (uint32_t)e[13] << 8 | (uint32_t)e[14] << 16 | (uint32_t)e[15] << 24;
        off += base;
        for (int b = 0; b < 4; b++) e[12 + b] = (unsigned char)(off >> (8 * b));
        dxl_buf_add(&head, (const char *)e, 16);
    }
    dxl_buf_add(&head, images.data, images.len);
    int rc = dxl_path_write_atomic(ico, head.data, head.len, err);
    dxl_buf_free(&images);
    dxl_buf_free(&head);
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

int dxl_desktop_installed(const char *data_home) {
    char *p = dxl_desktop_entry_path(data_home);
    int there = dxl_path_exists(p);
    free(p);
    return there;
}
