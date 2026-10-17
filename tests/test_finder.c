#include "test.h"
#include "scratch.h"
#include "core/finder.h"
#include "core/paths.h"

/* An install's files under home/rel: a DeusEx.exe the engine does not know,
 * which the finder lists all the same. */
static void install_at(const char *home, const char *rel) {
    static const char *files[] = {
        "System/DeusEx.exe", "System/Core.u", "System/Engine.u", "System/Extension.u", "System/DeusEx.u",
        "System/DeusExUI.u", "System/Default.ini", "System/DefUser.ini", "Maps/DX.dx",
    };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        char path[512];
        snprintf(path, sizeof path, "%s/%s", rel, files[i]);
        scratch_write(home, path, "x");
    }
}

static void test_nothing(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-empty"));
    dxl_found_list l;
    dxl_finder_quick(&l, home, NULL, NULL);
    CHECK_INT(l.count, 0);
    dxl_found_list_free(&l);
    scratch_remove(home);
    free(home);
}

/* Steam's library natively and as a Flatpak, Heroic's and GOG's folders;
 * Steam's two ways to its own folder lead to one entry. */
static void test_places(void) {
    char *home = dxl_xstrdup(scratch_dir("finder-places"));
    install_at(home, ".local/share/Steam/steamapps/common/Deus Ex");
    install_at(home, ".var/app/com.valvesoftware.Steam/.local/share/Steam/steamapps/common/Deus Ex");
    install_at(home, "Games/Heroic/Deus Ex GOTY");
    install_at(home, "GOG Games/Deus Ex");
    scratch_write(home, "Games/Heroic/Some Other Game/readme.txt", "x");
    char *link_dir = dxl_path_join(home, ".steam");
    dxl_path_mkdirs(link_dir);
    char *target = dxl_path_join(home, ".local/share/Steam");
    char *link = dxl_path_join(link_dir, "steam");
    CHECK_INT(symlink(target, link), 0);

    dxl_found_list l;
    dxl_finder_quick(&l, home, NULL, NULL);
    CHECK_INT(l.count, 4);
    CHECK_STR(l.items[0].source, "in Steam's library");
    CHECK(strstr(l.items[0].install.root, ".local/share/Steam/steamapps/common/Deus Ex") != NULL);
    CHECK_STR(l.items[1].source, "in Steam's library, Steam's Flatpak");
    CHECK_STR(l.items[2].source, "in Heroic's games");
    CHECK(strstr(l.items[2].install.root, "Deus Ex GOTY") != NULL);
    CHECK_STR(l.items[3].source, "in GOG Games");
    for (int i = 0; i < l.count; i++) {
        CHECK_INT(l.items[i].install.found, 1);
        CHECK_INT(l.items[i].install.ok, 0);   /* not a build the engine knows */
    }
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
    dxl_finder_quick(&l, home, steam, known);
    CHECK_INT(l.count, 2);
    CHECK_STR(l.items[0].source, "the folder given");
    CHECK_STR(l.items[1].source, "played before");
    CHECK(strstr(l.items[1].install.root, "elsewhere/DX") != NULL);
    dxl_found_list_free(&l);

    dxl_finder_quick(&l, home, none, NULL);
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

TEST_MAIN_BEGIN
    RUN(test_nothing);
    RUN(test_places);
    RUN(test_given_and_known);
TEST_MAIN_END
