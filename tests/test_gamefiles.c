#include "test.h"
#include "scratch.h"
#include "app/run.h"
#include "core/config.h"
#include "core/ini.h"
#include "core/install.h"
#include "core/localize.h"
#include "core/paths.h"
#include "core/peicon.h"
#include "core/profile.h"

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

/* The checks tests/fixtures can only stand in for, run against a real install
 * in gamefiles/System: the byte-identical round trip, and the values
 * dx-reverse-info/ describes. The game's files are not part of this
 * repository, so without an install there this test is skipped (exit 77). */

#ifndef DXL_GAMEFILES
#define DXL_GAMEFILES "gamefiles/System"
#endif

static char *game_file(const char *name) {
    size_t n = strlen(DXL_GAMEFILES) + strlen(name) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", DXL_GAMEFILES, name);
    return p;
}

/* DeusEx.ini and User.ini only exist once the game has run; the defaults it
 * builds them from always do. */
static void test_roundtrip(void) {
    const char *names[] = { "Default.ini", "DefUser.ini", "DeusEx.ini", "User.ini" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char *path = game_file(names[i]);
        size_t orig_len = 0;
        char *orig = slurp(path, &orig_len);
        if (!orig && i >= 2) { free(path); continue; }
        CHECK(orig != NULL);
        dxl_ini *ini = orig ? dxl_ini_load(path, NULL) : NULL;
        if (ini) {
            size_t out_len = 0;
            char *out = dxl_ini_render(ini, &out_len);
            CHECK_INT(out_len, orig_len);
            CHECK(out_len == orig_len && memcmp(out, orig, orig_len) == 0);
            free(out);
            dxl_ini_free(ini);
        }
        free(orig);
        free(path);
    }
}

static void test_default_ini_matches_the_spec(void) {
    char *path = game_file("Default.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini != NULL);
    if (!ini) return;

    /* dx-reverse-info/wizard.md: a pristine install ships FirstRun=0, so the
     * first-time flow always runs. */
    CHECK_INT(dxl_ini_get_int(ini, "FirstRun", "FirstRun", -1), 0);
    /* dx-reverse-info/launch-flow.md section 8: CdPath=..\ makes the CD check pass. */
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "CdPath"), "..\\");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameEngine"), "DeusEx.DeusExGameEngine");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice"),
              "GlideDrv.GlideRenderDevice");
    CHECK_INT(dxl_ini_get_all(ini, "Core.System", "Paths", NULL, 0), 5);

    /* dx-reverse-info/ini-keys.md: DescFlags and Description are runtime values that
     * appear in no shipped ini. If these ever start existing, the detection
     * story in the docs is wrong. */
    CHECK(dxl_ini_get(ini, "D3DDrv.D3DRenderDevice", "Description") == NULL);
    CHECK(dxl_ini_get(ini, "D3DDrv.D3DRenderDevice", "DescFlags") == NULL);
    dxl_ini_free(ini);
}

/* The words the pages show, from the game's own .int files. */
static void test_strings(void) {
    dxl_loc *l = dxl_loc_open(DXL_GAMEFILES, NULL);
    CHECK_STR(dxl_loc_general(l, "Startup", "FirstTime"), "Deus Ex First-Time Configuration");
    CHECK_STR(dxl_loc_general(l, "Startup", "Run"), "Run!");
    CHECK_STR(dxl_loc_general(l, "Startup", "WorldHigh"), "High detail textures");   /* unquoted */
    CHECK_STR(dxl_loc_general(l, "Window", "NextButton"), "&Next >");
    CHECK_STR(dxl_loc_general(l, "Window", "InsertCdTitle"), "Cd Required At Startup");
    CHECK_STR(dxl_loc_get(l, "Startup", "IDDIALOG_ConfigPageRenderer", "IDC_RenderNote", 0), "");
    dxl_loc_free(l);
}

/* The install: a build the engine plays, nothing missing. */
static void test_install(void) {
    dxl_install in;
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    CHECK_INT(dxl_install_check(root, &in), 0);
    CHECK_INT(in.found, 1);
    CHECK(in.build != NULL);
    if (in.build) CHECK_STR(in.build->version, "1112fm");
    CHECK_STR(in.missing, "");
    dxl_install_free(&in);
    free(root);
}

/* What a first run would make of the install, nothing written. */
static void test_import_plan(void) {
    setenv("DXL_HOME", "/nonexistent/dxl-home", 1);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    dxl_import r;
    CHECK_INT(dxl_profile_import(&p, DXL_GAMEFILES, 1, &r, NULL), 0);
    CHECK(strstr(r.system_from, "DeusEx.ini") || strstr(r.system_from, "Default.ini"));
    CHECK(r.user_from[0] != '\0');
    CHECK_INT(dxl_path_exists("/nonexistent/dxl-home"), 0);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    unsetenv("DXL_HOME");
}

/* Every file under the game's folder changed since mark: none may be. The
 * IDA databases beside the game's binaries are not the game's, and an IDA
 * session may write them at any time. */
static int changed_since(const char *mark) {
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    char cmd[1200];
    snprintf(cmd, sizeof cmd,
             "find '%s' -newer '%s' ! -name '*.i64' ! -name '*.id[0-2]' ! -name '*.nam' ! -name '*.til'"
             " -print | head -5", root, mark);
    FILE *f = popen(cmd, "r");
    int n = 0;
    char line[600];
    while (f && fgets(line, sizeof line, f)) {
        fprintf(stderr, "  changed: %s", line);
        n++;
    }
    if (f) pclose(f);
    free(root);
    return n;
}

static int run_launcher(const char *game, const char *word) {
    char opt[700];
    snprintf(opt, sizeof opt, "--game=%s", game);
    char *argv[] = { "deusex-launcher", opt, (char *)word, NULL };
    static volatile int stop;
    return dxl_run_main(word ? 3 : 2, argv, NULL, &stop);
}

/* A whole start with no window, the stub in the engine's place, in a
 * scratch home: the first run makes the profile from the install and plays;
 * a crash leaves Running.ini, and the next start stops at Recovery, which
 * has no window. Nothing is written to the game's folder. */
static void test_headless_run(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-run"));
    char *mark = scratch_path(home, "mark");
    scratch_write(home, "mark", "");
    sleep(1);   /* the mark older than anything the run could write */
    setenv("DXL_HOME", home, 1);
    setenv("DXL_ENGINE", DXL_STUB_ENGINE, 1);
    char *root = dxl_path_dirname(DXL_GAMEFILES);

    setenv("DXL_STUB", "ready", 1);
    setenv("DXL_STUB_MS", "200", 1);
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    CHECK_INT(dxl_profile_exists(&p), 1);
    char *sentinel = dxl_path_join(p.state_dir, "Running.ini");
    CHECK_INT(dxl_path_exists(sentinel), 0);
    dxl_lastrun last;
    CHECK_INT(dxl_lastrun_load(&last, p.state_dir), 0);
    CHECK_INT(last.clean, 1);
    /* The engine's line: the profile's files, the game's folder. */
    char *log = dxl_path_join(p.logs_dir, DXL_LASTRUN_LOG);
    CHECK_INT(dxl_path_exists(log), 1);

    setenv("DXL_STUB", "crash", 1);
    CHECK_INT(run_launcher(root, NULL), 128 + SIGSEGV);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    dxl_lastrun_load(&last, p.state_dir);
    CHECK_INT(last.signal, SIGSEGV);

    setenv("DXL_STUB", "ready", 1);
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_SCREEN);
    CHECK_INT(dxl_path_exists(sentinel), 1);

    /* An engine without the userdir feature is refused. */
    unlink(sentinel);
    setenv("DXL_STUB_FEATURES", "loadgame-url", 1);
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_NOGAME);
    unsetenv("DXL_STUB_FEATURES");

    CHECK_INT(changed_since(mark), 0);

    free(log);
    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    unsetenv("DXL_STUB");
    unsetenv("DXL_STUB_MS");
    unsetenv("DXL_ENGINE");
    unsetenv("DXL_HOME");
    free(mark);
    scratch_remove(home);
    free(home);
}

/* A window that plays from its first Home and quits at the next, looking
 * at the inis then. */
static int views, saw_rewrite;

static int home_then_quit(void *ctx, dxl_run *r, dxl_view which) {
    (void)ctx;
    if (which != DXL_VIEW_HOME) return 0;
    if (++views == 1) return 1;
    saw_rewrite = r->config && dxl_ini_get(dxl_config_user_ini(r->config), "DXL.Stub", "Wrote") != NULL;
    return 0;
}

/* Back here after playing: Home after a clean end, the inis as the engine
 * left them; with DXL_NO_HOME=1, no Home and one run. */
static void test_back_home(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-back"));
    char *count = scratch_path(home, "runs");
    setenv("DXL_HOME", home, 1);
    setenv("DXL_ENGINE", DXL_STUB_ENGINE, 1);
    setenv("DXL_STUB", "ready", 1);
    setenv("DXL_STUB_MS", "200", 1);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    char *launcher_ini = dxl_path_join(x.config, "launcher.ini");
    dxl_ini *li = dxl_ini_load(launcher_ini, NULL);
    CHECK(li != NULL);
    if (li) {
        dxl_ini_set(li, "Launcher", "ReturnAfterPlaying", "True");
        CHECK_INT(dxl_ini_save(li, launcher_ini, NULL), 0);
        dxl_ini_free(li);
    }

    setenv("DXL_STUB", "rewrite", 1);
    setenv("DXL_STUB_COUNT", count, 1);
    dxl_run_ui ui = { .view = home_then_quit };
    char *argv[] = { "deusex-launcher", NULL };
    static volatile int stop;
    views = saw_rewrite = 0;
    CHECK_INT(dxl_run_main(1, argv, &ui, &stop), 0);
    CHECK_INT(views, 2);
    CHECK_INT(saw_rewrite, 1);
    size_t len = 0;
    char *runs = slurp(count, &len);
    CHECK_STR(runs, "x");
    free(runs);

    setenv("DXL_NO_HOME", "1", 1);
    views = 0;
    CHECK_INT(dxl_run_main(1, argv, &ui, &stop), 0);
    CHECK_INT(views, 0);
    runs = slurp(count, &len);
    CHECK_STR(runs, "xx");
    free(runs);
    unsetenv("DXL_NO_HOME");

    dxl_xdg_free(&x);
    free(launcher_ini);
    free(root);
    unsetenv("DXL_STUB_COUNT");
    unsetenv("DXL_STUB");
    unsetenv("DXL_STUB_MS");
    unsetenv("DXL_ENGINE");
    unsetenv("DXL_HOME");
    free(count);
    scratch_remove(home);
    free(home);
}

/* dx-reverse-info/wizard.md, the frame: the wizard's icon is DeusEx.exe's
 * group 128, the only one it has. For 32 its 32x32 of 256 colours is taken,
 * square and every pixel in, a near-black corner and a blue centre; for 16
 * its 16x16 of 16. */
static void test_exe_icon(void) {
    char *path = game_file("DeusEx.exe");
    dxl_icon icon;
    CHECK_INT(dxl_pe_icon(path, 128, 32, &icon, NULL), 0);
    CHECK_INT(icon.w, 32);
    CHECK_INT(icon.h, 32);
    if (icon.argb) {
        CHECK_INT(icon.argb[0], 0xff040404);
        CHECK_INT(icon.argb[16 * 32 + 16], 0xff3333cc);
    }
    dxl_icon_free(&icon);
    CHECK_INT(dxl_pe_icon(path, 128, 16, &icon, NULL), 0);
    CHECK_INT(icon.w, 16);
    if (icon.argb) CHECK_INT(icon.argb[8 * 16 + 8], 0xff000080);
    dxl_icon_free(&icon);
    CHECK_INT(dxl_pe_icon(path, 129, 32, &icon, NULL), -1);
    CHECK(icon.argb == NULL);
    free(path);
}

TEST_MAIN_BEGIN
    if (access(DXL_GAMEFILES "/Default.ini", R_OK) != 0) {
        printf("skipped: no Deus Ex install in %s\n", DXL_GAMEFILES);
        return 77;
    }
    RUN(test_roundtrip);
    RUN(test_default_ini_matches_the_spec);
    RUN(test_strings);
    RUN(test_exe_icon);
    RUN(test_install);
    RUN(test_import_plan);
    RUN(test_headless_run);
    RUN(test_back_home);
TEST_MAIN_END
