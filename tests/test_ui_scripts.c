/* The window through its own loop, on the desktop's display: a script of
 * input a run (screens/uiscript.h, the scripts in tests/ui/), with the
 * stub engine (stub_engine.c) in the engine's place, the launcher in a
 * scratch app folder (its two programs, the source's data/), its profile
 * made from the install in gamefiles/ in a scratch home. Then what each
 * run changed, read back from the profile's files and the launcher's log.
 * Skipped with no display or no install. Nothing is written but the two
 * scratch folders. */
#define _GNU_SOURCE
#include "test.h"
#include "core/engine_settings.h"
#include "core/ini.h"
#include "core/json.h"
#include "core/keys.h"
#include "core/paths.h"
#include "core/bindings.h"
#include "core/profile.h"
#include "core/settings.h"
#include "core/xdg.h"

#include "upkg_writer.h"

#include <arpa/inet.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char app[256], home[256];

static int sh(const char *fmt, ...) {
    char cmd[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    int rc = system(cmd);
    return WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;
}

/* A home with nothing in it but the profile, made from the install. */
static int fresh_home(void) {
    sh("rm -rf '%s' && mkdir -p '%s'", home, home);
    return sh("DXL_HOME='%s' '%s/deusex-launcher' --game='%s' --import >>'%s/out.txt' 2>&1", home, app, DXL_GAME,
              home);
}

/* SDL's controllers those of the scripts' virtual ones' makes and models
 * alone (uiscript.c's): a pad of another plugged into the machine moves
 * nothing of a run, though a real one of those models would. Their input
 * reaches the window as it waits behind another one too: SDL drops a pad's
 * for a window without the focus, which one on a desktop someone uses may
 * not have. */
#define OUR_PADS "SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x045e/0x0b12,0x054c/0x0ce6,0x057e/0x2009,0x1234/0x5678,0x1209/0xd0d1 " \
                 "SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1 " QUIET
/* The window's sound to SDL's dummy output: the title music and the menu's
 * sounds played, as a player's would be, and none of it heard on the
 * desktop the runs share. */
#define QUIET "SDL_AUDIO_DRIVER=dummy"

/* The launcher on the script, the stub doing mode; its exit code. */
static int launch_with(const char *script, const char *mode, const char *env) {
    return sh("DXL_HOME='%s' DXL_ENGINE='%s' DXL_STUB=%s DXL_STUB_MS=300 %s %s DXL_UI_SCRIPT='%s/%s' "
              "'%s/deusex-launcher' >>'%s/out.txt' 2>&1",
              home, DXL_STUB_ENGINE, mode, OUR_PADS, env, DXL_UI_SCRIPTS, script, app, home);
}

static int launch(const char *script, const char *mode) { return launch_with(script, mode, ""); }

static char *home_file(const char *rel) {
    size_t n = strlen(home) + strlen(rel) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", home, rel);
    return p;
}

static char *read_home(const char *rel) {
    char *p = home_file(rel);
    size_t len = 0;
    char *text = slurp(p, &len);
    free(p);
    return text ? text : dxl_xstrdup("");
}

/* The script ran to its end: no line of it failed. */
static void script_ok(void) {
    char *out = read_home("out.txt"), *log = read_home("state/dxlauncher/launcher.log");
    if (strstr(out, "ui script: FAILED")) fprintf(stderr, "%s", out);
    CHECK(strstr(out, "ui script: FAILED") == NULL);
    CHECK(strstr(log, "ui script: FAILED") == NULL);
    free(out);
    free(log);
}

static int count(const char *text, const char *what) {
    int n = 0;
    for (const char *p = text; (p = strstr(p, what)); p += strlen(what)) n++;
    return n;
}

/* The nth line holding what, or "". */
static void line_with(const char *text, const char *what, int nth, char *out, size_t size) {
    out[0] = '\0';
    const char *p = text;
    for (int i = 0; (p = strstr(p, what)); i++, p += strlen(what)) {
        if (i < nth) continue;
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        if (n >= size) n = size - 1;
        memcpy(out, p, n);
        out[n] = '\0';
        return;
    }
}

typedef struct {
    dxl_profile p;
    dxl_xdg x;
    dxl_ini *sys, *usr;
    dxl_engine_settings *es;
} profile_files;

static void open_files(profile_files *f) {
    setenv("DXL_HOME", home, 1);
    dxl_xdg_open(&f->x, NULL);
    dxl_profile_open(&f->p, &f->x, "default", NULL);
    f->sys = dxl_ini_load(f->p.system_ini, NULL);
    f->usr = dxl_ini_load(f->p.user_ini, NULL);
    char *json = dxl_path_join(f->p.engine_dir, "Settings.json");
    char *def = dxl_path_join(DXL_DATA, "engine-settings.json.default");
    f->es = dxl_es_open(json, def);
    free(json);
    free(def);
    unsetenv("DXL_HOME");
}

static void close_files(profile_files *f) {
    dxl_ini_free(f->sys);
    dxl_ini_free(f->usr);
    dxl_es_free(f->es);
    dxl_profile_free(&f->p);
    dxl_xdg_free(&f->x);
}

/* The first Paths= line, the mod's: dxl_ini_get gives the last, the value
 * the engine reads for a key given once. */
static const char *first_path(const dxl_ini *sys) {
    const char *v[1];
    return sys && dxl_ini_get_all(sys, "Core.System", "Paths", v, 1) ? v[0] : NULL;
}

static const char *value(const dxl_ini *ini, const char *section, const char *key) {
    const char *v = ini ? dxl_ini_get(ini, section, key) : NULL;
    return v ? v : "";
}

/* A row of each kind on each page, changed through the window, is in its
 * file as its reader takes it; the next run's words went on the run's
 * command line. */
static void test_settings_roundtrip(void) {
    CHECK_INT(fresh_home(), 0);
    profile_files a, b;
    open_files(&a);
    CHECK(a.sys && a.usr);
    if (!a.sys || !a.usr) return;
    char was_aa[32], was_theme[64], was_c[128], crouch[32] = "";
    snprintf(was_c, sizeof was_c, "%s", value(a.usr, "Engine.Input", "C"));
    const char *keys[2];
    if (dxl_keys_bound(a.usr, "Duck", keys, 2) > 0) snprintf(crouch, sizeof crouch, "%s", keys[0]);
    snprintf(was_aa, sizeof was_aa, "%s", dxl_es_choice(a.es, DXL_ES_ANTIALIAS));
    snprintf(was_theme, sizeof was_theme, "%s", value(a.usr, "DeusEx.DeusExPlayer", "MenuThemeName"));
    /* A value outside its slider's range, as the game's console or a hand
     * sets one: shown on Keyboard & mouse, and kept as it is. */
    dxl_ini_set(a.usr, "Engine.PlayerPawn", "MouseSensitivity", "15");
    CHECK_INT(dxl_ini_save(a.usr, a.p.user_ini, NULL), 0);
    /* The original's software renderer in the ini, and no Settings.json:
     * the engine's choice, which the page shows, and no Type written. */
    CHECK(!dxl_path_exists(dxl_es_path(a.es)));
    dxl_ini_set(a.sys, "Engine.Engine", "GameRenderDevice", "SoftDrv.SoftwareRenderDevice");
    CHECK_INT(dxl_ini_save(a.sys, a.p.system_ini, NULL), 0);

    /* A profile whose ini asks for a window: the run is full screen all the same. */
    dxl_ini_set(a.sys, "Engine.DXEngineClient", "StartupFullscreen", "False");
    CHECK_INT(dxl_ini_save(a.sys, a.p.system_ini, NULL), 0);

    CHECK_INT(launch("settings_roundtrip.script", "ready"), 0);
    script_ok();
    open_files(&b);
    CHECK(b.sys && b.usr);
    if (!b.sys || !b.usr) {
        close_files(&a);
        return;
    }

    /* Display: full screen for the run, whatever the ini said. */
    CHECK_INT(dxl_ini_get_bool(b.sys, "Engine.DXEngineClient", "StartupFullscreen", -1), 1);
    double fov = atof(value(a.usr, "Engine.PlayerPawn", "DesiredFOV")) + 5;
    if (fov > 120) fov = 120;
    CHECK(atof(value(b.usr, "Engine.PlayerPawn", "DesiredFOV")) == fov);
    CHECK(atof(value(b.usr, "Engine.PlayerPawn", "DefaultFOV")) == fov);
    /* Graphics: Settings.json made, with no renderer named. */
    CHECK(strcmp(dxl_es_choice(b.es, DXL_ES_ANTIALIAS), was_aa) != 0);
    dxl_json *json = dxl_json_load(dxl_es_path(b.es), NULL);
    CHECK(json != NULL);
    CHECK_STR(dxl_json_get_string(dxl_json_get(json, "RenderDevice"), "Type", "none"), "none");
    dxl_json_free(json);
    /* Audio: two steps of 0.02 down, as a byte. */
    double music = atoi(value(a.sys, "Engine.DXEngineAudioDevice", "MusicVolume")) / 255.0 - 0.04;
    if (music < 0) music = 0;
    char want[16];
    snprintf(want, sizeof want, "%ld", lround(music * 255));
    CHECK_STR(value(b.sys, "Engine.DXEngineAudioDevice", "MusicVolume"), want);
    /* Keyboard: the mouse's sensitivity as it was, untouched; J for Jump,
     * in its second slot, the key there before let go; C for Jump as well,
     * in its first, Space let go; Crouch's first key gone. */
    CHECK_STR(value(b.usr, "Engine.PlayerPawn", "MouseSensitivity"), "15");
    CHECK_STR(value(b.usr, "Engine.Input", "J"), "Jump");
    CHECK_STR(value(b.usr, "Engine.Input", "Insert"), "");
    CHECK_STR(value(b.usr, "Engine.Input", "Space"), "");
    char both[160];
    snprintf(both, sizeof both, "%s|Jump", was_c);
    CHECK_STR(value(b.usr, "Engine.Input", "C"), both);
    CHECK(crouch[0] != 0);
    if (crouch[0]) CHECK_STR(value(b.usr, "Engine.Input", crouch), "");
    /* Gameplay & HUD. */
    CHECK_INT(dxl_ini_get_bool(b.usr, "DeusEx.DeusExPlayer", "bSubtitles", -1),
              !dxl_ini_get_bool(a.usr, "DeusEx.DeusExPlayer", "bSubtitles", -1));
    CHECK(strcmp(value(b.usr, "DeusEx.DeusExPlayer", "MenuThemeName"), was_theme) != 0);
    /* Multiplayer. */
    CHECK_STR(value(b.usr, "DefaultPlayer", "Name"), "JC(Denton)");
    /* Launcher: the next run's words, on that run's command line. */
    char *log = read_home("state/dxlauncher/launcher.log");
    /* The window wore the menu theme chosen, the install's colours of it. */
    char worn[160];
    snprintf(worn, sizeof worn, "theme: %s\n", value(b.usr, "DeusEx.DeusExPlayer", "MenuThemeName"));
    CHECK(strstr(log, worn) != NULL);
    char line[2048];
    line_with(log, "starting the game:", 0, line, sizeof line);
    CHECK(strstr(line, "-nohard") != NULL);
    CHECK(strstr(line, "-noddraw") == NULL);
    CHECK(strstr(log, "full screen: the device shows nothing else") != NULL);
    CHECK(strstr(log, "the game ended: 0, cleanly") != NULL);
    free(log);
    close_files(&a);
    close_files(&b);
}

/* Settings.json edited by hand, then Settings opened and left, nothing
 * changed: one the engine refuses -- a comma before a close, a comma
 * missing -- kept beside the new one, and said. */
static void test_settings_kept(void) {
    static const char *const files[] = {
        "{\"RenderDevice\": {\"Antialias\": \"MSAA2x\",}}\n",
        "{\"RenderDevice\": {\"Antialias\": \"MSAA2x\" \"Light\": \"OneX\"}}\n",
    };
    for (int i = 0; i < 2; i++) {
        CHECK_INT(fresh_home(), 0);
        profile_files a, b;
        open_files(&a);
        char *json = dxl_xstrdup(dxl_es_path(a.es)), kept[600];
        snprintf(kept, sizeof kept, "%s" DXL_ES_KEPT, json);
        close_files(&a);
        char *dir = dxl_path_dirname(json);
        dxl_path_mkdirs(dir);
        free(dir);
        FILE *f = fopen(json, "wb");
        CHECK(f != NULL);
        if (f) {
            fputs(files[i], f);
            fclose(f);
        }
        CHECK_INT(launch("settings_kept.script", "ready"), 0);
        script_ok();
        open_files(&b);
        char *log = read_home("state/dxlauncher/launcher.log");
        char *was = slurp(kept, NULL);
        CHECK_STR(was, files[i]);
        CHECK(strstr(log, "could not be read (JSON: ") != NULL);
        CHECK_INT(dxl_es_was_corrupt(b.es), 0);
        free(was);
        free(log);
        free(json);
        close_files(&b);
    }
}

/* After a crash: Recovery, its log page, the next run in a window, another
 * renderer; Play; the window's words on that run alone. */
/* After a crash: Recovery, its log page, the next run on the software
 * renderer, another renderer; Play; the window's words on that run alone. */
static void test_recovery(void) {
    CHECK_INT(fresh_home(), 0);
    profile_files a, b;
    open_files(&a);
    char was[32];
    snprintf(was, sizeof was, "%s", dxl_es_choice(a.es, DXL_ES_RENDER_TYPE));
    launch("recovery.script", "crash");
    script_ok();
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK_INT(count(log, "starting the game:"), 2);
    char first[2048], second[2048];
    line_with(log, "starting the game:", 0, first, sizeof first);
    line_with(log, "starting the game:", 1, second, sizeof second);
    CHECK(strstr(first, "-nohard") == NULL);
    CHECK(strstr(second, "-nohard") != NULL);
    CHECK(strstr(second, "-noddraw") == NULL);
    /* The renderer chosen, when this machine runs more than one. */
    open_files(&b);
    char chose[256];
    line_with(log, "renderer: ", 0, chose, sizeof chose);
    if (chose[0]) {
        char want[64];
        snprintf(want, sizeof want, "renderer: %s ", dxl_es_choice(b.es, DXL_ES_RENDER_TYPE));
        CHECK(strncmp(chose, want, strlen(want)) == 0);
    } else {
        fprintf(stderr, "  (one renderer runs here: none chosen; it stays %s)\n", was);
        CHECK_STR(dxl_es_choice(b.es, DXL_ES_RENDER_TYPE), was);
    }
    free(log);
    close_files(&a);
    close_files(&b);
}

/* The Controller page: three buttons bound from it, two found by pressing
 * or pulling them; a pad mapped by the wizard its confirm opened, and a
 * controller SDL had no mapping for, kept and handed to the engine. */
static void test_remap_pad(void) {
    CHECK_INT(fresh_home(), 0);
    launch_with("remap_pad.script", "env", "DXL_STUB_ENV=SDL_GAMECONTROLLERCONFIG");
    script_ok();
    profile_files b;
    open_files(&b);
    CHECK_STR(value(b.usr, "Engine.Input", "Joy1"), "QuickSave");
    CHECK_STR(value(b.usr, "Engine.Input", "Joy4"), "QuickLoad");
    CHECK_STR(value(b.usr, "Engine.Input", "Joy12"), "ToggleScope");
    char *db = read_home("data/dxlauncher/gamecontrollerdb.txt");
    /* The confirm that opened it not taken for the first step. */
    CHECK(strstr(db, ",Xbox Series X Controller,a:b2,platform:Linux,") != NULL);
    /* The trigger resting at the top its whole axis, turned; the d-pad's up
     * once. */
    const char *raw = ",A pad of no maker,a:b0,b:b1,x:b2,y:b3,dpup:h0.1,dpdown:h0.4,lefttrigger:a2~,platform:Linux,";
    CHECK(strstr(db, raw) != NULL);
    free(db);
    /* What the engine was started with: each, and its copy for an older SDL2. */
    char *run = read_home("state/dxlauncher/profiles/default/logs/last-run.log");
    CHECK(strstr(run, raw) != NULL);
    CHECK(strstr(run, "lefttrigger:a2~,platform:Linux,crc:") != NULL);
    free(run);
    close_files(&b);
}

/* The controller's test: the pad drove the model alone till its back was
 * held -- Start played nothing, the back let go of left nothing -- and Esc
 * ended it again; tried twice, done twice. */
static void test_pad_test(void) {
    CHECK_INT(fresh_home(), 0);
    launch("pad_test.script", "ready");
    script_ok();
    char *log = read_home("state/dxlauncher/launcher.log");
    /* Home wore the install's art on the way, its music and sounds with it. */
    CHECK(strstr(log, "art: the logo and the main menu's buttons from") != NULL);
    CHECK(strstr(log, "sound: the title music and 6 menu sounds") != NULL);
    CHECK_INT(count(log, "pad test: Xbox Series X Controller"), 2);
    CHECK_INT(count(log, "pad test: done"), 2);
    CHECK(strstr(log, "starting the game") == NULL);
    free(log);
}

/* Engines in the scratch home: glibc, one the loader refuses for a glibc
 * newer than this system's; good, the stub as DXEngine. */
static void engines(void) {
    sh("mkdir -p '%s/engines/glibc' '%s/engines/good' && cp '%s' '%s/engines/good/DXEngine'", home, home,
       DXL_STUB_ENGINE, home);
    char *script = home_file("engines/glibc/DXEngine");
    FILE *f = fopen(script, "w");
    if (f) {
        fputs("#!/bin/sh\necho \"$0: /lib64/libm.so.6: version \\`GLIBC_2.99' not found (required by $0)\" >&2\n"
              "exit 127\n", f);
        fclose(f);
    }
    chmod(script, 0755);
    free(script);
}

/* An engine this system cannot start, launcher.ini's, no DXL_ENGINE: Home
 * said why and offered the fix; another taken by the browser, after one
 * more refused; the game's folder chosen again from Settings; the
 * launcher's own engine again. */
static void test_engine_and_game(void) {
    CHECK_INT(fresh_home(), 0);
    engines();
    char *ini = home_file("config/dxlauncher/launcher.ini");
    dxl_ini *li = dxl_ini_load(ini, NULL);
    CHECK(li != NULL);
    if (!li) return;
    char *glibc = home_file("engines/glibc/DXEngine"), *good = home_file("engines/good/DXEngine");
    dxl_ini_set(li, "Launcher", "Engine", glibc);
    dxl_ini_save(li, ini, NULL);
    dxl_ini_free(li);
    sh("DXL_HOME='%s' %s DXL_UI_SCRIPT='%s/engine_and_game.script' '%s/deusex-launcher' >>'%s/out.txt' 2>&1", home,
       OUR_PADS, DXL_UI_SCRIPTS, app, home);
    script_ok();
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "does not start here: it needs glibc 2.99") != NULL);
    CHECK_INT(count(log, "engine refused:"), 1);
    char line[2048];
    snprintf(line, sizeof line, "engine: %s", good);
    CHECK(strstr(log, line) != NULL);
    CHECK(strstr(log, "profile default: install") != NULL);
    CHECK(strstr(log, "engine: the launcher's own") != NULL);
    free(log);
    li = dxl_ini_load(ini, NULL);
    CHECK(li && dxl_ini_get(li, "Launcher", "Engine") == NULL);
    dxl_ini_free(li);
    free(ini);
    free(glibc);
    free(good);
}

/* An engine this system cannot start, DXL_ENGINE's: at a start that finds
 * Running.ini, Recovery says why and its play key starts nothing; at
 * -changevideo's Graphics, Play is off. No game started, and the launcher
 * ends 0, not 4 with the reason on stderr alone. */
static void test_play_refused(void) {
    CHECK_INT(fresh_home(), 0);
    engines();
    char *glibc = home_file("engines/glibc/DXEngine");
    char *running = home_file("state/dxlauncher/profiles/default/Running.ini");
    CHECK_INT(sh("mkdir -p \"$(dirname '%s')\" && : >'%s'", running, running), 0);
    char env[700];
    snprintf(env, sizeof env, "DXL_ENGINE='%s'", glibc);
    CHECK_INT(launch_with("recovery_engine.script", "ready", env), 0);
    script_ok();
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "does not start here: it needs glibc 2.99") != NULL);
    CHECK(strstr(log, "starting the game") == NULL);
    free(log);
    /* Its Play off, not to be found: the script ends there. */
    CHECK_INT(sh("DXL_HOME='%s' %s %s DXL_UI_SCRIPT='%s/graphics_engine.script' '%s/deusex-launcher' -changevideo "
                 ">>'%s/out.txt' 2>&1",
                 home, OUR_PADS, env, DXL_UI_SCRIPTS, app, home),
              0);
    char *out = read_home("out.txt");
    CHECK(strstr(out, "ui script: FAILED: no Play on settings") != NULL);
    free(out);
    log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "starting the game") == NULL);
    free(log);
    free(running);
    free(glibc);
}

/* The Profiles page: a profile made, named after its name, used; a copy
 * renamed and deleted, its four folders gone; the first in use again. */
static void test_profiles(void) {
    CHECK_INT(fresh_home(), 0);
    launch("profiles.script", "ready");
    script_ok();
    char *made = read_home("config/dxlauncher/profiles/mod-test/profile.ini");
    CHECK(strstr(made, "Name=Mod test") != NULL);
    free(made);
    char *ini = read_home("config/dxlauncher/launcher.ini");
    CHECK(strstr(ini, "Profile=default") != NULL);
    free(ini);
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "profile mod-test made from") != NULL);
    CHECK(strstr(log, "profile deus-ex-copy: a copy of default's settings") != NULL);
    CHECK(strstr(log, "profile deus-ex-copy: named Copy") != NULL);
    CHECK(strstr(log, "profile deus-ex-copy: deleted") != NULL);
    CHECK(strstr(log, "named Wrong") == NULL);
    free(log);
    const char *folders[] = { "config/dxlauncher/profiles/deus-ex-copy", "data/dxlauncher/profiles/deus-ex-copy",
                              "state/dxlauncher/profiles/deus-ex-copy", "cache/dxlauncher/profiles/deus-ex-copy" };
    for (int i = 0; i < 4; i++) {
        char *gone = home_file(folders[i]);
        CHECK(access(gone, F_OK) != 0);
        free(gone);
    }
    char *trash = home_file("data/Trash");
    CHECK(access(trash, F_OK) != 0);
    free(trash);
}

/* A mod of its own made in the launcher's mods folder: its profile made
 * through Mods and Your profile, the one in use; its folders first in the
 * profile's paths, its own save copied. */
static void test_mods(void) {
    CHECK_INT(fresh_home(), 0);
    const char *files[][2] = {
        { "data/dxlauncher/mods/TestMod/System/TestMod.u", "pkg" },
        { "data/dxlauncher/mods/TestMod/System/TestMod.ini",
          "[URL]\r\nClass=TestMod.ModPlayer\r\n\r\n[Core.System]\r\nSavePath=..\\TestMod\\Save\r\n"
          "Paths=..\\TestMod\\System\\*.u\r\nPaths=..\\System\\*.u\r\nPaths=..\\Maps\\*.dx\r\n" },
        { "data/dxlauncher/mods/TestMod/System/TestModUser.ini", "[Engine.Input]\r\nF1=ModsOwn\r\n" },
        { "data/dxlauncher/mods/TestMod/Save/Save0001/SaveInfo.dxs", "a save" },
    };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        char *path = home_file(files[i][0]), *dir = dxl_path_dirname(path);
        dxl_path_mkdirs(dir);
        FILE *f = fopen(path, "wb");
        if (f) {
            fputs(files[i][1], f);
            fclose(f);
        }
        free(dir);
        free(path);
    }
    launch("mods.script", "ready");
    script_ok();
    char *dir = home_file("data/dxlauncher/mods/TestMod"), *real = realpath(dir, NULL);
    char *made = read_home("config/dxlauncher/profiles/testmod/profile.ini"), want[1200];
    snprintf(want, sizeof want, "Mod=%s\n", real ? real : dir);
    CHECK(strstr(made, want) != NULL);
    CHECK(strstr(made, "Name=TestMod") != NULL);
    free(made);
    char *ini = read_home("config/dxlauncher/launcher.ini");
    CHECK(strstr(ini, "Profile=testmod") != NULL);
    free(ini);
    char *sys_path = home_file("config/dxlauncher/profiles/testmod/DeusEx.ini");
    dxl_ini *sys = dxl_ini_load(sys_path, NULL);
    snprintf(want, sizeof want, "%s/System/*.u", real ? real : dir);
    CHECK_STR(first_path(sys), want);
    CHECK_STR(value(sys, "URL", "Class"), "TestMod.ModPlayer");
    dxl_ini_free(sys);
    free(sys_path);
    char *save = home_file("data/dxlauncher/profiles/testmod/Save/Save0001/SaveInfo.dxs");
    CHECK(access(save, F_OK) == 0);
    free(save);
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "profile testmod made from") != NULL);
    CHECK(strstr(log, " and the mod ") != NULL);
    free(log);
    free(real);
    free(dir);
}

/* The engine that starts on a save, as DXEngine's --version says it. */
#define STARTS_ON_SAVES "DXL_STUB_FEATURES='userdir loadgame-url'"

/* The first run's command line holds want. */
static void started_with(const char *want) {
    char *log = read_home("state/dxlauncher/launcher.log");
    char line[2048];
    line_with(log, "starting the game:", 0, line, sizeof line);
    if (!strstr(line, want)) fprintf(stderr, "  no %s in: %s\n", want, line);
    CHECK(strstr(line, want) != NULL);
    free(log);
}

/* The profile's saves, as the game writes them: Save0010 on Liberty
 * Island, and with newer, a newer one in a lower slot, Save0003. */
static void write_saves(int newer) {
    char *saves = home_file("data/dxlauncher/profiles/default/Save");
    dxl_test_save island = { { 2026, 10, 14, 20, 47, 15 }, 10, "NYC - Liberty Island", "NYC - Liberty Island",
                             "01_NYC_UNATCOIsland", 452, 1 };
    CHECK_INT(dxl_test_write_save(saves, "Save0010", &island), 0);
    dxl_test_save park = { { 2026, 10, 16, 9, 0, 0 }, 3, "Battery Park", "NYC - Battery Park", "03_NYC_BatteryPark",
                           600, 1 };
    if (newer) CHECK_INT(dxl_test_write_save(saves, "Save0003", &park), 0);
    free(saves);
}

/* ---- a master and a server on this machine -------------------------- */

static int local_socket(int type, int *port) {
    int fd = socket(AF_INET, type, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof sa;
    if (fd < 0 || bind(fd, (struct sockaddr *)&sa, sizeof sa) != 0 || getsockname(fd, (struct sockaddr *)&sa, &len) != 0)
        return -1;
    *port = ntohs(sa.sin_port);
    return fd;
}

/* Waits up to 60 s for fd: 1 ready. */
static int ready(int fd) {
    struct pollfd p = { fd, POLLIN, 0 };
    return poll(&p, 1, 60000) == 1;
}

typedef struct {
    int master, server, server_port, master_port;
    int password;      /* the server asks for one */
    pthread_t mt, st;
} fakes;

/* The master: one look, the server listed. */
static void *fake_master(void *p) {
    fakes *f = p;
    if (!ready(f->master)) return NULL;
    int c = accept(f->master, NULL, NULL);
    if (c < 0) return NULL;
    send(c, "\\basic\\\\secure\\abcdef", 21, 0);
    char asked[512] = "";
    size_t len = 0;
    while (len < sizeof asked - 1 && !strstr(asked, "\\list\\") && ready(c)) {
        ssize_t n = recv(c, asked + len, sizeof asked - 1 - len, 0);
        if (n <= 0) break;
        len += (size_t)n;
        asked[len] = '\0';
    }
    char list[128];
    snprintf(list, sizeof list, "\\ip\\127.0.0.1:%d\\final\\", f->server_port);
    send(c, list, strlen(list), 0);
    close(c);
    return NULL;
}

/* The server: its status, whole, for each asking until a minute passes. */
static void *fake_server(void *p) {
    fakes *f = p;
    char query[256];
    while (ready(f->server)) {
        struct sockaddr_in from;
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(f->server, query, sizeof query - 1, 0, (struct sockaddr *)&from, &len);
        if (n <= 0) break;
        char st[400];
        snprintf(st, sizeof st,
                 "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\hostname\\Fake One\\hostport\\7790"
                 "\\mapname\\DXMP_Cathedral\\gametype\\DeathMatchGame\\numplayers\\0\\maxplayers\\8%s"
                 "\\final\\\\queryid\\1.1",
                 f->password ? "\\password\\True" : "");
        sendto(f->server, st, strlen(st), 0, (struct sockaddr *)&from, len);
    }
    return NULL;
}

/* The profile's master the one here. */
static void master_here(int port) {
    profile_files f;
    open_files(&f);
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", port);
    dxl_ini_set(f.sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress", at);
    CHECK_INT(dxl_ini_save(f.sys, f.p.system_ini, NULL), 0);
    close_files(&f);
}

/* A master and its server here, the profile's master the one. */
static void fakes_start(fakes *f, int password) {
    memset(f, 0, sizeof *f);
    f->password = password;
    f->master = local_socket(SOCK_STREAM, &f->master_port);
    listen(f->master, 4);
    f->server = local_socket(SOCK_DGRAM, &f->server_port);
    master_here(f->master_port);
    pthread_create(&f->mt, NULL, fake_master, f);
    pthread_create(&f->st, NULL, fake_server, f);
}

static void fakes_stop(fakes *f) {
    shutdown(f->master, SHUT_RDWR);
    shutdown(f->server, SHUT_RDWR);
    pthread_join(f->mt, NULL);
    pthread_join(f->st, NULL);
    close(f->master);
    close(f->server);
}

/* Multiplayer's servers, a master's here: the one listed joined; an
 * address left half typed joining nothing, then typed and joined; a link's
 * server asked about and joined; a server's password typed and joined,
 * the engine's, not the log's. Each on the engine's command line as its
 * --url. */
static void test_multiplayer(void) {
    CHECK_INT(fresh_home(), 0);
    fakes f;
    fakes_start(&f, 0);
    CHECK_INT(launch("multiplayer.script", "ready"), 0);
    script_ok();
    started_with("--url=127.0.0.1:7790");
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "multiplayer: 1 server(s), 1 answered") != NULL);
    free(log);
    fakes_stop(&f);

    /* On a pad, Refresh reached from Sort by the d-pad: the look again. */
    CHECK_INT(fresh_home(), 0);
    fakes_start(&f, 0);
    CHECK_INT(launch("multiplayer_pad.script", "ready"), 0);
    script_ok();
    log = read_home("state/dxlauncher/launcher.log");
    CHECK_INT(count(log, "multiplayer: asking"), 2);
    free(log);
    fakes_stop(&f);

    CHECK_INT(fresh_home(), 0);
    master_here(1);   /* no master there: the address alone */
    CHECK_INT(launch("multiplayer_address.script", "ready"), 0);
    script_ok();
    started_with("--url=127.0.0.1:7999");
    log = read_home("state/dxlauncher/launcher.log");
    CHECK_INT(count(log, "starting the game:"), 1);
    free(log);

    CHECK_INT(fresh_home(), 0);
    master_here(1);
    CHECK_INT(sh("DXL_HOME='%s' DXL_ENGINE='%s' DXL_STUB=ready DXL_STUB_MS=300 %s DXL_UI_SCRIPT='%s/multiplayer_link.script' "
                 "'%s/deusex-launcher' 'deusex://127.0.0.1:7998/' >>'%s/out.txt' 2>&1",
                 home, DXL_STUB_ENGINE, OUR_PADS, DXL_UI_SCRIPTS, app, home),
              0);
    script_ok();
    started_with("--url=127.0.0.1:7998");

    CHECK_INT(fresh_home(), 0);
    fakes_start(&f, 1);
    CHECK_INT(launch_with("multiplayer_password.script", "ready", "DXL_STUB_ARGS=1"), 0);
    script_ok();
    started_with("--url=127.0.0.1:7790?password=******");
    /* The script's own line has it; the launcher's none. */
    log = read_home("state/dxlauncher/launcher.log");
    CHECK_INT(count(log, "sesame"), count(log, "ui script: type sesame"));
    free(log);
    char *run = read_home("state/dxlauncher/profiles/default/logs/last-run.log");
    CHECK(strstr(run, "[--url=127.0.0.1:7790?password=sesame]") != NULL);
    free(run);
    fakes_stop(&f);
}

/* Continue from Home: the latest save, by when it was made, not by its
 * slot, in the engine's --url. */
static void test_continue(void) {
    CHECK_INT(fresh_home(), 0);
    write_saves(1);
    launch_with("continue.script", "ready", STARTS_ON_SAVES);
    script_ok();
    started_with("--url=?loadgame=3");
}

/* The Saves page: a save loaded by its row; then deleted, for good. */
static void test_saves_page(void) {
    CHECK_INT(fresh_home(), 0);
    write_saves(0);
    launch_with("saves_load.script", "ready", STARTS_ON_SAVES);
    script_ok();
    started_with("--url=?loadgame=10");
    launch_with("saves_delete.script", "ready", STARTS_ON_SAVES);
    script_ok();
    char *gone = home_file("data/dxlauncher/profiles/default/Save/Save0010");
    CHECK(access(gone, F_OK) != 0);
    free(gone);
    char *trash = home_file("data/Trash");
    CHECK(access(trash, F_OK) != 0);
    free(trash);
}

/* A picture of 2 x 2 pixels, a 24-bit BMP as the engine writes its shots. */
static void write_bmp(const char *path) {
    unsigned char b[70];
    memset(b, 0, sizeof b);
    const unsigned head[][2] = { { 2, 70 }, { 10, 54 }, { 14, 40 }, { 18, 2 }, { 22, 2 }, { 34, 16 } };
    b[0] = 'B';
    b[1] = 'M';
    for (size_t i = 0; i < sizeof head / sizeof *head; i++)
        for (int k = 0; k < 4; k++) b[head[i][0] + k] = (unsigned char)(head[i][1] >> (8 * k));
    b[26] = 1;
    b[28] = 24;
    for (int i = 54; i < 70; i++) b[i] = (unsigned char)(i * 13);
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(b, 1, sizeof b, f);
        fclose(f);
    }
}

/* The gallery: the profile's shot shown whole, deleted from there for
 * good. */
static void test_shots_page(void) {
    CHECK_INT(fresh_home(), 0);
    char *shot = home_file("data/dxlauncher/profiles/default/Screenshots/Shot0001.bmp");
    write_bmp(shot);
    launch("shots.script", "ready");
    script_ok();
    CHECK(access(shot, F_OK) != 0);
    free(shot);
    char *log = read_home("state/dxlauncher/launcher.log");
    CHECK(strstr(log, "Shot0001.bmp deleted") != NULL);
    free(log);
}

/* A first run from nothing on the script, a controller connected: the
 * graphics preset and the pad layout the profile was made with, by their
 * ids ("" for none of them). */
static void first_run(const char *script, char *looks, char *layout) {
    looks[0] = layout[0] = '\0';
    sh("rm -rf '%s' && mkdir -p '%s'", home, home);
    CHECK_INT(sh("DXL_HOME='%s' DXL_ENGINE='%s' DXL_STUB=ready %s DXL_UI_SCRIPT='%s/%s' "
                 "'%s/deusex-launcher' --game='%s' >>'%s/out.txt' 2>&1",
                 home, DXL_STUB_ENGINE, OUR_PADS, DXL_UI_SCRIPTS, script, app, DXL_GAME, home),
              0);
    script_ok();
    profile_files b;
    open_files(&b);
    CHECK(b.sys && b.usr && b.es);
    if (b.sys && b.usr && b.es) {
        dxl_stores st;
        memset(&st, 0, sizeof st);
        st.system = b.sys;
        st.user = b.usr;
        st.engine = b.es;
        const dxl_preset *p = dxl_preset_matching(&st);
        const dxl_pad_preset *l = dxl_bindings_detect(b.usr);
        snprintf(looks, 32, "%s", p ? p->id : "");
        snprintf(layout, 32, "%s", l ? l->id : "");
    }
    close_files(&b);
}

/* Your profile's quick setup: as chosen; and as it comes, the engine's own
 * graphics -- a preset of none of ours -- and Modern over the game's three
 * buttons. */
static void test_first_run(void) {
    char looks[32], layout[32];
    first_run("first_run.script", looks, layout);
    CHECK_STR(looks, "enhanced");
    CHECK_STR(layout, "southpaw");
    first_run("first_run_defaults.script", looks, layout);
    CHECK_STR(looks, "");
    CHECK_STR(layout, "modern");
}

int dxl_test_failures = 0;
const char *dxl_test_current = "(none)";

int main(void) {
    const char *wayland = getenv("WAYLAND_DISPLAY"), *x11 = getenv("DISPLAY");
    if (!(wayland && *wayland) && !(x11 && *x11)) {
        printf("skipped: no display\n");
        return 77;
    }
    char exe[600];
    snprintf(exe, sizeof exe, "%s/System/DeusEx.exe", DXL_GAME);
    if (access(exe, F_OK) != 0) {
        printf("skipped: no install in %s\n", DXL_GAME);
        return 77;
    }
    /* No beacon of the local network's in the scripts' server lists: what
     * a server on this network says changes no script. */
    setenv("DXL_LAN", "none", 1);
    snprintf(app, sizeof app, "/tmp/dxl-uiapp-%d", (int)getpid());
    snprintf(home, sizeof home, "/tmp/dxl-uihome-%d", (int)getpid());
    /* The programs copied, so each finds data/ beside itself. */
    if (sh("rm -rf '%s' && mkdir -p '%s' && cp '%s' '%s' '%s/' && ln -s '%s' '%s/data'", app, app, DXL_LAUNCHER,
           DXL_CLI, app, DXL_DATA, app) != 0) {
        fprintf(stderr, "cannot make %s\n", app);
        return 1;
    }
    RUN(test_settings_roundtrip);
    RUN(test_settings_kept);
    RUN(test_recovery);
    RUN(test_remap_pad);
    RUN(test_pad_test);
    RUN(test_engine_and_game);
    RUN(test_play_refused);
    RUN(test_profiles);
    RUN(test_mods);
    RUN(test_multiplayer);
    RUN(test_continue);
    RUN(test_saves_page);
    RUN(test_shots_page);
    RUN(test_first_run);
    if (!dxl_test_failures) sh("rm -rf '%s' '%s'", app, home);
    else fprintf(stderr, "kept for a look: %s, %s\n", app, home);
    if (dxl_test_failures) {
        fprintf(stderr, "%d check(s) failed\n", dxl_test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
