#include "test.h"
#include "scratch.h"
#include "core/ini.h"
#include "core/mods.h"
#include "core/paths.h"
#include "core/profile.h"
#include "platform/os.h"
#include "platform/fs.h"

#include <unistd.h>

/* A made-up install with made-up mods: GameMod in the game's folder, its
 * ini as such a mod's names it, relative to the game's System; in the
 * launcher's mods folder OutMod, its ini written for it in the game's
 * folder, Renamed, its ini for it under the name it had there, OldName,
 * Linked, a link to Target-1.0 elsewhere, its ini naming it by both,
 * RootSave, its saves its own folder, Picky, two inis of settings, one
 * with its controls, and Bare, with none. Each file the few lines the code
 * reads. */
static char *root;
static char game[300], mods[300], system_dir[320];

static char *at(const char *rel) {
    char *p = dxl_xmalloc(strlen(root) + strlen(rel) + 2);
    sprintf(p, "%s/%s", root, rel);
    return p;
}

static void make_all(void) {
    root = dxl_xstrdup(scratch_dir("mods"));
    snprintf(game, sizeof game, "%s/game", root);
    snprintf(mods, sizeof mods, "%s/home/data/dxlauncher/mods", root);
    snprintf(system_dir, sizeof system_dir, "%s/System", game);
    scratch_write(root, "game/System/DeusEx.exe", "not the game");
    scratch_write(root, "game/System/Core.u", "pkg");
    scratch_write(root, "game/System/DeusEx.ini",
                  "[URL]\r\nClass=DeusEx.JCDentonMale\r\n\r\n[Core.System]\r\nSavePath=..\\Save\r\n"
                  "CachePath=..\\Cache\r\nPaths=..\\System\\*.u\r\nPaths=..\\Maps\\*.dx\r\n"
                  "Paths=..\\Textures\\*.utx\r\n");
    scratch_write(root, "game/System/DefUser.ini", "[Engine.Input]\r\nF1=TheGames\r\n");
    scratch_write(root, "game/System/Shot0001.bmp", "BM");
    scratch_write(root, "game/Save/Save0001/SaveInfo.dxs", "the game's");
    scratch_write(root, "game/Maps/DX.dx", "map");
    scratch_write(root, "game/Textures/CoreTexMetal.utx", "tex");

    /* GameMod: its own ini, named after it, and its controls; another ini
     * with paths, and Default.ini, which lose to it; its own saves. */
    scratch_write(root, "game/GameMod/System/GameMod.u", "pkg");
    scratch_write(root, "game/GameMod/System/GameMod.ini",
                  "[URL]\r\nClass=GameMod.ModPlayer\r\n\r\n[Core.System]\r\nSavePath=..\\GameMod\\Save\r\n"
                  "CachePath=..\\Cache\r\nPaths=..\\GameMod\\System\\*.u\r\nPaths=..\\gamemod\\MAPS\\*.dx\r\n"
                  "Paths=..\\GameMod\\Maps\\Mod_Intro.dx\r\n"
                  "Paths=..\\System\\*.u\r\nPaths=..\\Maps\\*.dx\r\nPaths=..\\HDTP\\System\\*.u\r\n"
                  "Paths=..\\System\\*.u\r\n\r\n[DeusEx.MenuScreenJoinGame]\r\n"
                  "MasterServerAddress=master0.gamespy.com\r\n");
    scratch_write(root, "game/GameMod/System/GameModUser.ini", "[Engine.Input]\r\nF1=TheMods\r\n");
    scratch_write(root, "game/GameMod/System/Another.ini", "[Core.System]\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "game/GameMod/System/Default.ini", "[Core.System]\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "game/GameMod/Maps/Mod_Intro.dx", "map");
    scratch_write(root, "game/GameMod/Save/Save0002/SaveInfo.dxs", "the mod's");
    scratch_write(root, "game/GameMod/Save/Current/Mod_Intro.dxs", "current");
    /* Not mods: a folder of maps, an empty System, and a copy of the game. */
    scratch_write(root, "game/MapPack/Maps/Extra.dx", "map");
    scratch_write(root, "game/Empty/System/Readme.txt", "none");
    scratch_write(root, "game/Copy/System/DeusEx.exe", "a copy");
    scratch_write(root, "game/Copy/System/DeusEx.u", "pkg");

    /* OutMod: no ini named after it, Default.ini its settings, DefUser.ini
     * its controls; its ini names it as it would be in the game's folder. */
    scratch_write(root, "home/data/dxlauncher/mods/OutMod/system/OutMod.U", "pkg");
    scratch_write(root, "home/data/dxlauncher/mods/OutMod/system/Default.ini",
                  "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\OutMod\\System\\*.u\r\n"
                  "Paths=..\\outmod\\TEXTURES\\*.utx\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/OutMod/system/DefUser.ini", "[Engine.Input]\r\nF1=Out\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/OutMod/Textures/OutTex.utx", "tex");
    /* Renamed: its ini names the folder it had. */
    scratch_write(root, "home/data/dxlauncher/mods/Renamed/System/Renamed.u", "pkg");
    scratch_write(root, "home/data/dxlauncher/mods/Renamed/System/Renamed.ini",
                  "[Core.System]\r\nPaths=..\\OldName\\System\\*.u\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/Renamed/Maps/R.dx", "map");
    /* Linked: the player's link to the mod's folder, of another name. */
    scratch_write(root, "elsewhere/Target-1.0/System/Target.u", "pkg");
    scratch_write(root, "elsewhere/Target-1.0/System/Target-1.0.ini",
                  "[Core.System]\r\nSavePath=..\\Target-1.0\\Save\r\nPaths=..\\Linked\\System\\*.u\r\n"
                  "Paths=..\\System\\*.u\r\n");
    scratch_write(root, "elsewhere/Target-1.0/Save/Save0003/SaveInfo.dxs", "linked's");
    char *target = at("elsewhere/Target-1.0"), *link = at("home/data/dxlauncher/mods/Linked");
    if (dxl_fs_link_dir(target, link) != 0) fprintf(stderr, "  could not link %s\n", link);
    free(target);
    free(link);
    /* RootSave: its SavePath its own folder. */
    scratch_write(root, "home/data/dxlauncher/mods/RootSave/System/RootSave.u", "pkg");
    scratch_write(root, "home/data/dxlauncher/mods/RootSave/System/RootSave.ini",
                  "[Core.System]\r\nSavePath=..\\RootSave\r\nPaths=..\\RootSave\\System\\*.u\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/RootSave/Maps/RS.dx", "map");
    /* Picky: a server's ini before its own, which has its controls. */
    scratch_write(root, "home/data/dxlauncher/mods/Picky/System/Picky.u", "pkg");
    scratch_write(root, "home/data/dxlauncher/mods/Picky/System/Aserver.ini", "[Core.System]\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/Picky/System/Main.ini", "[Core.System]\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(root, "home/data/dxlauncher/mods/Picky/System/MainUser.ini", "[Engine.Input]\r\n");
    /* Bare: packages and maps, no ini. */
    scratch_write(root, "home/data/dxlauncher/mods/Bare/System/Bare.u", "pkg");
    scratch_write(root, "home/data/dxlauncher/mods/Bare/Maps/Bare.dx", "map");
}

static void test_check(void) {
    dxl_mod m;
    dxl_err e;
    char *dir = at("game/GameMod");
    CHECK_INT(dxl_mod_check(dir, &m, &e), 0);
    CHECK_STR(m.dir, dir);
    CHECK_STR(m.name, "GameMod");
    CHECK_INT(m.packages, 1);
    CHECK(m.ini && strstr(m.ini, "/GameMod/System/GameMod.ini"));
    CHECK(m.user_ini && strstr(m.user_ini, "/GameMod/System/GameModUser.ini"));
    dxl_mod_free(&m);
    free(dir);

    dir = at("home/data/dxlauncher/mods/OutMod");
    CHECK_INT(dxl_mod_check(dir, &m, &e), 0);
    CHECK(m.system_dir && strstr(m.system_dir, "/OutMod/system"));
    CHECK(m.ini && strstr(m.ini, "/OutMod/system/Default.ini"));
    CHECK(m.user_ini && strstr(m.user_ini, "/OutMod/system/DefUser.ini"));
    dxl_mod_free(&m);
    free(dir);

    dir = at("home/data/dxlauncher/mods/Bare");
    CHECK_INT(dxl_mod_check(dir, &m, &e), 0);
    CHECK(m.ini == NULL && m.user_ini == NULL);
    dxl_mod_free(&m);
    free(dir);

    /* Known by its link, named after it; its settings named after the
     * folder the link is to. */
    dir = at("home/data/dxlauncher/mods/Linked");
    CHECK_INT(dxl_mod_check(dir, &m, &e), 0);
    CHECK_STR(m.dir, dir);
    CHECK_STR(m.name, "Linked");
    CHECK(m.ini && strstr(m.ini, "/Linked/System/Target-1.0.ini"));
    dxl_mod_free(&m);
    free(dir);
    /* Its own settings, with its controls beside them, over another's. */
    dir = at("home/data/dxlauncher/mods/Picky");
    CHECK_INT(dxl_mod_check(dir, &m, &e), 0);
    CHECK(m.ini && strstr(m.ini, "/Picky/System/Main.ini"));
    CHECK(m.user_ini && strstr(m.user_ini, "/Picky/System/MainUser.ini"));
    dxl_mod_free(&m);
    free(dir);
    /* A relative folder, made absolute by the text. */
    char *cwd = getcwd(NULL, 0);
    if (cwd && chdir(root) == 0) {
        CHECK_INT(dxl_mod_check("game/./GameMod/", &m, &e), 0);
        char *want = at("game/GameMod");
        CHECK_STR(m.dir, want);
        free(want);
        dxl_mod_free(&m);
        CHECK_INT(chdir(cwd), 0);
    }
    free(cwd);

    const char *nots[] = { "game/MapPack", "game/Empty", "game/Copy", "game", "game/nothing-there" };
    for (size_t i = 0; i < sizeof nots / sizeof *nots; i++) {
        dir = at(nots[i]);
        CHECK_INT(dxl_mod_check(dir, &m, &e), -1);
        CHECK(dxl_err_msg(&e)[0] != '\0');
        free(dir);
    }
}

static void test_find(void) {
    dxl_mod_list l;
    dxl_mods_find(&l, game, mods);
    CHECK_INT(l.count, 7);
    if (l.count == 7) {
        CHECK_STR(l.items[0].name, "GameMod");
        CHECK_INT(l.items[0].where, DXL_MOD_GAME);
        CHECK_STR(l.items[1].name, "Bare");
        CHECK_INT(l.items[1].where, DXL_MOD_LAUNCHER);
        CHECK_STR(l.items[2].name, "Linked");
        CHECK_STR(l.items[3].name, "OutMod");
        CHECK_STR(l.items[4].name, "Picky");
        CHECK_STR(l.items[5].name, "Renamed");
        CHECK_STR(l.items[6].name, "RootSave");
    }
    /* One of a folder there already is not put in again. */
    dxl_mod m;
    char *dir = at("game/GameMod");
    dxl_mod_check(dir, &m, NULL);
    CHECK_INT(dxl_mod_list_add(&l, &m), -1);
    CHECK_INT(l.count, 7);
    dxl_mod_free(&m);
    free(dir);
    dxl_mod_list_free(&l);
    /* No mods folder yet. */
    dxl_mods_find(&l, game, "/nonexistent/mods");
    CHECK_INT(l.count, 1);
    dxl_mod_list_free(&l);
}

static void test_folders(void) {
    dxl_mod in, out;
    char *d = at("game/GameMod"), *o = at("home/data/dxlauncher/mods/OutMod");
    dxl_mod_check(d, &in, NULL);
    dxl_mod_check(o, &out, NULL);
    char *want = at("game/GameMod/System"), *got = dxl_mod_folder("..\\GameMod\\System\\*.u", system_dir, &in);
    CHECK_STR(got, want);
    free(got); free(want);
    /* In any case, as the engine finds a folder. */
    want = at("game/GameMod/Maps");
    got = dxl_mod_folder("..\\gamemod\\MAPS\\*.dx", system_dir, &in);
    CHECK_STR(got, want);
    free(got); free(want);
    want = at("game/GameMod/Save");
    got = dxl_mod_folder("..\\GameMod\\Save\\", system_dir, &in);
    CHECK_STR(got, want);
    free(got); free(want);
    CHECK(dxl_mod_folder("..\\System\\*.u", system_dir, &in) == NULL);
    CHECK(dxl_mod_folder("..\\HDTP\\System\\*.u", system_dir, &in) == NULL);
    /* Written for it in the game's folder, where it is not: its own, in
     * any case. */
    want = at("home/data/dxlauncher/mods/OutMod/system");
    got = dxl_mod_folder("..\\OutMod\\System\\*.u", system_dir, &out);
    CHECK_STR(got, want);
    free(got); free(want);
    want = at("home/data/dxlauncher/mods/OutMod/Textures");
    got = dxl_mod_folder("..\\outmod\\TEXTURES\\*.utx", system_dir, &out);
    CHECK_STR(got, want);
    free(got); free(want);
    /* Not its name, a folder of the game's, one it lacks, or out of the
     * game's folder. */
    CHECK(dxl_mod_folder("..\\OldName\\System\\*.u", system_dir, &out) == NULL);
    CHECK(dxl_mod_folder("..\\Maps\\*.dx", system_dir, &out) == NULL);
    CHECK(dxl_mod_folder("..\\OutMod\\Music\\*.umx", system_dir, &out) == NULL);
    CHECK(dxl_mod_folder("..\\..\\OutMod\\System\\*.u", system_dir, &out) == NULL);
    /* A link's mod: under the link, by its name or its folder's. */
    dxl_mod lk;
    char *l = at("home/data/dxlauncher/mods/Linked");
    dxl_mod_check(l, &lk, NULL);
    want = at("home/data/dxlauncher/mods/Linked/System");
    got = dxl_mod_folder("..\\Linked\\System\\*.u", system_dir, &lk);
    CHECK_STR(got, want);
    free(got); free(want);
    want = at("home/data/dxlauncher/mods/Linked/Save");
    got = dxl_mod_folder("..\\Target-1.0\\Save", system_dir, &lk);
    CHECK_STR(got, want);
    free(got); free(want);
    /* Named by the folder it is to, by its absolute path: still the link's. */
    char *t = at("elsewhere/Target-1.0/System/*.u");
    want = at("home/data/dxlauncher/mods/Linked/System");
    got = dxl_mod_folder(t, system_dir, &lk);
    CHECK_STR(got, want);
    free(got); free(want); free(t);
    dxl_mod_free(&lk);
    free(l);
    dxl_mod_free(&in);
    dxl_mod_free(&out);
    free(d); free(o);
}

static void test_paths(void) {
    dxl_mod m;
    char *d = at("game/GameMod");
    dxl_mod_check(d, &m, NULL);
    dxl_ini *sys = dxl_ini_load(m.ini, NULL);
    dxl_mod_set_paths(sys, system_dir, &m);
    const char *v[16];
    size_t n = dxl_ini_get_all(sys, "Core.System", "Paths", v, 16);
    CHECK_INT(n, 6);
    char *a = at("game/GameMod/System/*.u"), *b = at("game/GameMod/Maps/*.dx");
    if (n == 6) {
        CHECK_STR(v[0], a);
        CHECK_STR(v[1], b);
        CHECK_STR(v[2], "..\\GameMod\\Maps\\Mod_Intro.dx");   /* no pattern: as it is */
        CHECK_STR(v[3], "..\\System\\*.u");
        CHECK_STR(v[4], "..\\Maps\\*.dx");
        CHECK_STR(v[5], "..\\HDTP\\System\\*.u");
    }
    free(a); free(b);
    dxl_ini_free(sys);
    dxl_mod_free(&m);
    free(d);

    /* An ini for a folder of another name: the mod's folders, then its
     * paths as they are. */
    d = at("home/data/dxlauncher/mods/Renamed");
    dxl_mod_check(d, &m, NULL);
    sys = dxl_ini_load(m.ini, NULL);
    dxl_mod_set_paths(sys, system_dir, &m);
    n = dxl_ini_get_all(sys, "Core.System", "Paths", v, 16);
    CHECK_INT(n, 4);
    a = at("home/data/dxlauncher/mods/Renamed/System/*.u");
    b = at("home/data/dxlauncher/mods/Renamed/Maps/*.dx");
    if (n == 4) {
        CHECK_STR(v[0], a);
        CHECK_STR(v[1], b);
        CHECK_STR(v[2], "..\\OldName\\System\\*.u");
        CHECK_STR(v[3], "..\\System\\*.u");
    }
    free(a); free(b);
    dxl_ini_free(sys);
    dxl_mod_free(&m);
    free(d);

    /* No ini: the game's paths after the mod's folders. */
    d = at("home/data/dxlauncher/mods/Bare");
    dxl_mod_check(d, &m, NULL);
    char *game_ini = at("game/System/DeusEx.ini");
    sys = dxl_ini_load(game_ini, NULL);
    dxl_mod_set_paths(sys, system_dir, &m);
    n = dxl_ini_get_all(sys, "Core.System", "Paths", v, 16);
    CHECK_INT(n, 5);
    a = at("home/data/dxlauncher/mods/Bare/System/*.u");
    b = at("home/data/dxlauncher/mods/Bare/Maps/*.dx");
    if (n == 5) {
        CHECK_STR(v[0], a);
        CHECK_STR(v[1], b);
        CHECK_STR(v[2], "..\\System\\*.u");
        CHECK_STR(v[4], "..\\Textures\\*.utx");
    }
    free(a); free(b);
    dxl_ini_free(sys);
    dxl_mod_free(&m);
    free(d);
    free(game_ini);
}

static void test_import(void) {
    char *home = at("home");
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &x, "gamemod", NULL), 0);
    dxl_mod m;
    char *d = at("game/GameMod");
    dxl_mod_check(d, &m, NULL);
    CHECK_INT(scratch_writable(game, 0), 0);

    dxl_import r;
    dxl_err e;
    CHECK_INT(dxl_profile_import_mod(&p, system_dir, &m, 1, &r, &e), 0);
    CHECK(strstr(r.system_from, "/GameMod/System/GameMod.ini") != NULL);
    CHECK(strstr(r.user_from, "/GameMod/System/GameModUser.ini") != NULL);
    CHECK(strstr(r.saves_from, "/GameMod/Save") != NULL);
    CHECK_INT(r.saves, 1);   /* Save0002; never Current */
    CHECK_INT(r.shots, 0);
    CHECK_INT(r.master_replaced, 1);
    CHECK_INT(dxl_path_exists(p.dir), 0);

    CHECK_INT(dxl_profile_import_mod(&p, system_dir, &m, 0, &r, &e), 0);
    CHECK_INT(scratch_writable(game, 1), 0);

    dxl_ini *sys = dxl_ini_load(p.system_ini, NULL);
    CHECK(sys != NULL);
    if (sys) {
        CHECK_STR(dxl_ini_get(sys, "URL", "Class"), "GameMod.ModPlayer");
        CHECK_STR(dxl_ini_get(sys, "Core.System", "SavePath"), p.save_dir);
        CHECK_STR(dxl_ini_get(sys, "Core.System", "CachePath"), p.cache_dir);
        char *first = at("game/GameMod/System/*.u");
        CHECK_STR(dxl_ini_get(sys, "Core.System", "Paths"), first);
        free(first);
        CHECK_STR(dxl_ini_get(sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress"), "master.333networks.com");
        dxl_ini_free(sys);
    }
    dxl_ini *usr = dxl_ini_load(p.user_ini, NULL);
    CHECK(usr && dxl_ini_get(usr, "Engine.Input", "F1") && !strcmp(dxl_ini_get(usr, "Engine.Input", "F1"), "TheMods"));
    dxl_ini_free(usr);
    char *save = dxl_path_join(p.save_dir, "Save0002/SaveInfo.dxs"), *theirs = dxl_path_join(p.save_dir, "Save0001");
    CHECK_INT(dxl_path_exists(save), 1);
    CHECK_INT(dxl_path_exists(theirs), 0);
    free(save); free(theirs);
    dxl_mod_free(&m);
    dxl_profile_free(&p);
    free(d);

    /* A link's mod: its saves, its paths under the link. */
    CHECK_INT(dxl_profile_open(&p, &x, "linked", NULL), 0);
    d = at("home/data/dxlauncher/mods/Linked");
    dxl_mod_check(d, &m, NULL);
    CHECK_INT(dxl_profile_import_mod(&p, system_dir, &m, 0, &r, &e), 0);
    CHECK_INT(r.saves, 1);
    sys = dxl_ini_load(p.system_ini, NULL);
    char *own = at("home/data/dxlauncher/mods/Linked/System/*.u");
    CHECK(sys && dxl_ini_get(sys, "Core.System", "Paths") && !strcmp(dxl_ini_get(sys, "Core.System", "Paths"), own));
    free(own);
    dxl_ini_free(sys);
    dxl_mod_free(&m);
    dxl_profile_free(&p);
    free(d);

    /* SavePath the mod's own folder: none of its folders taken for saves. */
    CHECK_INT(dxl_profile_open(&p, &x, "rootsave", NULL), 0);
    d = at("home/data/dxlauncher/mods/RootSave");
    dxl_mod_check(d, &m, NULL);
    CHECK_INT(dxl_profile_import_mod(&p, system_dir, &m, 1, &r, &e), 0);
    CHECK_INT(r.saves, 0);
    dxl_mod_free(&m);
    dxl_profile_free(&p);
    free(d);

    /* A mod with no ini: the game's settings and controls, no saves. */
    CHECK_INT(dxl_profile_open(&p, &x, "bare", NULL), 0);
    d = at("home/data/dxlauncher/mods/Bare");
    dxl_mod_check(d, &m, NULL);
    CHECK_INT(dxl_profile_import_mod(&p, system_dir, &m, 0, &r, &e), 0);
    CHECK(strstr(r.system_from, "/game/System/DeusEx.ini") != NULL);
    CHECK(strstr(r.user_from, "/game/System/DefUser.ini") != NULL);
    CHECK_INT(r.saves, 0);
    sys = dxl_ini_load(p.system_ini, NULL);
    char *first = at("home/data/dxlauncher/mods/Bare/System/*.u");
    CHECK(sys && dxl_ini_get(sys, "Core.System", "Paths") && !strcmp(dxl_ini_get(sys, "Core.System", "Paths"), first));
    CHECK(sys && !strcmp(dxl_ini_get(sys, "URL", "Class"), "DeusEx.JCDentonMale"));
    free(first);
    dxl_ini_free(sys);
    dxl_mod_free(&m);
    dxl_profile_free(&p);
    free(d);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    free(home);
}

TEST_MAIN_BEGIN
    make_all();
    RUN(test_check);
    RUN(test_find);
    RUN(test_folders);
    RUN(test_paths);
    RUN(test_import);
    scratch_remove(root);
    free(root);
TEST_MAIN_END
