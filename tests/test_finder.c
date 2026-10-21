#include "test.h"
#include "scratch.h"
#include "core/finder.h"
#include "core/paths.h"

#include <unistd.h>

/* An install's files under base/rel: a DeusEx.exe the engine does not know,
 * which the finder lists all the same. */
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

static void find_in(dxl_found_list *l, const char *home) {
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = home;
    dxl_finder_find(l, &w);
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

static void test_nothing(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-empty"));
    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 0);
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

/* Steam's own library natively and as a Flatpak, Heroic's, GOG's and the
 * Games folders; Steam's two ways to its own folder lead to one entry. */
static void test_places(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-places"));
    install_at(home, ".local/share/Steam/steamapps/common/Deus Ex");
    install_at(home, ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Deus Ex");
    install_at(home, "Games/Heroic/Deus Ex GOTY");
    install_at(home, "GOG Games/Deus Ex");
    install_at(home, "Games/Deus Ex");
    scratch_write(home, "Games/Heroic/Some Other Game/readme.txt", "x");
    char *link_dir = dxl_path_join(home, ".steam");
    dxl_path_mkdirs(link_dir);
    char *target = dxl_path_join(home, ".local/share/Steam");
    char *link = dxl_path_join(link_dir, "steam");
    CHECK_INT(symlink(target, link), 0);

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 5);
    check_item(&l, 0, "in Steam's library", ".local/share/Steam/steamapps/common/Deus Ex");
    check_item(&l, 1, "in Steam's library, Steam's Flatpak", "Steam/steamapps/common/Deus Ex");
    check_item(&l, 2, "in Heroic's games", "Games/Heroic/Deus Ex GOTY");
    check_item(&l, 3, "in GOG Games", "GOG Games/Deus Ex");
    check_item(&l, 4, "in Games", "Games/Deus Ex");
    for (int i = 0; i < l.count; i++) CHECK_INT(l.items[i].install.ok, 0);   /* not a build the engine knows */
    dxl_found_list_free(&l);
    free(link);
    free(target);
    free(link_dir);
    scratch_remove(home);
    free(home);
}

/* The folder given comes first, and always, with what is wrong with it;
 * launcher.ini's after it; each folder once. */
static void test_given_and_known(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-given"));
    install_at(home, ".local/share/Steam/steamapps/common/Deus Ex");
    install_at(home, "elsewhere/DX");
    char *steam = dxl_path_join(home, ".local/share/Steam/steamapps/common/Deus Ex");
    char *other = dxl_path_join(home, "elsewhere/DX");
    char *none = dxl_path_join(home, "not-there");

    dxl_found_list l;
    const char *known[] = { other, steam, NULL };
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = home;
    w.given = steam;
    w.known = known;
    dxl_finder_find(&l, &w);
    CHECK_INT(l.count, 2);
    CHECK_STR(l.items[0].source, "the folder given");
    check_item(&l, 1, "played before", "elsewhere/DX");
    dxl_found_list_free(&l);

    w.given = none;
    w.known = NULL;
    dxl_finder_find(&l, &w);
    CHECK_INT(l.count, 2);
    CHECK_STR(l.items[0].source, "the folder given");
    CHECK_INT(l.items[0].install.found, 0);
    CHECK(strstr(l.items[0].install.problem, "no System/DeusEx.exe") != NULL);
    dxl_found_list_free(&l);

    free(none);
    free(other);
    free(steam);
    scratch_remove(home);
    free(home);
}

/* Each library libraryfolders.vdf lists, today's way and an older Steam's,
 * the game where its manifest puts it. */
static void test_steam_libraries(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-steam"));
    char *lib2 = dxl_path_join(home, "drive2/SteamLibrary");
    char *lib3 = dxl_path_join(home, "drive3/Steam Library");
    char text[2048];
    snprintf(text, sizeof text,
             "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"%s/.local/share/Steam\"\n\t}\n"
             "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"%s\"\n\t\t\"apps\" { \"6910\" \"1\" }\n\t}\n}\n", home, lib2);
    scratch_write(home, ".local/share/Steam/steamapps/libraryfolders.vdf", text);
    snprintf(text, sizeof text, "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\"1\"\n\t\"1\"\t\"%s\"\n}\n", lib3);
    scratch_write(home, ".local/share/Steam/config/libraryfolders.vdf", text);
    scratch_write(home, "drive2/SteamLibrary/steamapps/appmanifest_6910.acf",
                  "\"AppState\"\n{\n\t\"appid\"\t\"6910\"\n\t\"installdir\"\t\"Deus Ex GOTY\"\n}\n");
    install_at(home, "drive2/SteamLibrary/steamapps/common/Deus Ex GOTY");
    install_at(home, "drive3/Steam Library/steamapps/common/Deus Ex");

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 2);
    check_item(&l, 0, "in Steam's library", "drive2/SteamLibrary/steamapps/common/Deus Ex GOTY");
    check_item(&l, 1, "in Steam's library", "drive3/Steam Library/steamapps/common/Deus Ex");
    dxl_found_list_free(&l);
    free(lib3);
    free(lib2);
    scratch_remove(home);
    free(home);
}

/* Heroic's GOG games, natively and as a Flatpak: installed.json's paths,
 * those that hold the game. */
static void test_heroic(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-heroic"));
    char text[2048];
    snprintf(text, sizeof text,
             "{\"installed\": [{\"platform\": \"windows\", \"install_path\": \"%s/GOG/Deus Ex GOTY\", "
             "\"appName\": \"1207658995\"}, {\"install_path\": \"%s/GOG/Another Game\", \"appName\": \"1\"}]}",
             home, home);
    scratch_write(home, ".config/heroic/gog_store/installed.json", text);
    snprintf(text, sizeof text, "{\"installed\": [{\"install_path\": \"%s/Flat/Deus Ex\"}]}", home);
    scratch_write(home, ".var/app/com.heroicgameslauncher.hgl/config/heroic/gog_store/installed.json", text);
    install_at(home, "GOG/Deus Ex GOTY");
    scratch_write(home, "GOG/Another Game/game.exe", "x");
    install_at(home, "Flat/Deus Ex");

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 2);
    check_item(&l, 0, "in Heroic's games", "GOG/Deus Ex GOTY");
    check_item(&l, 1, "in Heroic's games", "Flat/Deus Ex");
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

/* Lutris' games: the game a game's exe is in, or beside it, and the game in
 * its prefix; an exe of another section, or a relative one, is not read. */
static void test_lutris(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-lutris"));
    char text[2048];
    snprintf(text, sizeof text,
             "# Lutris\ngame:\n  exe: '%s/lutris games/Deus Ex''s/System/DeusEx.exe'\n  prefix: %s/pfx  # its prefix\n"
             "system: {}\nwine:\n  exe: %s/not/System/DeusEx.exe\n  version: lutris-7.2\n",
             home, home, home);
    scratch_write(home, ".config/lutris/games/deus-ex-1700000000.yml", text);
    snprintf(text, sizeof text, "game:\n  exe: \"%s/beside/DeusEx.exe\"\nsystem:\n  exe: relative/System/DeusEx.exe\n", home);
    scratch_write(home, ".local/share/lutris/games/deus-ex-2.yml", text);
    /* As PyYAML writes a long one: folded onto the lines under it; and one
     * with an e acute, in double quotes, escaped, its line's end too. */
    snprintf(text, sizeof text,
             "game:\n  exe: %s/a folder whose name is long enough that PyYAML folds\n    it/Deus Ex/System/DeusEx.exe\n"
             "  prefix: \"%s/pr\\xE9\\\n    fixe\"\nsystem: {}\n", home, home);
    scratch_write(home, ".local/share/lutris/games/deus-ex-3.yml", text);
    scratch_write(home, ".local/share/lutris/games/notes.txt", "game:\n  exe: /nowhere\n");
    install_at(home, "lutris games/Deus Ex's");
    install_at(home, "pfx/drive_c/GOG Games/Deus Ex GOTY");
    install_at(home, "not");
    install_at(home, "beside");
    install_at(home, "a folder whose name is long enough that PyYAML folds it/Deus Ex");
    install_at(home, "pr\xc3\xa9" "fixe/drive_c/DeusEx");

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 5);
    check_item(&l, 0, "in Lutris' games", "lutris games/Deus Ex's");
    check_item(&l, 1, "in Lutris' games", "pfx/drive_c/GOG Games/Deus Ex GOTY");
    check_item(&l, 2, "in Lutris' games", "/beside");
    check_item(&l, 3, "in Lutris' games", "PyYAML folds it/Deus Ex");
    check_item(&l, 4, "in Lutris' games", "/pr\xc3\xa9" "fixe/drive_c/DeusEx");
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

/* Bottles' bottles and the other Wine prefixes, each where Windows'
 * installers put the game, in a drive_c's folders of any case. */
static void test_prefixes(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-prefixes"));
    install_at(home, ".local/share/bottles/bottles/Gaming/drive_c/Program Files (x86)/GOG Galaxy/Games/Deus Ex");
    install_at(home, "wp/drive_c/program files (x86)/steam/steamapps/common/Deus Ex");
    install_at(home, ".wine/drive_c/DeusEx");
    install_at(home, ".local/share/wineprefixes/dx/drive_c/GOG Games/Deus Ex");
    install_at(home, "Games/deus-ex/drive_c/Games/Deus Ex");
    install_at(home, "Games/Heroic/Prefixes/default/Deus Ex/drive_c/GOG Games/DX");
    char *wp = dxl_path_join(home, "wp");

    dxl_found_list l;
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = home;
    w.wineprefix = wp;
    dxl_finder_find(&l, &w);
    CHECK_INT(l.count, 6);
    check_item(&l, 0, "in Bottles", "Gaming/drive_c/Program Files (x86)/GOG Galaxy/Games/Deus Ex");
    check_item(&l, 1, "in a Wine prefix", "wp/drive_c/program files (x86)/steam/steamapps/common/Deus Ex");
    check_item(&l, 2, "in a Wine prefix", ".wine/drive_c/DeusEx");
    check_item(&l, 3, "in a Wine prefix", "wineprefixes/dx/drive_c/GOG Games/Deus Ex");
    check_item(&l, 4, "in a Wine prefix", "Games/deus-ex/drive_c/Games/Deus Ex");
    check_item(&l, 5, "in a Wine prefix", "Prefixes/default/Deus Ex/drive_c/GOG Games/DX");
    dxl_found_list_free(&l);
    free(wp);
    scratch_remove(home);
    free(home);
}

/* A Windows Steam in a Wine prefix: the libraries its libraryfolders.vdf
 * lists as Windows paths, found where the prefix's dosdevices lead, in any
 * case; a UNC path passed over. */
static void test_prefix_steam(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-winsteam"));
    install_at(home, "disk-d/games/steamlib/steamapps/common/Deus Ex");
    scratch_write(home, ".wine/drive_c/Program Files (x86)/Steam/steamapps/libraryfolders.vdf",
                  "\"libraryfolders\"\n{\n"
                  "\t\"0\"\n\t{\n\t\t\"path\"\t\t\"C:\\\\Program Files (x86)\\\\Steam\"\n\t}\n"
                  "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"\\\\\\\\server\\\\share\\\\lib\"\n\t}\n"
                  "\t\"2\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\Games\\\\SteamLib\"\n\t}\n}\n");
    char *devices = scratch_path(home, ".wine/dosdevices/c:");
    CHECK_INT(symlink("../drive_c", devices), 0);
    free(devices);
    char *disk = dxl_path_join(home, "disk-d");
    devices = scratch_path(home, ".wine/dosdevices/d:");
    CHECK_INT(symlink(disk, devices), 0);
    free(devices);
    free(disk);

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, 1);
    check_item(&l, 0, "in a Wine prefix", "disk-d/games/steamlib/steamapps/common/Deus Ex");
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

/* The drives mountinfo has mounted in the folders for them, however deep
 * -- a user's, the automounter's /run/media/system -- at a drive's top and
 * two folders down, where Windows' installers put the game, and in a
 * handheld card's Roms/PORTS, three down, in its own letter case; a folder
 * of them that links to another, and a drive mounted twice, looked in
 * once; what is mounted elsewhere, not at all. */
static void test_drives(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-drives"));
    install_at(home, "run-media/me/USB/Games/Deus Ex");
    install_at(home, "run-media/me/USB2/Deep/Er/Deus Ex");    /* three down: not looked for */
    install_at(home, "run-media/me/My Disk/DeusEx");
    install_at(home, "run-media/SD/DeusEx");
    install_at(home, "run-media/system/Win/GOG Games/Deus Ex B");
    install_at(home, "run-media/system/Win/SteamLibrary/steamapps/common/Deus Ex");
    install_at(home, "mnt/win/Program Files (x86)/Steam/steamapps/common/Deus Ex");
    install_at(home, "mnt/SDCARD/Roms/ports/DeusEx");
    install_at(home, "elsewhere/disk/DeusEx");                  /* mounted, but not for drives */
    char *run = dxl_path_join(home, "run-media"), *media_link = dxl_path_join(home, "media"),
         *mnt = dxl_path_join(home, "mnt"), *empty = dxl_path_join(home, "home");
    dxl_path_mkdirs(empty);
    CHECK_INT(symlink(run, media_link), 0);
    const char *media[] = { run, media_link, mnt, NULL };
    char mounts[4096];
    snprintf(mounts, sizeof mounts,
             "22 1 0:21 / /proc rw,nosuid shared:13 - proc proc rw\n"
             "41 29 8:17 / %s/run-media/me/USB rw,relatime shared:1 - vfat /dev/sdb1 rw\n"
             "42 29 8:18 / %s/run-media/me/USB2 rw shared:2 - ext4 /dev/sdb2 rw\n"
             "43 29 8:19 / %s/run-media/me/My\\040Disk rw shared:3 - exfat /dev/sdb3 rw\n"
             "44 29 179:1 / %s/run-media/SD rw shared:4 - ext4 /dev/mmcblk0p1 rw\n"
             "45 29 8:3 / %s/run-media/system/Win rw shared:5 - ntfs3 /dev/sda3 rw\n"
             "46 29 8:3 / %s/run-media/system/Win rw shared:5 - ntfs3 /dev/sda3 rw\n"
             "47 29 8:4 / %s/mnt/win rw shared:6 - ntfs3 /dev/sda4 rw\n"
             "48 29 8:5 / %s/elsewhere/disk rw shared:7 - ext4 /dev/sda5 rw\n"
             "49 29 179:9 / %s/mnt/SDCARD rw shared:8 - exfat /dev/mmcblk1p1 rw\n",
             home, home, home, home, home, home, home, home, home);

    dxl_found_list l;
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = empty;
    w.media = media;
    w.mounts = mounts;
    dxl_finder_find(&l, &w);
    CHECK_INT(l.count, 7);
    check_item(&l, 0, "on a mounted drive", "run-media/me/USB/Games/Deus Ex");
    check_item(&l, 1, "on a mounted drive", "run-media/me/My Disk/DeusEx");
    check_item(&l, 2, "on a mounted drive", "run-media/SD/DeusEx");
    check_item(&l, 3, "on a mounted drive", "run-media/system/Win/GOG Games/Deus Ex B");
    check_item(&l, 4, "on a mounted drive", "run-media/system/Win/SteamLibrary/steamapps/common/Deus Ex");
    check_item(&l, 5, "on a mounted drive", "mnt/win/Program Files (x86)/Steam/steamapps/common/Deus Ex");
    check_item(&l, 6, "on a mounted drive", "mnt/SDCARD/Roms/ports/DeusEx");
    dxl_found_list_free(&l);
    free(empty);
    free(mnt);
    free(media_link);
    free(run);
    scratch_remove(home);
    free(home);
}

static void count_found(void *ctx, const dxl_found *f) {
    (void)f;
    ++*(int *)ctx;
}

/* Stopped, it lists nothing more; a full list holds the first it found. */
static void test_stop_and_full(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-full"));
    for (int i = 0; i < DXL_FOUND_MAX + 8; i++) {
        char rel[64];
        snprintf(rel, sizeof rel, "GOG Games/Deus Ex %02d", i);
        install_at(home, rel);
    }
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.home = home;
    w.given = home;
    int seen = 0;
    atomic_int stop = 1;
    dxl_finder_run(&w, count_found, &seen, &stop);
    CHECK_INT(seen, 0);

    dxl_found_list l;
    find_in(&l, home);
    CHECK_INT(l.count, DXL_FOUND_MAX);
    check_item(&l, 0, "in GOG Games", "GOG Games/Deus Ex 00");
    check_item(&l, DXL_FOUND_MAX - 1, "in GOG Games", "GOG Games/Deus Ex 31");
    dxl_found f = l.items[0];
    CHECK_INT(dxl_found_list_add(&l, &f), -1);
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_nothing);
    RUN(test_places);
    RUN(test_given_and_known);
    RUN(test_steam_libraries);
    RUN(test_heroic);
    RUN(test_lutris);
    RUN(test_prefixes);
    RUN(test_prefix_steam);
    RUN(test_drives);
    RUN(test_stop_and_full);
TEST_MAIN_END
