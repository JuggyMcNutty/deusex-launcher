#include "test.h"
#include "scratch.h"
#include "core/ini.h"
#include "core/paths.h"
#include "core/profile.h"
#include "platform/os.h"
#include "platform/fs.h"

#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

/* A fake install, written for the test: the few lines of each file the
 * import reads, the game's CRLF, and a save, a shot and a Current folder.
 * Made read-only before the import: nothing may be written to it. */
static char *make_install(void) {
    char *g = dxl_xstrdup(scratch_dir("profile-game"));
    scratch_write(g, "System/DeusEx.exe", "not the game");
    scratch_write(g, "System/Default.ini",
                  "[URL]\r\nLocalMap=DX.dx\r\n\r\n[Core.System]\r\nSavePath=..\\Save\r\n"
                  "CachePath=..\\Cache\r\nPaths=..\\System\\*.u\r\nPaths=..\\Maps\\*.dx\r\n\r\n"
                  "[DeusEx.MenuScreenJoinGame]\r\nMasterServerAddress=master0.gamespy.com\r\n");
    scratch_write(g, "System/DeusEx.ini",
                  "[FirstRun]\r\nFirstRun=1100\r\n\r\n[Core.System]\r\nSavePath=..\\Save\r\n"
                  "CachePath=..\\Cache\r\nPaths=..\\System\\*.u\r\nPaths=..\\Maps\\*.dx\r\n\r\n"
                  "[DeusEx.MenuScreenJoinGame]\r\nMasterServerAddress=master0.gamespy.com\r\n");
    scratch_write(g, "System/DXE-DeusEx.ini",
                  "[Engine.DXEngineClient]\nBrightness=0.7\nStartupFullscreen=False\nFullscreenViewportX=1024\n"
                  "FullscreenViewportY=768\n\n"
                  "[Engine.DXEngineAudioDevice]\nMusicVolume=128\n");
    scratch_write(g, "System/DefUser.ini", "[Engine.Input]\r\nF1=Default\r\n");
    scratch_write(g, "System/User.ini", "[Engine.Input]\r\nF1=Older\r\n");
    /* The bindings the original saved: its key names as it spells them, and
     * its key 0. */
    scratch_write(g, "System/DXE-User.ini",
                  "[Engine.Input]\nF1=Newer\nHome=None\n\n"
                  "[Extension.InputExt]\nhome=Jump\nperiod=Duck\nStrProperty0=\n");
    scratch_write(g, "System/Shot0003.bmp", "BM");
    scratch_write(g, "System/Running.ini", "");   /* main's launcher's sentinel: ignored */
    scratch_write(g, "Save/Save0010/SaveInfo.dxs", "info");
    scratch_write(g, "Save/Save0010/01_NYC_UNATCOIsland.dxs", "level");
    scratch_write(g, "Save/QuickSave/SaveInfo.dxs", "quick");
    scratch_write(g, "Save/Current/01_NYC_UNATCOIsland.dxs", "current");
    scratch_write(g, "Maps/DX.dx", "map");
    /* User.ini older than DXE-User.ini, as the engine's own runs leave them. */
    char *user = scratch_path(g, "System/User.ini");
    dxl_fs_set_mtime(user, (long)time(NULL) - 3600);
    free(user);
    /* The shot taken two hours ago. */
    char *shot = scratch_path(g, "System/Shot0003.bmp");
    dxl_fs_set_mtime(shot, (long)time(NULL) - 7200);
    free(shot);
    /* A link in the save folder to the folder itself: no save, not followed. */
    char *loop = scratch_path(g, "Save/Loop");
    dxl_fs_link_dir(".", loop);
    free(loop);
    return g;
}

static void test_paths(void) {
    dxl_env_set("DXL_HOME", "/tmp/dxl-home");
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    CHECK_STR(p.dir, "/tmp/dxl-home/config/dxlauncher/profiles/default");
    CHECK_STR(p.system_ini, "/tmp/dxl-home/config/dxlauncher/profiles/default/DeusEx.ini");
    CHECK_STR(p.engine_dir, "/tmp/dxl-home/config/dxlauncher/profiles/default/engine");
    CHECK_STR(p.save_dir, "/tmp/dxl-home/data/dxlauncher/profiles/default/Save");
    CHECK_STR(p.shots_dir, "/tmp/dxl-home/data/dxlauncher/profiles/default/Screenshots");
    CHECK_STR(p.state_dir, "/tmp/dxl-home/state/dxlauncher/profiles/default");
    CHECK_STR(p.logs_dir, "/tmp/dxl-home/state/dxlauncher/profiles/default/logs");
    CHECK_STR(p.cache_dir, "/tmp/dxl-home/cache/dxlauncher/profiles/default/Cache");
    CHECK_INT(dxl_profile_exists(&p), 0);
    dxl_profile_free(&p);
    /* Ids are names for folders: nothing that walks out of one. */
    CHECK_INT(dxl_profile_open(&p, &x, "../escape", NULL), -1);
    CHECK_INT(dxl_profile_open(&p, &x, "", NULL), -1);
    CHECK_INT(dxl_profile_open(&p, &x, "a b", NULL), -1);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
}


/* The first Paths= line, the mod's: dxl_ini_get gives the last, the value
 * the engine reads for a key given once. */
static const char *first_path(const dxl_ini *sys) {
    const char *v[1];
    return sys && dxl_ini_get_all(sys, "Core.System", "Paths", v, 1) ? v[0] : NULL;
}

static void test_import(void) {
    char *game = make_install();
    char *home = dxl_xstrdup(scratch_dir("profile-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *system_dir = scratch_path(game, "System");

    CHECK_INT(scratch_writable(game, 0), 0);
    char *before = scratch_listing(game);

    /* What it would do, first: nothing written anywhere. */
    dxl_import r;
    dxl_err e;
    CHECK_INT(dxl_profile_import(&p, system_dir, 1, &r, &e), 0);
    CHECK(strstr(r.system_from, "System/DeusEx.ini") != NULL);
    CHECK(strstr(r.user_from, "System/DXE-User.ini") != NULL);
    CHECK(strstr(r.engine_from, "System/DXE-DeusEx.ini") != NULL);
    CHECK_INT(r.saves, 2);
    CHECK_INT(r.shots, 1);
    CHECK_INT(r.master_replaced, 1);
    CHECK_INT(r.bindings_folded, 2);
    CHECK_INT(dxl_path_exists(p.dir), 0);

    CHECK_INT(dxl_profile_import(&p, system_dir, 0, &r, &e), 0);
    if (dxl_err_msg(&e)[0] && !dxl_path_exists(p.system_ini)) fprintf(stderr, "  %s\n", dxl_err_msg(&e));

    /* The install is as it was. */
    char *after = scratch_listing(game);
    CHECK_STR(after, before);
    free(after);
    free(before);
    CHECK_INT(scratch_writable(game, 1), 0);

    /* DeusEx.ini: the game's, LF, the save and cache paths the profile's,
     * the Paths as they were, the master 333networks', and the engine's
     * sections from DXE-DeusEx.ini. */
    dxl_ini *sys = dxl_ini_load(p.system_ini, NULL);
    CHECK(sys != NULL);
    if (sys) {
        CHECK_INT(dxl_ini_get_int(sys, "FirstRun", "FirstRun", 0), 1100);
        CHECK_STR(dxl_ini_get(sys, "Core.System", "SavePath"), p.save_dir);
        CHECK_STR(dxl_ini_get(sys, "Core.System", "CachePath"), p.cache_dir);
        CHECK_INT(dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0), 2);
        CHECK_STR(first_path(sys), "..\\System\\*.u");
        CHECK_STR(dxl_ini_get(sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress"),
                  "master.333networks.com");
        CHECK_STR(dxl_ini_get(sys, "Engine.DXEngineClient", "Brightness"), "0.7");
        /* Full screen at the desktop's size, whatever the install's says. */
        CHECK_STR(dxl_ini_get(sys, "Engine.DXEngineClient", "FullscreenViewportX"), "0");
        CHECK_STR(dxl_ini_get(sys, "Engine.DXEngineClient", "FullscreenViewportY"), "0");
        CHECK_STR(dxl_ini_get(sys, "Engine.DXEngineAudioDevice", "MusicVolume"), "128");
        dxl_ini_free(sys);
    }
    size_t len = 0;
    char *text = slurp(p.system_ini, &len);
    CHECK(text && !strchr(text, '\r'));
    free(text);
    /* User.ini: the newer, DXE-User.ini, LF, the original's bindings in
     * [Engine.Input]. */
    dxl_ini *usr = dxl_ini_load(p.user_ini, NULL);
    CHECK(usr != NULL);
    if (usr) {
        CHECK_STR(dxl_ini_get(usr, "Engine.Input", "F1"), "Newer");
        CHECK_STR(dxl_ini_get(usr, "Engine.Input", "Home"), "Jump");
        CHECK_STR(dxl_ini_get(usr, "Engine.Input", "Period"), "Duck");
        CHECK(dxl_ini_get(usr, "Engine.Input", "StrProperty0") == NULL);
        CHECK_INT(dxl_ini_pairs(usr, "Extension.InputExt", NULL, NULL, 0), 0);
        dxl_ini_free(usr);
    }

    /* The saves but Current, and the shot; the shots folder is the engine's
     * through its link. */
    char *f = dxl_path_join(p.save_dir, "Save0010/01_NYC_UNATCOIsland.dxs");
    CHECK_INT(dxl_path_exists(f), 1);
    free(f);
    f = dxl_path_join(p.save_dir, "QuickSave/SaveInfo.dxs");
    CHECK_INT(dxl_path_exists(f), 1);
    free(f);
    f = dxl_path_join(p.save_dir, "Current");
    CHECK_INT(dxl_path_exists(f), 0);
    free(f);
    f = dxl_path_join(p.save_dir, "Loop");
    CHECK_INT(dxl_path_exists(f), 0);
    free(f);
    f = dxl_path_join(p.shots_dir, "Shot0003.bmp");
    CHECK_INT(dxl_path_exists(f), 1);
    CHECK(dxl_path_mtime(f) <= (long)time(NULL) - 7000);   /* taken when the game wrote it */
    free(f);
    f = dxl_path_join(p.engine_dir, "Screenshots/Shot0003.bmp");
    CHECK_INT(dxl_path_exists(f), 1);
    free(f);
    /* main's launcher's Running.ini is no crash of this profile's. */
    f = dxl_path_join(p.state_dir, "Running.ini");
    CHECK_INT(dxl_path_exists(f), 0);
    free(f);

    /* profile.ini. */
    dxl_profile_set(&p, "Name", "Deus Ex");
    CHECK_INT(dxl_profile_save(&p, NULL), 0);
    CHECK_INT(dxl_profile_exists(&p), 1);
    dxl_profile q;
    dxl_profile_open(&q, &x, "default", NULL);
    CHECK_STR(dxl_profile_get(&q, "Name"), "Deus Ex");
    dxl_profile_free(&q);

    dxl_profile_free(&p);
    free(system_dir);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    scratch_remove(game);
    free(home);
    free(game);
}

/* A DeusEx.ini with no Paths is a stub the engine cannot start from: the
 * profile's is Default.ini with the stub's values over it. With no
 * DXE-User.ini or User.ini, DefUser.ini. */
/* A filesystem that makes no links, as exFAT and FAT: how it refuses. */
static int link_errno;
static int no_link(const char *target, const char *link) {
    (void)target;
    (void)link;
    errno = link_errno;
    return -1;
}

/* There, the engine's Screenshots is a folder and the profile's shots
 * folder, the import's shot in it, as the profile opens again too; any
 * other refusal is an error. */
static void test_no_links(void) {
    char *game = make_install();
    char *home = dxl_xstrdup(scratch_dir("profile-nolinks"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *system_dir = scratch_path(game, "System");
    char *data_shots = dxl_xstrdup(p.shots_dir);
    char *engine_shots = dxl_path_join(p.engine_dir, "Screenshots");

    dxl_profile_link = no_link;
    link_errno = EPERM;
    dxl_import r;
    dxl_err e;
    CHECK_INT(dxl_profile_import(&p, system_dir, 0, &r, &e), 0);
    CHECK(!dxl_fs_is_link(engine_shots) && dxl_path_is_dir(engine_shots));
    CHECK_STR(p.shots_dir, engine_shots);
    CHECK_INT(dxl_path_exists(data_shots), 0);
    CHECK_INT(scratch_exists(engine_shots, "Shot0003.bmp"), 1);
    dxl_profile_free(&p);
    dxl_profile_open(&p, &x, "default", NULL);
    CHECK_STR(p.shots_dir, engine_shots);
    dxl_profile_free(&p);

    dxl_profile_open(&p, &x, "other", NULL);
    link_errno = EACCES;
    CHECK_INT(dxl_profile_make_dirs(&p, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "cannot link") != NULL);
    dxl_profile_free(&p);
    dxl_profile_link = dxl_fs_link_dir;

    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    free(engine_shots);
    free(data_shots);
    free(system_dir);
    scratch_remove(home);
    scratch_remove(game);
    free(home);
    free(game);
}

static void test_import_from_defaults(void) {
    char *game = dxl_xstrdup(scratch_dir("profile-stub"));
    scratch_write(game, "System/DeusEx.exe", "x");
    scratch_write(game, "System/Default.ini",
                  "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\System\\*.u\r\n\r\n"
                  "[Engine.Engine]\r\nGameRenderDevice=GlideDrv.GlideRenderDevice\r\n");
    scratch_write(game, "System/DeusEx.ini", "[Engine.Engine]\r\nGameRenderDevice=D3DDrv.D3DRenderDevice\r\n");
    scratch_write(game, "System/DefUser.ini", "[Engine.Input]\r\nF1=Default\r\n");
    char *home = dxl_xstrdup(scratch_dir("profile-stub-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *system_dir = scratch_path(game, "System");
    dxl_import r;
    CHECK_INT(dxl_profile_import(&p, system_dir, 0, &r, NULL), 0);
    CHECK_STR(r.engine_from, "");
    CHECK_INT(r.saves, 0);
    CHECK_INT(r.master_replaced, 0);
    dxl_ini *sys = dxl_ini_load(p.system_ini, NULL);
    CHECK(sys != NULL);
    if (sys) {
        CHECK_STR(first_path(sys), "..\\System\\*.u");
        CHECK_STR(dxl_ini_get(sys, "Engine.Engine", "GameRenderDevice"), "D3DDrv.D3DRenderDevice");
        dxl_ini_free(sys);
    }
    dxl_ini *usr = dxl_ini_load(p.user_ini, NULL);
    CHECK(usr && strcmp(dxl_ini_get(usr, "Engine.Input", "F1"), "Default") == 0);
    dxl_ini_free(usr);
    dxl_profile_free(&p);
    free(system_dir);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    scratch_remove(game);
    free(home);
    free(game);
}

/* More profiles than one: ids made of their names, each its own; those
 * made listed; one's settings copied to another, its saves and logs not,
 * the copy's SavePath and CachePath its own. */
static void test_profiles(void) {
    char *game = make_install();
    char *home = dxl_xstrdup(scratch_dir("profiles-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    char id[DXL_PROFILE_ID_MAX + 1];
    dxl_profile_new_id(&x, "Deus Ex 2", id, sizeof id);
    CHECK_STR(id, "deus-ex-2");
    dxl_profile_new_id(&x, "  GMDX: v10!  ", id, sizeof id);
    CHECK_STR(id, "gmdx-v10");
    dxl_profile_new_id(&x, "\xc3\x89t\xc3\xa9", id, sizeof id);
    CHECK_STR(id, "t");
    dxl_profile_new_id(&x, "***", id, sizeof id);
    CHECK_STR(id, "profile");

    dxl_profile a;
    dxl_profile_open(&a, &x, "default", NULL);
    char *system_dir = scratch_path(game, "System");
    dxl_import r;
    CHECK_INT(dxl_profile_import(&a, system_dir, 0, &r, NULL), 0);
    dxl_profile_set(&a, "Name", "Deus Ex");
    CHECK_INT(dxl_profile_save(&a, NULL), 0);
    scratch_write(a.engine_dir, "Settings.json", "{}");
    scratch_write(a.engine_dir, "DXE-Log-LastRun.txt", "a log");
    scratch_write(a.engine_dir, "HXMod.ini", "[HX.Mod]\nOn=True\n");
    /* Taken now: the next of that name another. */
    dxl_profile_new_id(&x, "Default", id, sizeof id);
    CHECK_STR(id, "default-2");

    dxl_profile b;
    dxl_profile_open(&b, &x, "copy", NULL);
    CHECK_INT(dxl_profile_copy(&a, &b, NULL), 0);
    dxl_profile_free(&b);
    dxl_profile_open(&b, &x, "copy", NULL);
    CHECK_INT(dxl_profile_exists(&b), 1);
    CHECK_STR(dxl_profile_get(&b, "Name"), "Deus Ex");
    dxl_ini *sys = dxl_ini_load(b.system_ini, NULL);
    CHECK(sys && !strcmp(dxl_ini_get(sys, "Core.System", "SavePath"), b.save_dir));
    CHECK(sys && !strcmp(dxl_ini_get(sys, "Core.System", "CachePath"), b.cache_dir));
    dxl_ini_free(sys);
    CHECK(scratch_exists(b.engine_dir, "Settings.json"));
    CHECK(scratch_exists(b.engine_dir, "HXMod.ini"));
    CHECK(!scratch_exists(b.engine_dir, "DXE-Log-LastRun.txt"));
    CHECK_INT(dxl_profile_count_saves(&a), 2);
    /* What the Saves page does not list, nor the count: a file of a save's
     * name, a folder of a name the engine does not give. */
    scratch_write(a.save_dir, "Save0001.bak", "x");
    scratch_write(a.save_dir, "Save010/SaveInfo.dxs", "x");
    CHECK_INT(dxl_profile_count_saves(&a), 2);
    CHECK_INT(dxl_profile_count_saves(&b), 0);
    /* Made already: not again. */
    CHECK_INT(dxl_profile_copy(&a, &b, NULL), -1);

    char ids[4][DXL_PROFILE_ID_MAX + 1];
    CHECK_INT((int)dxl_profiles_list(&x, ids, 4), 2);
    CHECK_STR(ids[0], "copy");
    CHECK_STR(ids[1], "default");
    char *dirs[4];
    dxl_profile_folders(&b, dirs);
    CHECK(strstr(dirs[0], "/config/dxlauncher/profiles/copy") != NULL);
    CHECK(strstr(dirs[1], "/data/dxlauncher/profiles/copy") != NULL);
    CHECK(strstr(dirs[2], "/state/dxlauncher/profiles/copy") != NULL);
    CHECK(strstr(dirs[3], "/cache/dxlauncher/profiles/copy") != NULL);
    for (int i = 0; i < 4; i++) free(dirs[i]);

    dxl_profile_free(&a);
    dxl_profile_free(&b);
    free(system_dir);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(scratch_writable(game, 1), 0);
    scratch_remove(home);
    scratch_remove(game);
    free(home);
    free(game);
}

TEST_MAIN_BEGIN
    RUN(test_paths);
    RUN(test_import);
    RUN(test_no_links);
    RUN(test_import_from_defaults);
    RUN(test_profiles);
TEST_MAIN_END
