#include "test.h"
#include "scratch.h"
#include "ends.h"
#include "app/run.h"
#include "core/bindings.h"
#include "core/config.h"
#include "core/ini.h"
#include "core/install.h"
#include "core/paths.h"
#include "core/profile.h"
#include "core/saves.h"
#include "core/theme_colors.h"
#include "core/upkg_assets.h"
#include "platform/instance.h"
#include "platform/os.h"

#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

/* The shipped DefUser.ini carries the classic layout's Joy lines, which the
 * first run's switch recognises (test_bindings' fixture copies them). */
static void test_shipped_bindings(void) {
    char *path = game_file("DefUser.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    CHECK(ini != NULL);
    if (ini) {
        const dxl_pad_preset *p = dxl_bindings_detect(ini);
        CHECK(p != NULL);
        if (p) CHECK_STR(p->id, "classic");
        dxl_ini_free(ini);
    }
    free(path);
}

/* DeusEx.ini and User.ini exist once the game has run, or as Steam ships
 * them, copies of the defaults; the defaults always do. */
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
    /* dx-reverse-info/launch-flow.md section 8: the CdPath an edition ships
     * makes the CD check pass -- GOG's ..\, the game's own Textures\;
     * Steam's ..\GOTY_1, a folder of its own with a Textures\Palettes.utx. */
    dxl_install in;
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_install_check(root, &in);
    int steam = in.build && !strcmp(in.build->edition, "Steam");
    const char *cd = dxl_ini_get(ini, "Engine.Engine", "CdPath");
    CHECK_STR(cd, steam ? "..\\GOTY_1" : "..\\");
    char *rel = dxl_path_from_ini(cd ? cd : "");
    char *base = dxl_path_join(DXL_GAMEFILES, rel);
    char *palette = dxl_path_resolve_ci(base, "Textures/Palettes.utx");
    CHECK(palette != NULL && dxl_path_size(palette) > 0);
    free(palette);
    free(base);
    free(rel);
    dxl_install_free(&in);
    free(root);
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
    dxl_env_set("DXL_HOME", "/nonexistent/dxl-home");
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
    dxl_env_set("DXL_HOME", NULL);
}

/* What is under a folder: each folder by its path, each file by its path,
 * size and time, sorted. The IDA databases beside the game's binaries are
 * not the game's: an IDA session writes them, makes and takes away their
 * companions at any time, and so changes their folder's own time, which is
 * no file's (bugs.md LAPP.17). */
typedef struct {
    char **line;
    size_t n, cap;
} tree;

static void tree_walk(const char *dir, tree *t) {
    static const char *const ida[] = { ".i64", ".id0", ".id1", ".id2", ".nam", ".til" };
    DIR *d = opendir(dir);
    struct dirent *e;
    while (d && (e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        const char *ext = strrchr(e->d_name, '.');
        int skip = 0;
        for (size_t i = 0; ext && i < sizeof ida / sizeof *ida; i++) skip |= !strcmp(ext, ida[i]);
        if (skip) continue;
        char *p = dxl_path_join(dir, e->d_name);
        struct stat st;
        if (stat(p, &st) == 0) {
            char line[1200];
            if (S_ISDIR(st.st_mode)) snprintf(line, sizeof line, "%s/", p);
            else snprintf(line, sizeof line, "%s %lld %lld", p, (long long)st.st_size, (long long)st.st_mtime);
            if (t->n == t->cap) {
                t->cap = t->cap ? t->cap * 2 : 256;
                t->line = dxl_xrealloc(t->line, t->cap * sizeof *t->line);
            }
            t->line[t->n++] = dxl_xstrdup(line);
            if (S_ISDIR(st.st_mode)) tree_walk(p, t);
        }
        free(p);
    }
    if (d) closedir(d);
}

static int by_line(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static void tree_take(const char *dir, tree *t) {
    memset(t, 0, sizeof *t);
    tree_walk(dir, t);
    if (t->n) qsort(t->line, t->n, sizeof *t->line, by_line);
}

static void tree_free(tree *t) {
    for (size_t i = 0; i < t->n; i++) free(t->line[i]);
    free(t->line);
    memset(t, 0, sizeof *t);
}

/* What differs between two takes of a folder, each said, at most five: a
 * file or a folder made or taken away, a file written. */
static int tree_changes(const tree *was, const tree *now) {
    int n = 0;
    size_t i = 0, j = 0;
    while (i < was->n || j < now->n) {
        int c = i == was->n ? 1 : j == now->n ? -1 : strcmp(was->line[i], now->line[j]);
        if (!c) {
            i++, j++;
            continue;
        }
        if (n < 5) fprintf(stderr, "  %s: %s\n", c < 0 ? "was" : "now", c < 0 ? was->line[i] : now->line[j]);
        n++;
        if (c < 0) i++;
        else j++;
    }
    return n;
}

/* The walk sees what changed: a file written in a new folder, and the
 * folder; a file written over and one taken away; not an IDA database's
 * companion made and taken away, which changes its folder's time alone. */
static void test_changed_seen(void) {
    char *dir = dxl_xstrdup(scratch_dir("gamefiles-changed"));
    scratch_write(dir, "System/Core.dll", "MZ");
    scratch_write(dir, "System/Core.u", "pkg");
    tree was, now;
    tree_take(dir, &was);
    dxl_os_sleep_ms(1000);
    scratch_write(dir, "System/Core.dll.id0", "");
    char *companion = scratch_path(dir, "System/Core.dll.id0");
    remove(companion);
    free(companion);
    tree_take(dir, &now);
    CHECK_INT(tree_changes(&was, &now), 0);
    tree_free(&now);
    scratch_write(dir, "new/file", "");
    scratch_write(dir, "System/Core.u", "pkg2");
    char *gone = scratch_path(dir, "System/Core.dll");
    remove(gone);
    free(gone);
    tree_take(dir, &now);
    CHECK_INT(tree_changes(&was, &now), 5);   /* new/, new/file, Core.u as it was and is, Core.dll */
    tree_free(&was);
    tree_free(&now);
    scratch_remove(dir);
    free(dir);
}

/* The game's folder over every start below: taken before the first, the
 * same after the last. */
static tree game_was;

static void test_mark_before_the_runs(void) {
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    tree_take(root, &game_was);
    free(root);
    CHECK(game_was.n > 0);
    dxl_os_sleep_ms(1000);
}

static void test_game_folder_untouched(void) {
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    tree now;
    tree_take(root, &now);
    CHECK_INT(tree_changes(&game_was, &now), 0);
    tree_free(&now);
    tree_free(&game_was);
    free(root);
}

static int run_launcher(const char *game, const char *word) {
    char opt[700];
    snprintf(opt, sizeof opt, "--game=%s", game);
    char *argv[] = { "deusex-launcher", opt, (char *)word, NULL };
    static atomic_int stop;
    return dxl_run_main(word ? 3 : 2, argv, NULL, &stop);
}

/* A whole start with no window, the stub in the engine's place, in a
 * scratch home: the first run makes the profile from the install and plays,
 * the engine started on the profile's files and the game's folder; a crash
 * leaves Running.ini, and the next start stops at Recovery, which has no
 * window. */
static void test_headless_run(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-run"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);

    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    dxl_env_set("DXL_STUB_ARGS", "1");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_env_set("DXL_STUB_ARGS", NULL);
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
    char *text = slurp(log, NULL);
    dxl_install in;
    dxl_install_check(root, &in);
    char want[4096];
    snprintf(want, sizeof want, "[%s]\n[--ini=%s]\n[--userini=%s]\n[--userdir=%s]\n", in.root, p.system_ini,
             p.user_ini, p.engine_dir);
    if (!text || !strstr(text, want)) fprintf(stderr, "  not in the engine's log:\n%s", want);
    CHECK(text && strstr(text, want) != NULL);
    free(text);
    dxl_install_free(&in);

    /* The profile's state folder gone, as a home's state removed: made again
     * with the run's Running.ini. */
    CHECK_INT(dxl_fs_remove_tree(p.state_dir), 0);
    dxl_env_set("DXL_STUB", "crash");
    CHECK_INT(run_launcher(root, NULL), END_CRASH_STATUS);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    dxl_lastrun_load(&last, p.state_dir);
    CHECK_INT(last.signal, END_CRASH);

    dxl_env_set("DXL_STUB", "ready");
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_SCREEN);
    CHECK_INT(dxl_path_exists(sentinel), 1);

    /* An engine without the userdir feature is refused. */
    remove(sentinel);
    dxl_env_set("DXL_STUB_FEATURES", "loadgame-url");
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_NOGAME);
    dxl_env_set("DXL_STUB_FEATURES", NULL);

    free(log);
    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A launcher killed while the game plays, with its engine (a session's
 * end, a power cut): Running.ini left, and the run before's record of a
 * clean end gone with the run's start, not taken for this run's; the next
 * start with no window stops at Recovery. */
static void test_killed_mid_game(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-killed"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *sentinel = dxl_path_join(p.state_dir, "Running.ini");
    dxl_lastrun last;
    CHECK_INT(dxl_lastrun_load(&last, p.state_dir), 0);
    CHECK_INT(last.clean, 1);

    dxl_env_set("DXL_STUB", "stubborn");
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        _exit(run_launcher(root, NULL));
    }
    setpgid(pid, pid);
    for (int i = 0; i < 200 && !dxl_path_exists(sentinel); i++) dxl_os_sleep_ms(50);
    dxl_os_sleep_ms(500);   /* the engine up */
    kill(-pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK_INT(dxl_path_exists(sentinel), 1);
    CHECK_INT(dxl_lastrun_load(&last, p.state_dir), -1);

    dxl_env_set("DXL_STUB", "ready");
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_SCREEN);
    CHECK_INT(dxl_path_exists(sentinel), 1);

    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A dedicated server takes no line a second start hands on, as the
 * original's -server: started so, or relaunched to it by the game, its
 * launcher marks itself running with no channel, and a forward finds no
 * one to take it. */
static void test_server_takes_no_handover(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-server"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_session ss;
    CHECK_INT(dxl_session_open(&ss, NULL, NULL), 0);
    char *id = dxl_session_instance_id(&ss);
    char *log = dxl_path_join(ss.xdg.state, "launcher.log");
    dxl_env_set("DXL_STUB_MS", "2500");
    dxl_env_set("DXL_STUB_LINE", "DX.dx -server");
    for (int relaunched = 0; relaunched < 2; relaunched++) {
        dxl_env_set("DXL_STUB", relaunched ? "relaunch" : "ready");
        /* The run before's log gone, its start not taken for this one's. */
        remove(log);
        pid_t pid = fork();
        if (pid == 0) _exit(run_launcher(root, relaunched ? NULL : "-server"));
        /* The server's engine up: its start the run's, or the relaunch's. */
        int starts = 0;
        for (int i = 0; i < 200 && starts < 1 + relaunched; i++) {
            dxl_os_sleep_ms(50);
            char *text = slurp(log, NULL);
            starts = 0;
            for (const char *p = text; p && (p = strstr(p, "starting the game:")); p++) starts++;
            free(text);
        }
        dxl_os_sleep_ms(300);
        CHECK_INT(starts, 1 + relaunched);
        CHECK_INT(dxl_instance_other_running(NULL, id), 1);
        dxl_err e;
        CHECK(dxl_instance_forward(id, "-nosound", 1000, &e) != 0);
        int status = 0;
        waitpid(pid, &status, 0);
        CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    free(log);
    free(id);
    dxl_session_close(&ss);
    free(root);
    dxl_env_set("DXL_STUB_LINE", NULL);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* Every launcher marks itself running, the first and one started beside
 * it, as the original's every instance keeps its mutex: with the first
 * gone, a start finds the one beside it live, whose Running.ini is no
 * crash -- Home, not Recovery, and the file left at a quit -- and with that
 * one gone too, Recovery (bugs.md LAPP.11). The two launchers are this
 * process's marks, the start's words a bypass word's. */
static int beside_view, beside_views;
static int note_view(void *ctx, dxl_run *r, dxl_view which) {
    (void)ctx;
    (void)r;
    beside_view = (int)which;
    beside_views++;
    return 0;
}
static void test_beside_outlives_the_first(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-beside"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_session ss;
    CHECK_INT(dxl_session_open(&ss, NULL, NULL), 0);
    char *id = dxl_session_instance_id(&ss);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *sentinel = dxl_path_join(p.state_dir, "Running.ini");

    /* The first at Home; one beside it playing, its Running.ini made; the
     * first quits. */
    dxl_instance *first = dxl_instance_acquire(id);
    dxl_instance *beside = dxl_instance_acquire(id);
    CHECK(first != NULL && beside != NULL);
    FILE *f = fopen(sentinel, "wb");
    if (f) fclose(f);
    dxl_instance_release(first);

    dxl_run_ui ui = { .view = note_view };
    char *argv[] = { "deusex-launcher", "-NewWindow", NULL };
    static atomic_int stop;
    beside_views = 0;
    CHECK_INT(dxl_run_main(2, argv, &ui, &stop), 0);
    CHECK_INT(beside_views, 1);
    CHECK_INT(beside_view, DXL_VIEW_HOME);
    CHECK_INT(dxl_path_exists(sentinel), 1);

    dxl_instance_release(beside);
    beside_views = 0;
    CHECK_INT(dxl_run_main(2, argv, &ui, &stop), 0);
    CHECK_INT(beside_views, 1);
    CHECK_INT(beside_view, DXL_VIEW_RECOVERY);
    CHECK_INT(dxl_path_exists(sentinel), 0);

    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(id);
    dxl_session_close(&ss);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* With the first gone, a start hands its line to a launcher started beside
 * it, still running, as the original's to that one's log window: no screen
 * shown, no second game (bugs.md LAPP.12). The two launchers are this
 * process's instances. */
static void test_beside_takes_the_line(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-line"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_session ss;
    CHECK_INT(dxl_session_open(&ss, NULL, NULL), 0);
    char *id = dxl_session_instance_id(&ss);

    /* The first at Home, one beside it; the first quits. */
    dxl_instance *first = dxl_instance_acquire(id);
    dxl_instance *beside = dxl_instance_acquire(id);
    CHECK(first != NULL && beside != NULL);
    dxl_instance_release(first);

    dxl_run_ui ui = { .view = note_view };
    char *argv[] = { "deusex-launcher", "-nosound", NULL };
    static atomic_int stop;
    beside_views = 0;
    CHECK_INT(dxl_run_main(2, argv, &ui, &stop), 0);
    CHECK_INT(beside_views, 0);
    char line[DXL_HANDOFF_MAX + 1];
    CHECK_INT(dxl_instance_poll(beside, line, sizeof line), 1);
    CHECK_STR(line, "-nosound");

    dxl_instance_release(beside);
    free(id);
    dxl_session_close(&ss);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A window closed at Recovery: its Quit. */
static int recovery_views;
static int quit_at_recovery(void *ctx, dxl_run *r, dxl_view which) {
    (void)ctx;
    (void)r;
    if (which == DXL_VIEW_RECOVERY) recovery_views++;
    return 0;
}

/* After a crash, an end with no game takes Running.ini away, as the
 * original's every end but a crash and its CD prompt: a quit on Recovery,
 * -consolecommand=, -testrendev=. */
static void test_ends_without_a_game(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-ends"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *sentinel = dxl_path_join(p.state_dir, "Running.ini");
    static const char *const words[] = { NULL, "-consolecommand=Quit", "-testrendev=D3DDrv.D3DRenderDevice" };
    for (int i = 0; i < 3; i++) {
        dxl_env_set("DXL_STUB", "crash");
        CHECK_INT(run_launcher(root, NULL), END_CRASH_STATUS);
        CHECK_INT(dxl_path_exists(sentinel), 1);
        dxl_env_set("DXL_STUB", "ready");
        if (!words[i]) {
            dxl_run_ui ui = { .view = quit_at_recovery };
            char *argv[] = { "deusex-launcher", NULL };
            static atomic_int stop;
            recovery_views = 0;
            CHECK_INT(dxl_run_main(1, argv, &ui, &stop), 0);
            CHECK_INT(recovery_views, 1);
        } else {
            CHECK_INT(run_launcher(root, words[i]), 0);
        }
        if (dxl_path_exists(sentinel)) fprintf(stderr, "  Running.ini kept after %s\n", words[i] ? words[i] : "Quit");
        CHECK_INT(dxl_path_exists(sentinel), 0);
    }
    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A first run with no window whose engine is refused: the profile is made,
 * launcher.ini holding the install it names, so the next start, with no
 * --game, plays it. */
static void test_first_run_refused(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-refused"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    dxl_env_set("DXL_STUB_FEATURES", "loadgame-url");
    CHECK_INT(run_launcher(root, NULL), DXL_RUN_EXIT_NOGAME);
    dxl_env_set("DXL_STUB_FEATURES", NULL);
    char *argv[] = { "deusex-launcher", NULL };
    static atomic_int stop;
    CHECK_INT(dxl_run_main(1, argv, NULL, &stop), 0);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A window that plays from Home and quits at Graphics, noting the ini its
 * run's config is then. */
static char graphics_ini[1024];
static int play_then_graphics(void *ctx, dxl_run *r, dxl_view which) {
    (void)ctx;
    if (which == DXL_VIEW_HOME) return 1;
    if (which == DXL_VIEW_GRAPHICS && r->config)
        snprintf(graphics_ini, sizeof graphics_ini, "%s", dxl_config_path(r->config));
    return 0;
}

/* A game played on a line's INI= relaunched to -changevideo: the relaunch's
 * words name no ini, so its Graphics edits the profile's DeusEx.ini, the
 * one the relaunched engine reads, as the original's relaunched DeusEx.exe
 * takes the rest of the line alone. */
static void test_relaunch_drops_ini(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-relaunch"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *mod_ini = scratch_path(home, "Mod.ini");
    size_t len = 0;
    char *text = slurp(p.system_ini, &len);
    scratch_write(home, "Mod.ini", text ? text : "");
    free(text);
    char word[1100];
    snprintf(word, sizeof word, "INI=%s", mod_ini);
    dxl_env_set("DXL_STUB", "relaunch");
    dxl_env_set("DXL_STUB_LINE", "-changevideo");
    dxl_run_ui ui = { .view = play_then_graphics };
    char *argv[] = { "deusex-launcher", word, NULL };
    static atomic_int stop;
    graphics_ini[0] = '\0';
    CHECK_INT(dxl_run_main(2, argv, &ui, &stop), 0);
    CHECK_STR(graphics_ini, p.system_ini);
    dxl_env_set("DXL_STUB_LINE", NULL);
    free(mod_ini);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    scratch_remove(home);
    free(home);
}

/* A window that answers nothing, as one that does not open or that a
 * signal closes, but for Play at Recovery while play is set. */
static int unanswered_views, play_at_recovery;
static int not_answered(void *ctx, dxl_run *r, dxl_view which) {
    (void)ctx;
    (void)r;
    unanswered_views++;
    return which == DXL_VIEW_RECOVERY && play_at_recovery ? 1 : DXL_VIEW_UNANSWERED;
}

/* The run's stop, set as the game is about to start, as the window goes
 * for it: after the start's own checks. */
static atomic_int unanswered_stop;
static void stop_as_it_starts(void *ctx, dxl_run *r) {
    (void)ctx;
    (void)r;
    unanswered_stop = 1;
}

/* A screen no one answered is no quit: after a crash, Recovery with no
 * window, or stopped while it is up, leaves Running.ini, as does a game the
 * launcher stopped, after which no Recovery opens; a stop before the game
 * starts none, an end with no game. */
static void test_unanswered(void) {
    char *home = dxl_xstrdup(scratch_dir("gamefiles-unanswered"));
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
    CHECK_INT(run_launcher(root, NULL), 0);
    dxl_xdg x;
    dxl_xdg_open(&x, NULL);
    dxl_profile p;
    dxl_profile_open(&p, &x, "default", NULL);
    char *sentinel = dxl_path_join(p.state_dir, "Running.ini");
    dxl_env_set("DXL_STUB", "crash");
    CHECK_INT(run_launcher(root, NULL), END_CRASH_STATUS);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    dxl_env_set("DXL_STUB", "ready");

    dxl_run_ui ui = { .view = not_answered };
    char *argv[] = { "deusex-launcher", NULL };
    atomic_int *stop = &unanswered_stop;
    *stop = 0;
    unanswered_views = play_at_recovery = 0;
    CHECK_INT(dxl_run_main(1, argv, &ui, stop), DXL_RUN_EXIT_SCREEN);
    CHECK_INT(unanswered_views, 1);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    *stop = 1;
    CHECK_INT(dxl_run_main(1, argv, &ui, stop), 1);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    /* Play at Recovery, the launcher stopped as the game starts: the game,
     * which does not end cleanly, stopped, and no Recovery after it. The
     * launcher's SIGTERM, sent at its first wake, may come before the stub
     * has its handler, which would end it there: SIGTERM blocked across the
     * run, the stub, which inherits the mask, takes the stop once its
     * handler is in, however soon it came. */
    *stop = 0;
    ui.before_play = stop_as_it_starts;
    unanswered_views = 0;
    play_at_recovery = 1;
    dxl_env_set("DXL_STUB", "crash");
    sigset_t term, was;
    sigemptyset(&term);
    sigaddset(&term, SIGTERM);
    sigprocmask(SIG_BLOCK, &term, &was);
    CHECK_INT(dxl_run_main(1, argv, &ui, stop), END_CRASH_STATUS);
    sigprocmask(SIG_SETMASK, &was, NULL);
    CHECK_INT(unanswered_views, 1);
    CHECK_INT(dxl_path_exists(sentinel), 1);
    dxl_lastrun last;
    CHECK_INT(dxl_lastrun_load(&last, p.state_dir), 0);
    CHECK_INT(last.stopped, 1);
    /* A stop before the game -- while the start asks the GPU about
     * anti-aliasing -- starts none: an end with no game, Running.ini taken
     * away and no LastRun, so no Recovery after it (bugs.md LAPP.11). */
    ui.before_play = NULL;
    unanswered_views = 0;
    dxl_env_set("DXL_STUB", "ready");
    CHECK_INT(dxl_run_main(1, argv, &ui, stop), 0);
    CHECK_INT(unanswered_views, 1);
    CHECK_INT(dxl_path_exists(sentinel), 0);
    CHECK_INT(dxl_lastrun_load(&last, p.state_dir), -1);
    *stop = 0;

    free(sentinel);
    dxl_profile_free(&p);
    dxl_xdg_free(&x);
    free(root);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
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
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_ENGINE", DXL_STUB_ENGINE);
    dxl_env_set("DXL_STUB", "ready");
    dxl_env_set("DXL_STUB_MS", "200");
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

    dxl_env_set("DXL_STUB", "rewrite");
    dxl_env_set("DXL_STUB_COUNT", count);
    dxl_run_ui ui = { .view = home_then_quit };
    char *argv[] = { "deusex-launcher", NULL };
    static atomic_int stop;
    views = saw_rewrite = 0;
    CHECK_INT(dxl_run_main(1, argv, &ui, &stop), 0);
    CHECK_INT(views, 2);
    CHECK_INT(saw_rewrite, 1);
    size_t len = 0;
    char *runs = slurp(count, &len);
    CHECK_STR(runs, "x");
    free(runs);

    dxl_env_set("DXL_NO_HOME", "1");
    views = 0;
    CHECK_INT(dxl_run_main(1, argv, &ui, &stop), 0);
    CHECK_INT(views, 0);
    runs = slurp(count, &len);
    CHECK_STR(runs, "xx");
    free(runs);
    dxl_env_set("DXL_NO_HOME", NULL);

    dxl_xdg_free(&x);
    free(launcher_ini);
    free(root);
    dxl_env_set("DXL_STUB_COUNT", NULL);
    dxl_env_set("DXL_STUB", NULL);
    dxl_env_set("DXL_STUB_MS", NULL);
    dxl_env_set("DXL_ENGINE", NULL);
    dxl_env_set("DXL_HOME", NULL);
    free(count);
    scratch_remove(home);
    free(home);
}

/* Every asset the launcher reads from an install: the 31 menu themes by
 * their names, the Default's colours as the kit has them; the main menu's
 * textures; the six menu sounds, each a WAV; the title music, an IT
 * module; the logo, a BMP; a save's info and its snapshot. */
static void test_assets(void) {
    dxl_err e;
    char *u = game_file("DeusEx.u");
    static dxl_menu_theme t[DXL_MENU_THEMES_MAX];
    int n = dxl_menu_themes_read(u, t, DXL_MENU_THEMES_MAX, &e);
    CHECK_INT(n, 31);
    const dxl_menu_theme *def = dxl_menu_theme_find(t, n, "Default");
    CHECK(def && !memcmp(def->colors[1], "\x64\x64\xff", 3));
    CHECK(dxl_menu_theme_find(t, n, "Blue & Gold") != NULL);
    free(u);

    dxl_upkg p;
    char *ui = game_file("DeusExUI.u");
    CHECK_INT(dxl_upkg_load(&p, ui, &e), 0);
    static const char *const art[] = { "MenuMainBackground_1", "MenuMainBackground_2", "MenuMainBackground_3",
                                       "MenuMainButtonNormal_Left", "MenuMainButtonNormal_Center",
                                       "MenuMainButtonNormal_Right", "MenuMainButtonPressed_Left",
                                       "MenuMainButtonPressed_Center", "MenuMainButtonPressed_Right" };
    for (size_t i = 0; i < sizeof art / sizeof *art; i++) {
        dxl_uimage im;
        int rc = dxl_upkg_texture(&p, dxl_upkg_find(&p, "Texture", art[i]), &im, &e);
        CHECK_INT(rc, 0);
        CHECK(im.w > 0 && im.h > 0 && im.rgba);
        dxl_uimage_free(&im);
    }
    dxl_upkg_free(&p);
    free(ui);

    char *snd = game_file("DeusExSounds.u");
    CHECK_INT(dxl_upkg_load(&p, snd, &e), 0);
    static const char *const sounds[] = { "Menu_Focus", "Menu_Press", "Menu_OK", "Menu_Cancel", "Menu_Slider",
                                          "Menu_Activate" };
    for (size_t i = 0; i < sizeof sounds / sizeof *sounds; i++) {
        size_t len;
        char type[16];
        uint8_t *f = dxl_upkg_file(&p, dxl_upkg_find(&p, "Sound", sounds[i]), &len, type, sizeof type, &e);
        CHECK(f && len > 44 && !memcmp(f, "RIFF", 4) && !memcmp(f + 8, "WAVE", 4));
        CHECK_STR(type, "wav");
        free(f);
    }
    dxl_upkg_free(&p);
    free(snd);

    char *root = dxl_path_dirname(DXL_GAMEFILES);
    char *music = dxl_path_join(root, "Music/Title_Music.umx");
    CHECK_INT(dxl_upkg_load(&p, music, &e), 0);
    size_t len;
    char type[16];
    uint8_t *it = dxl_upkg_file(&p, dxl_upkg_find(&p, "Music", "Title_Music"), &len, type, sizeof type, &e);
    CHECK(it && len > 1000 && !memcmp(it, "IMPM", 4));
    CHECK_STR(type, "it");
    free(it);
    dxl_upkg_free(&p);
    free(music);

    char *logo = dxl_path_join(root, "Help/Logo.bmp");
    size_t logo_len = 0;
    char *bmp = slurp(logo, &logo_len);
    CHECK(bmp && logo_len > 1078 && !memcmp(bmp, "BM", 2));
    free(bmp);
    free(logo);

    free(root);
}

/* The install's own saves (gamefiles/Save), as the window reads them: the
 * first there is, its info and snapshot; then the list, as Continue and the
 * save browser take it, the newest the latest. With none there, none of it
 * read: test_gamefiles_saves is skipped (bugs.md LAPP.16). */
static int saves_read;

static void test_install_saves(void) {
    dxl_err e;
    char *root = dxl_path_dirname(DXL_GAMEFILES);
    /* The first save there is, its info and snapshot. */
    char *saves = dxl_path_join(root, "Save");
    DIR *d = opendir(saves);
    struct dirent *de;
    int read = 0;
    while (d && (de = readdir(d)) && !read) {
        if (dxl_strnicmp(de->d_name, "Save", 4) || de->d_name[4] < '0' || de->d_name[4] > '9') continue;
        char rel[300];
        snprintf(rel, sizeof rel, "%s/SaveInfo.dxs", de->d_name);
        char *info = dxl_path_join(saves, rel);
        dxl_saveinfo si;
        if (dxl_saveinfo_read(info, 1, &si, &e) == 0) {
            CHECK(si.year >= 2000);
            CHECK(si.map[0] != '\0');
            CHECK(si.snapshot.rgba && si.snapshot.w == 256 && si.snapshot.h == 128);
            dxl_saveinfo_free(&si);
            read = 1;
        }
        free(info);
    }
    if (d) closedir(d);
    saves_read = read;
    /* Listed as Continue and the save browser list them: each that read
     * above among them, the newest the latest. */
    dxl_saves list;
    CHECK_INT(dxl_saves_list(saves, &list, &e), 0);
    if (read) {
        CHECK(list.count > 0);
        CHECK(dxl_saves_latest(&list) != NULL);
        for (int i = 1; i < list.count; i++)
            if (list.items[i].readable && list.items[i - 1].readable)
                CHECK(list.items[i - 1].info.year >= list.items[i].info.year);
    }
    dxl_saves_free(&list);
    free(saves);
    free(root);
}

TEST_MAIN_BEGIN
    if (access(DXL_GAMEFILES "/Default.ini", R_OK) != 0) {
        printf("skipped: no Deus Ex install in %s\n", DXL_GAMEFILES);
        return 77;
    }
    /* --saves: the install's saves alone, a test of their own. */
    if (argc > 1 && !strcmp(argv[1], "--saves")) {
        RUN(test_install_saves);
        if (!saves_read && !dxl_test_failures) {
            printf("skipped: no save in the install to read\n");
            return 77;
        }
        printf("ok\n");
        return dxl_test_failures ? 1 : 0;
    }
    RUN(test_roundtrip);
    RUN(test_shipped_bindings);
    RUN(test_default_ini_matches_the_spec);
    RUN(test_assets);
    RUN(test_install);
    RUN(test_import_plan);
    RUN(test_changed_seen);
    RUN(test_mark_before_the_runs);
    RUN(test_headless_run);
    RUN(test_killed_mid_game);
    RUN(test_server_takes_no_handover);
    RUN(test_ends_without_a_game);
    RUN(test_unanswered);
    RUN(test_beside_outlives_the_first);
    RUN(test_beside_takes_the_line);
    RUN(test_first_run_refused);
    RUN(test_relaunch_drops_ini);
    RUN(test_back_home);
    RUN(test_game_folder_untouched);
TEST_MAIN_END
