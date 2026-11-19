#include "test.h"
#include "scratch.h"
#include "app/session.h"
#include "core/ini.h"
#include "core/paths.h"
#include "core/profile.h"
#include "platform/os.h"
#include "platform/fs.h"

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
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
                  "[Engine.Engine]\r\nGameRenderDevice=GlideDrv.GlideRenderDevice\r\n\r\n"
                  "[DeusEx.DeusExGameEngine]\r\nServerActors=IpDrv.UdpBeacon\r\n"
                  "ServerActors=IpServer.UdpServerQuery\r\nServerActors=IpServer.UdpServerUplink\r\n");
    /* Its own list of the server's actors, given line by line. */
    scratch_write(game, "System/DeusEx.ini", "[Engine.Engine]\r\nGameRenderDevice=D3DDrv.D3DRenderDevice\r\n\r\n"
                  "[DeusEx.DeusExGameEngine]\r\nServerActors=IpDrv.UdpBeacon\r\nServerActors=Mod.Announcer\r\n");
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
        /* The stub's list, all of it and nothing of Default.ini's. */
        const char *actors[4];
        CHECK_INT(dxl_ini_get_all(sys, "DeusEx.DeusExGameEngine", "ServerActors", actors, 4), 2);
        CHECK_STR(actors[0], "IpDrv.UdpBeacon");
        CHECK_STR(actors[1], "Mod.Announcer");
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

/* A copy recased on a case-sensitive file system: its SAVE folder is its
 * SavePath's ..\Save, in any case, as the engine finds it, and its saves
 * are carried over. */
static void test_import_recased(void) {
    char *g = dxl_xstrdup(scratch_dir("profile-recased-game"));
    scratch_write(g, "system/deusex.exe", "x");
    scratch_write(g, "system/deusex.ini", "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(g, "system/defuser.ini", "[Engine.Input]\r\nF1=X\r\n");
    scratch_write(g, "SAVE/Save0003/SaveInfo.dxs", "recased");
    char *home = dxl_xstrdup(scratch_dir("profile-recased-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *system_dir = scratch_path(g, "system");
    dxl_import r;
    CHECK_INT(dxl_profile_import(&p, system_dir, 0, &r, NULL), 0);
    CHECK(strstr(r.saves_from, "/SAVE") != NULL);
    CHECK_INT(r.saves, 1);
    char *f = dxl_path_join(p.save_dir, "Save0003/SaveInfo.dxs");
    CHECK_INT(dxl_path_exists(f), 1);
    free(f);
    dxl_profile_free(&p);
    free(system_dir);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    scratch_remove(g);
    free(home);
    free(g);
}

/* More profiles than one: ids made of their names, each its own; those
 * made listed; one's settings copied to another, its saves and logs not,
 * the copy's SavePath and CachePath its own. */
/* A profile made over a data folder that is still there -- its config
 * folder removed to start afresh, a folder put back from the trash -- keeps
 * what the folder holds: a save of its own is never copied into, the game's
 * old files mixed with its later ones (bugs.md LCORE.24). */
static void test_import_keeps_its_own(void) {
    char *g = dxl_xstrdup(scratch_dir("profile-reimport-game"));
    scratch_write(g, "System/DeusEx.exe", "x");
    scratch_write(g, "System/DeusEx.ini", "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(g, "System/User.ini", "[Engine.Input]\r\nF1=X\r\n");
    scratch_write(g, "System/Shot0000.bmp", "game-shot-0");
    scratch_write(g, "System/Shot0001.bmp", "game-shot-1");
    scratch_write(g, "Save/QuickSave/SaveInfo.dxs", "game-old-info");
    scratch_write(g, "Save/QuickSave/01_NYC_UNATCOIsland.dxs", "game-old-level");
    scratch_write(g, "Save/Save0001/SaveInfo.dxs", "game-slot-1");
    char *home = dxl_xstrdup(scratch_dir("profile-reimport-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    /* What the profile's own play left: a later quick save, and a shot. */
    CHECK_INT(dxl_path_mkdirs(p.save_dir), 0);
    CHECK_INT(dxl_path_mkdirs(p.shots_dir), 0);
    scratch_write(p.save_dir, "QuickSave/SaveInfo.dxs", "profile-new-info");
    scratch_write(p.save_dir, "QuickSave/05_NYC_UNATCOMJ12Lab.dxs", "profile-new-level");
    scratch_write(p.shots_dir, "Shot0000.bmp", "profile-shot-0");
    char *sys = scratch_path(g, "System");
    dxl_import r;
    dxl_err e;
    CHECK_INT(dxl_profile_import(&p, sys, 0, &r, &e), 0);
    CHECK_INT(r.saves_kept, 1);
    CHECK_INT(r.saves, 1);
    CHECK_INT(r.shots_kept, 1);
    CHECK_INT(r.shots, 1);
    char *path = dxl_path_join(p.save_dir, "QuickSave/SaveInfo.dxs");
    char *text = slurp(path, NULL);
    CHECK_STR(text, "profile-new-info");
    free(text); free(path);
    path = dxl_path_join(p.save_dir, "QuickSave/01_NYC_UNATCOIsland.dxs");
    CHECK_INT(dxl_path_exists(path), 0);
    free(path);
    path = dxl_path_join(p.save_dir, "Save0001/SaveInfo.dxs");
    text = slurp(path, NULL);
    CHECK_STR(text, "game-slot-1");
    free(text); free(path);
    path = dxl_path_join(p.shots_dir, "Shot0000.bmp");
    text = slurp(path, NULL);
    CHECK_STR(text, "profile-shot-0");
    free(text); free(path);
    path = dxl_path_join(p.shots_dir, "Shot0001.bmp");
    text = slurp(path, NULL);
    CHECK_STR(text, "game-shot-1");
    free(text); free(path);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(sys); free(g); free(home);
}

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

/* Whether len bytes at hay hold n bytes of needle. */
static int has_bytes(const char *hay, size_t len, const char *needle, size_t n) {
    for (size_t i = 0; i + n <= len; i++)
        if (!memcmp(hay + i, needle, n)) return 1;
    return 0;
}

/* A home past ASCII: the profile's DeusEx.ini names its folders as the
 * engine reads them, a byte a character, or UTF-16 for a character past
 * U+00FF, so the saves go where the launcher looks (bugs.md LCORE.7); the
 * launcher's own profile.ini keeps its UTF-8. */
static void test_home_past_ascii(void) {
    const char *tags[] = { "profile-home-h\xc3\xa9", "profile-home-\xd0\xae" };
    char *game = make_install();
    char *system_dir = scratch_path(game, "System");
    for (int k = 0; k < 2; k++) {
        char *home = dxl_xstrdup(scratch_dir(tags[k]));
        dxl_env_set("DXL_HOME", home);
        dxl_xdg x;
        dxl_xdg_open(&x, NULL);
        dxl_profile p;
        dxl_profile_open(&p, &x, "default", NULL);
        dxl_import r;
        dxl_err e;
        CHECK_INT(dxl_profile_import(&p, system_dir, 0, &r, &e), 0);
        size_t len = 0;
        char *text = slurp(p.system_ini, &len);
        CHECK(text != NULL);
        if (text && k == 0) {
            CHECK(has_bytes(text, len, "home-h\xe9-", 8));
            CHECK(!has_bytes(text, len, "home-h\xc3\xa9", 8));
        }
        if (text && k == 1) CHECK(len > 2 && (unsigned char)text[0] == 0xff && (unsigned char)text[1] == 0xfe);
        free(text);
        dxl_ini *sys = dxl_ini_load(p.system_ini, NULL);
        CHECK_STR(sys ? dxl_ini_get(sys, "Core.System", "SavePath") : NULL, p.save_dir);
        CHECK_STR(sys ? dxl_ini_get(sys, "Core.System", "CachePath") : NULL, p.cache_dir);
        dxl_ini_free(sys);

        dxl_profile_set(&p, "Name", "\xc3\x89t\xc3\xa9");
        CHECK_INT(dxl_profile_save(&p, NULL), 0);
        text = slurp(p.ini_path, &len);
        CHECK(text && has_bytes(text, len, "\xc3\x89t\xc3\xa9", 5));
        free(text);
        dxl_profile q;
        dxl_profile_open(&q, &x, "default", NULL);
        CHECK_STR(dxl_profile_get(&q, "Name"), "\xc3\x89t\xc3\xa9");
        dxl_profile_free(&q);

        dxl_profile_free(&p);
        dxl_xdg_free(&x);
        dxl_env_set("DXL_HOME", NULL);
        scratch_remove(home);
        free(home);
    }
    free(system_dir);
    scratch_remove(game);
    free(game);
}

/* A game of three saves and a shot, for the imports that fail. */
static char *small_install(const char *tag) {
    char *g = dxl_xstrdup(scratch_dir(tag));
    scratch_write(g, "System/DeusEx.exe", "x");
    scratch_write(g, "System/DeusEx.ini", "[Core.System]\r\nSavePath=..\\Save\r\nPaths=..\\System\\*.u\r\n");
    scratch_write(g, "System/User.ini", "[Engine.Input]\r\nF1=X\r\n");
    scratch_write(g, "System/Shot0000.bmp", "game-shot-0");
    scratch_write(g, "Save/Save0001/SaveInfo.dxs", "game-slot-1");
    scratch_write(g, "Save/Save0001/01_NYC_UNATCOIsland.dxs", "game-slot-1-level");
    scratch_write(g, "Save/Save0002/SaveInfo.dxs", "game-slot-2");
    scratch_write(g, "Save/Save0003/SaveInfo.dxs", "game-slot-3");
    return g;
}

/* Whether a folder holds a name the import copies under. */
static int has_copying(const char *dir) {
    DIR *d = opendir(dir);
    int found = 0;
    struct dirent *e;
    while (d && (e = readdir(d))) found |= !strncmp(e->d_name, ".dxl-copy-", 10);
    if (d) closedir(d);
    return found;
}

/* Whether any of a profile's four folders is there. */
static int any_folder(const dxl_profile *p) {
    char *dirs[4];
    dxl_profile_folders(p, dirs);
    int any = 0;
    for (int i = 0; i < 4; i++) {
        any |= dxl_path_exists(dirs[i]);
        free(dirs[i]);
    }
    return any;
}

static int cancel_at, told_files, told_total;
static long long told_bytes, told_bytes_total;
static int stop_at(void *ctx, int files, int files_total, long long bytes, long long bytes_total) {
    (void)ctx;
    told_files = files;
    told_total = files_total;
    told_bytes = bytes;
    told_bytes_total = bytes_total;
    return cancel_at && files >= cancel_at;
}

#ifndef _WIN32
/* A link of the player's own among a profile's folders and files is never
 * taken away with what an import made: a save folder linked to a drive that
 * is not there, an ini linked to a file not there yet, which is written
 * through and alone goes again. */
static void test_import_links_kept(void) {
    char *g = small_install("profile-links-game");
    char *home = dxl_xstrdup(scratch_dir("profile-links-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    char *sys = scratch_path(g, "System");
    dxl_import r;
    dxl_err e;
    dxl_made made;
    dxl_profile p;
    struct stat st;

    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    char *up = dxl_path_dirname(p.save_dir);
    CHECK_INT(dxl_path_mkdirs(up), 0);
    CHECK_INT(symlink("/nonexistent/dxl-test-drive/Save", p.save_dir), 0);
    CHECK_INT(dxl_profile_import_watched(&p, sys, NULL, stop_at, NULL, &made, &r, &e), -1);
    CHECK_INT(lstat(p.save_dir, &st), 0);
    CHECK(S_ISLNK(st.st_mode));
    CHECK_INT(dxl_path_exists(p.engine_dir), 0);
    unlink(p.save_dir);
    free(up);
    dxl_profile_free(&p);
    scratch_remove(home);

    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    CHECK_INT(dxl_path_mkdirs(p.dir), 0);
    char *target = dxl_path_join(home, "dotfiles-DeusEx.ini");
    CHECK_INT(symlink(target, p.system_ini), 0);
    cancel_at = 2;
    CHECK_INT(dxl_profile_import_watched(&p, sys, NULL, stop_at, NULL, &made, &r, &e), -1);
    cancel_at = 0;
    CHECK_STR(dxl_err_msg(&e), "the copy was cancelled");
    CHECK_INT(lstat(p.system_ini, &st), 0);
    CHECK(S_ISLNK(st.st_mode));
    CHECK_INT(dxl_path_exists(target), 0);
    CHECK_INT(dxl_path_exists(p.user_ini), 0);
    unlink(p.system_ini);
    free(target);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(sys); free(g); free(home);
}
#endif

/* An import that fails half-way takes away what it made and nothing else
 * (bugs.md LCORE.34): a save it cannot read, its message why; one cancelled
 * through its progress after two files, in a home it made all of. */
static void test_import_undone(void) {
    char *g = small_install("profile-undo-game");
    char *home = dxl_xstrdup(scratch_dir("profile-undo-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    char *sys = scratch_path(g, "System");
    dxl_import r;
    dxl_err e;
    dxl_made made;
    dxl_profile p;
#ifndef _WIN32
    /* A file no one may read: Windows has no such mode. */
    char *bad = scratch_path(g, "Save/Save0002/SaveInfo.dxs");
    chmod(bad, 0);
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    /* What was there: a part-made profile's DeusEx.ini, a save of its own. */
    CHECK_INT(dxl_path_mkdirs(p.dir), 0);
    CHECK_INT(dxl_path_mkdirs(p.save_dir), 0);
    scratch_write(p.dir, "DeusEx.ini", "[Core.System]\nSavePath=before\n");
    scratch_write(p.save_dir, "Save0003/SaveInfo.dxs", "profile-slot-3");
    CHECK_INT(dxl_profile_import_watched(&p, sys, NULL, stop_at, NULL, &made, &r, &e), -1);
    CHECK_INT((int)made.count, 0);
    CHECK(strstr(dxl_err_msg(&e), "Save0002") && strstr(dxl_err_msg(&e), strerror(EACCES)));
    char *text = slurp(p.system_ini, NULL);
    CHECK_STR(text, "[Core.System]\nSavePath=before\n");
    free(text);
    CHECK_INT(dxl_path_exists(p.user_ini), 0);
    CHECK_INT(dxl_path_exists(p.engine_dir), 0);
    CHECK_INT(dxl_path_exists(p.state_dir), 0);
    CHECK_INT(dxl_path_exists(p.cache_dir), 0);
    CHECK_INT(scratch_exists(p.save_dir, "Save0001"), 0);
    CHECK_INT(scratch_exists(p.save_dir, "Save0002"), 0);
    CHECK_INT(has_copying(p.save_dir), 0);
    char *own = dxl_path_join(p.save_dir, "Save0003/SaveInfo.dxs");
    text = slurp(own, NULL);
    CHECK_STR(text, "profile-slot-3");
    free(text);
    free(own);
    dxl_profile_free(&p);
    chmod(bad, 0644);
    free(bad);
    scratch_remove(home);
#endif

    /* Cancelled after two files: the totals counted first, nothing left. */
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    cancel_at = 2;
    CHECK_INT(dxl_profile_import_watched(&p, sys, NULL, stop_at, NULL, &made, &r, &e), -1);
    cancel_at = 0;
    CHECK_STR(dxl_err_msg(&e), "the copy was cancelled");
    CHECK_INT(told_files, 2);
    CHECK_INT(told_total, 5);
    CHECK_INT(told_bytes_total, 11 + 17 + 11 + 11 + 11);
    CHECK_INT(any_folder(&p), 0);
    dxl_profile_free(&p);

    /* Whole, the plan's size the copy's. */
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    CHECK_INT(dxl_profile_import(&p, sys, 1, &r, &e), 0);
    CHECK_INT(r.files, 5);
    CHECK_INT(r.bytes, 61);
    CHECK_INT(any_folder(&p), 0);
    CHECK_INT(dxl_profile_import_watched(&p, sys, NULL, stop_at, NULL, &made, &r, &e), 0);
    CHECK_INT(told_files, 5);
    CHECK_INT(told_bytes, 61);
    CHECK(made.count > 0);
    /* Taken away by its caller, as a profile.ini not written does. */
    dxl_profile_unmake(&made);
    CHECK_INT(any_folder(&p), 0);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(sys); free(g); free(home);
}

/* What an import killed half-way left under its temporary names is no
 * save of the profile's: taken away, the game's copied whole. */
static void test_import_after_kill(void) {
    char *g = small_install("profile-kill-game");
    char *home = dxl_xstrdup(scratch_dir("profile-kill-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    CHECK_INT(dxl_profile_open(&p, &x, "default", NULL), 0);
    CHECK_INT(dxl_path_mkdirs(p.save_dir), 0);
    CHECK_INT(dxl_path_mkdirs(p.shots_dir), 0);
    scratch_write(p.save_dir, ".dxl-copy-Save0001/SaveInfo.dxs", "game-sl");
    scratch_write(p.shots_dir, ".dxl-copy-Shot0000.bmp", "game");
    CHECK_INT(has_copying(p.save_dir) && has_copying(p.shots_dir), 1);
    char *sys = scratch_path(g, "System");
    dxl_import r;
    dxl_err e;
    CHECK_INT(dxl_profile_import(&p, sys, 0, &r, &e), 0);
    CHECK_INT(r.saves_kept, 0);
    CHECK_INT(r.shots_kept, 0);
    CHECK_INT(r.saves, 3);
    CHECK_INT(r.shots, 1);
    CHECK_INT(has_copying(p.save_dir), 0);
    CHECK_INT(has_copying(p.shots_dir), 0);
    char *path = dxl_path_join(p.save_dir, "Save0001/SaveInfo.dxs");
    char *text = slurp(path, NULL);
    CHECK_STR(text, "game-slot-1");
    free(text); free(path);
    path = dxl_path_join(p.save_dir, "Save0001/01_NYC_UNATCOIsland.dxs");
    CHECK_INT(dxl_path_exists(path), 1);
    free(path);
    path = dxl_path_join(p.shots_dir, "Shot0000.bmp");
    text = slurp(path, NULL);
    CHECK_STR(text, "game-shot-0");
    free(text); free(path);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(sys); free(g); free(home);
}

/* A profile whose profile.ini cannot be written is not made: what its
 * import made goes too (bugs.md LCORE.34). */
static void test_make_undone(void) {
    char *g = small_install("profile-made-game");
    char *home = dxl_xstrdup(scratch_dir("profile-made-home"));
    dxl_env_set("DXL_HOME", home);
    dxl_session s;
    dxl_err e;
    CHECK_INT(dxl_session_open(&s, NULL, &e), 0);
    /* profile.ini a folder: no file can be put in its place. */
    CHECK_INT(dxl_path_mkdirs(s.profile.ini_path), 0);
    scratch_write(s.profile.ini_path, "x", "");
    dxl_install in;
    memset(&in, 0, sizeof in);
    in.root = g;
    in.system_dir = scratch_path(g, "System");
    dxl_import r;
    CHECK_INT(dxl_session_make_profile(&s, &in, NULL, "Deus Ex", &r, &e), -1);
    CHECK_INT(dxl_path_exists(s.profile.system_ini), 0);
    CHECK_INT(dxl_path_exists(s.profile.user_ini), 0);
    CHECK_INT(dxl_path_exists(s.profile.engine_dir), 0);
    char *dirs[4];
    dxl_profile_folders(&s.profile, dirs);
    CHECK_INT(dxl_path_exists(dirs[0]), 1);   /* there before, with the folder in it */
    for (int i = 1; i < 4; i++) CHECK_INT(dxl_path_exists(dirs[i]), 0);
    for (int i = 0; i < 4; i++) free(dirs[i]);
    free(in.system_dir);
    dxl_session_close(&s);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(g);
    scratch_remove(home);
    free(g); free(home);
}

TEST_MAIN_BEGIN
    RUN(test_paths);
    RUN(test_import);
    RUN(test_no_links);
    RUN(test_import_from_defaults);
    RUN(test_import_recased);
    RUN(test_import_keeps_its_own);
    RUN(test_import_undone);
#ifndef _WIN32
    RUN(test_import_links_kept);
#endif
    RUN(test_import_after_kill);
    RUN(test_make_undone);
    RUN(test_profiles);
    RUN(test_home_past_ascii);
TEST_MAIN_END
