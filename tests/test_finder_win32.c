/* The finder's places on Windows (core/finder_places_win32.c): with a home
 * given, a test's, Heroic's games and the drives' tops given; with none,
 * this machine's own -- Steam's, GOG's and the original installer's keys,
 * read here from a key of the test's own in their place
 * (DXL_FINDER_KEYS), taken out after. */
#include "test.h"
#include "scratch.h"
#include "core/finder.h"
#include "core/paths.h"

#include <windows.h>

/* An install's files under base/rel, as the Linux finder's test makes one. */
static void install_at(const char *base, const char *rel) {
    static const char *files[] = {
        "System/DeusEx.exe", "System/Core.u", "System/Engine.u", "System/Extension.u", "System/DeusEx.u",
        "System/DeusExUI.u", "System/Default.ini", "System/DefUser.ini", "Maps/DX.dx",
    };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", rel, files[i]);
        scratch_write(base, path, "x");
    }
}

/* The list's i-th: found there, under that path. */
static void check_item(const dxl_found_list *l, int i, const char *source, const char *path_end) {
    if (i >= l->count) {
        fprintf(stderr, "  FAIL in %s: no item %d (of %d)\n", dxl_test_current, i, l->count);
        dxl_test_failures++;
        return;
    }
    CHECK_STR(l->items[i].source, source);
    const char *root = l->items[i].install.root;
    size_t n = strlen(root), m = strlen(path_end);
    if (n < m || strcmp(root + n - m, path_end) != 0) {
        fprintf(stderr, "  FAIL in %s: item %d is %s, not ...%s\n", dxl_test_current, i, root, path_end);
        dxl_test_failures++;
    }
    CHECK_INT(l->items[i].install.found, 1);
}

/* path with Windows' separators, each doubled as a JSON or VDF string has
 * a backslash. */
static void escaped(const char *path, char *out, size_t size) {
    size_t n = 0;
    for (const char *p = path; *p && n + 3 < size; p++) {
        if (*p == '/' || *p == '\\') {
            out[n++] = '\\';
            out[n++] = '\\';
        } else {
            out[n++] = *p;
        }
    }
    out[n] = '\0';
}

/* Heroic's list (%APPDATA%'s, here the config folder given), its own games
 * folder, and the drives' places Windows' installers use. */
static void test_given(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-given"));
    install_at(home, "Heroic GOG/Deus Ex");
    install_at(home, "Games/Heroic/Deus Ex GOTY");
    install_at(home, "drive/Program Files (x86)/Steam/steamapps/common/Deus Ex");
    install_at(home, "drive/GOG Games/Deus Ex");
    char *at = scratch_path(home, "Heroic GOG/Deus Ex");
    char path[1200], json[1400];
    escaped(at, path, sizeof path);
    snprintf(json, sizeof json, "{\"installed\":[{\"install_path\":\"%s\"}]}", path);
    scratch_write(home, "config/heroic/gog_store/installed.json", json);
    char *config = scratch_path(home, "config"), *drive = scratch_path(home, "drive");
    const char *media[] = { drive, NULL };
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = home;
    w.config_home = config;
    w.media = media;
    dxl_found_list l;
    dxl_finder_find(&l, &w);
    CHECK_INT(l.count, 4);
    check_item(&l, 0, "in Heroic's games", "Heroic GOG/Deus Ex");
    check_item(&l, 1, "in Heroic's games", "Games/Heroic/Deus Ex GOTY");
    check_item(&l, 2, "on a drive", "Program Files (x86)/Steam/steamapps/common/Deus Ex");
    check_item(&l, 3, "on a drive", "GOG Games/Deus Ex");
    dxl_found_list_free(&l);
    free(at);
    free(config);
    free(drive);
    scratch_remove(home);
    free(home);
}

#define KEYS L"Software\\DXLauncher test"

static int key_there(const wchar_t *sub) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    RegCloseKey(k);
    return 1;
}

static void set_string(const wchar_t *sub, const wchar_t *name, const char *value) {
    wchar_t w[1024];
    MultiByteToWideChar(CP_UTF8, 0, value, -1, w, 1024);
    CHECK_INT((int)RegSetKeyValueW(HKEY_CURRENT_USER, sub, name, REG_SZ, w, (DWORD)((wcslen(w) + 1) * sizeof *w)),
              ERROR_SUCCESS);
}

/* This machine's own places, read from the test's key in the registry's
 * place (DXL_FINDER_KEYS): the Steam it names, its own library and the one
 * its libraryfolders.vdf lists by a Windows path; a GOG game; where the
 * original installer put the game. */
static void test_registry(void) {
    dxl_env_set("DXL_FINDER_KEYS", "Software\\DXLauncher test");
    char *dir = dxl_xstrdup(scratch_dir("finder-registry"));
    install_at(dir, "Steam/steamapps/common/Deus Ex");
    install_at(dir, "Library/steamapps/common/Deus Ex");
    install_at(dir, "GOG/Deus Ex GOTY");
    install_at(dir, "CD/DeusEx");
    char *steam = scratch_path(dir, "Steam"), *library = scratch_path(dir, "Library");
    char *gog = scratch_path(dir, "GOG/Deus Ex GOTY"), *cd = scratch_path(dir, "CD/DeusEx");
    char own[1200], other[1200], vdf[3000];
    escaped(steam, own, sizeof own);
    escaped(library, other, sizeof other);
    snprintf(vdf, sizeof vdf,
             "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"%s\"\n\t}\n\t\"1\"\n\t{\n\t\t\"path\"\t\t\"%s\"\n\t}\n}\n",
             own, other);
    scratch_write(dir, "Steam/steamapps/libraryfolders.vdf", vdf);
    /* Each value as its own writes it: Steam's with '/', the others'
     * with Windows' '\\'. */
    set_string(KEYS L"\\Valve\\Steam", L"SteamPath", steam);
    for (char *p = gog; *p; p++)
        if (*p == '/') *p = '\\';
    for (char *p = cd; *p; p++)
        if (*p == '/') *p = '\\';
    set_string(KEYS L"\\GOG.com\\Games\\1207658801", L"path", gog);
    set_string(KEYS L"\\Unreal Technology\\Installed Apps\\Deus Ex", L"Folder", cd);

    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    dxl_found_list l;
    dxl_finder_find(&l, &w);
    CHECK(l.count >= 4);
    check_item(&l, 0, "in Steam's library", "Steam/steamapps/common/Deus Ex");
    check_item(&l, 1, "in Steam's library", "Library/steamapps/common/Deus Ex");
    check_item(&l, 2, "in GOG's games", "GOG/Deus Ex GOTY");
    check_item(&l, 3, "where its installer put it", "CD/DeusEx");
    dxl_found_list_free(&l);

    RegDeleteTreeW(HKEY_CURRENT_USER, KEYS);
    CHECK_INT(key_there(KEYS), 0);
    dxl_env_set("DXL_FINDER_KEYS", NULL);
    free(steam);
    free(library);
    free(gog);
    free(cd);
    scratch_remove(dir);
    free(dir);
}

TEST_MAIN_BEGIN
    RUN(test_given);
    RUN(test_registry);
TEST_MAIN_END
