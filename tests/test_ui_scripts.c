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
#include "core/keys.h"
#include "core/paths.h"
#include "core/profile.h"
#include "core/xdg.h"

#include <math.h>
#include <stdarg.h>
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

/* The launcher on the script, the stub doing mode; its exit code. */
static int launch(const char *script, const char *mode) {
    return sh("DXL_HOME='%s' DXL_ENGINE='%s' DXL_STUB=%s DXL_STUB_MS=300 DXL_UI_SCRIPT='%s/%s' "
              "'%s/deusex-launcher' >>'%s/out.txt' 2>&1",
              home, DXL_STUB_ENGINE, mode, DXL_UI_SCRIPTS, script, app, home);
}

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
    char was_size[64], was_aa[32], was_theme[64], was_c[128], crouch[32] = "";
    snprintf(was_c, sizeof was_c, "%s", value(a.usr, "Engine.Input", "C"));
    const char *keys[2];
    if (dxl_keys_bound(a.usr, "Duck", keys, 2) > 0) snprintf(crouch, sizeof crouch, "%s", keys[0]);
    snprintf(was_size, sizeof was_size, "%sx%s", value(a.sys, "Engine.SurrealClient", "WindowedViewportX"),
             value(a.sys, "Engine.SurrealClient", "WindowedViewportY"));
    snprintf(was_aa, sizeof was_aa, "%s", dxl_es_choice(a.es, DXL_ES_ANTIALIAS));
    snprintf(was_theme, sizeof was_theme, "%s", value(a.usr, "DeusEx.DeusExPlayer", "MenuThemeName"));

    CHECK_INT(launch("settings_roundtrip.script", "ready"), 0);
    script_ok();
    open_files(&b);
    CHECK(b.sys && b.usr);
    if (!b.sys || !b.usr) {
        close_files(&a);
        return;
    }

    /* Display. */
    CHECK_INT(dxl_ini_get_bool(b.sys, "Engine.SurrealClient", "StartupFullscreen", -1),
              !dxl_ini_get_bool(a.sys, "Engine.SurrealClient", "StartupFullscreen", -1));
    char size[64];
    snprintf(size, sizeof size, "%sx%s", value(b.sys, "Engine.SurrealClient", "WindowedViewportX"),
             value(b.sys, "Engine.SurrealClient", "WindowedViewportY"));
    CHECK(strcmp(size, was_size) != 0);
    double fov = atof(value(a.usr, "Engine.PlayerPawn", "DesiredFOV")) + 5;
    if (fov > 120) fov = 120;
    CHECK(atof(value(b.usr, "Engine.PlayerPawn", "DesiredFOV")) == fov);
    CHECK(atof(value(b.usr, "Engine.PlayerPawn", "DefaultFOV")) == fov);
    /* Graphics. */
    CHECK(strcmp(dxl_es_choice(b.es, DXL_ES_ANTIALIAS), was_aa) != 0);
    /* Audio: two steps of 0.02 down, as a byte. */
    double music = atoi(value(a.sys, "Engine.SurrealAudioDevice", "MusicVolume")) / 255.0 - 0.04;
    if (music < 0) music = 0;
    char want[16];
    snprintf(want, sizeof want, "%ld", lround(music * 255));
    CHECK_STR(value(b.sys, "Engine.SurrealAudioDevice", "MusicVolume"), want);
    /* Keyboard: J for Jump, in its second slot, the key there before let
     * go; C for Jump as well, in its first, Space let go; Crouch's first
     * key gone. */
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
    char line[2048];
    line_with(log, "starting the game:", 0, line, sizeof line);
    CHECK(strstr(line, "-nohard -noddraw") != NULL);
    CHECK(strstr(log, "the game ended: 0, cleanly") != NULL);
    free(log);
    close_files(&a);
    close_files(&b);
}

/* After a crash: Recovery, its log page, the next run in a window, another
 * renderer; Play; the window's words on that run alone. */
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
    CHECK(strstr(second, "-nohard -noddraw") != NULL);
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
    snprintf(app, sizeof app, "/tmp/dxl-uiapp-%d", (int)getpid());
    snprintf(home, sizeof home, "/tmp/dxl-uihome-%d", (int)getpid());
    /* The programs copied, so each finds data/ beside itself. */
    if (sh("rm -rf '%s' && mkdir -p '%s' && cp '%s' '%s' '%s/' && ln -s '%s' '%s/data'", app, app, DXL_LAUNCHER,
           DXL_CLI, app, DXL_DATA, app) != 0) {
        fprintf(stderr, "cannot make %s\n", app);
        return 1;
    }
    RUN(test_settings_roundtrip);
    RUN(test_recovery);
    if (!dxl_test_failures) sh("rm -rf '%s' '%s'", app, home);
    else fprintf(stderr, "kept for a look: %s, %s\n", app, home);
    if (dxl_test_failures) {
        fprintf(stderr, "%d check(s) failed\n", dxl_test_failures);
        return 1;
    }
    printf("ok\n");
    return 0;
}
