/* The Smart Pro's app entry, ports/trimui-smartpro/packaging/launch.sh, run
 * as the menu runs it, against a card of the test's own: an app's folder
 * with the script and a stand-in for the launcher, which says the home it
 * was given, and the card's Saves. The home an earlier release kept in the
 * app's folder moves to Saves once (bugs.md D.91). */
#include "test.h"
#include "scratch.h"
#include "platform/process.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DXL_LAUNCH_SH
#define DXL_LAUNCH_SH "launch.sh"
#endif

static char *card, *app, *launched, *saves_home, *app_home;

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return dxl_xstrdup("");
    static char buf[65536];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    return dxl_xstrdup(buf);
}

static void setup(void) {
    card = dxl_xstrdup(scratch_dir("launch"));
    app = scratch_path(card, "App/DeusEx");
    dxl_fs_mkdir(app);
    launched = scratch_path(card, "launched");
    saves_home = scratch_path(card, "Saves/DXLauncher");
    app_home = scratch_path(card, "App/DeusEx/home");
    rmdir(saves_home);
    char *script = read_all(DXL_LAUNCH_SH);
    scratch_write(app, "launch.sh", script);
    free(script);
    /* The launcher: the home it was given, and whether the save is there. */
    scratch_write(app, "deusex-launcher",
                  "#!/bin/sh\n"
                  "echo \"$DXL_HOME\" > \"$DXL_TEST_LAUNCHED\"\n"
                  "[ -f \"$DXL_HOME/data/dxlauncher/profiles/default/Save/Save0001/SaveInfo.dxs\" ]"
                  " && echo save >> \"$DXL_TEST_LAUNCHED\"\n"
                  "exit 0\n");
    char *stub = scratch_path(app, "deusex-launcher");
    chmod(stub, 0755);
    free(stub);
    setenv("DXL_LAUNCH_CARD", card, 1);
    setenv("DXL_TEST_LAUNCHED", launched, 1);
}

static void teardown(void) {
    scratch_remove(card);
    free(card); free(app); free(launched); free(saves_home); free(app_home);
}

/* An earlier release's home: a profile with a save and a shot. */
static void old_home(const char *root) {
    dxl_fs_mkdir(root);
    scratch_write(root, "config/dxlauncher/launcher.ini", "[Launcher]\nProfile=default\n");
    scratch_write(root, "config/dxlauncher/profiles/default/profile.ini", "[Profile]\nName=Deus Ex\n");
    scratch_write(root, "config/dxlauncher/profiles/default/engine/Screenshots/Shot0001.bmp", "BM");
    scratch_write(root, "data/dxlauncher/profiles/default/Save/Save0001/SaveInfo.dxs", "info");
}

static int launch(char *out, size_t size) {
    char *sh = scratch_path(app, "launch.sh");
    char *argv[] = { "/bin/sh", sh, NULL };
    int status = -1;
    dxl_err e;
    int rc = dxl_run_capture(argv, 10000, out, size, NULL, 0, &status, &e);
    free(sh);
    return rc != 0 ? -1 : status;
}

/* The launcher's line: the home, then "save" where the save is in it. */
static void check_launched(const char *home, int with_save) {
    char *got = read_all(launched);
    char want[1024];
    snprintf(want, sizeof want, "%s\n%s", home, with_save ? "save\n" : "");
    CHECK_STR(got, want);
    free(got);
}

static char *log_of(const char *home) {
    char *p = scratch_path(home, "launch.log");
    char *t = read_all(p);
    free(p);
    return t;
}

/* The app's home moved whole to Saves, the app's folder left without it;
 * the next start finds it there and says nothing. */
static void test_moved(void) {
    setup();
    old_home(app_home);
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 1);
    CHECK_INT(dxl_path_exists(app_home), 0);
    CHECK_INT(scratch_exists(saves_home, "config/dxlauncher/profiles/default/engine/Screenshots/Shot0001.bmp"), 1);
    CHECK_INT(scratch_exists(saves_home, "config/dxlauncher/launcher.ini"), 1);
    char *said = log_of(saves_home);
    CHECK(strstr(said, "moved to") != NULL);
    free(said);
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 1);
    said = log_of(saves_home);
    CHECK_STR(said, "");
    free(said);
    teardown();
}

/* Both hold a profile: Saves' are used, the app's left as they
 * are, a line said. */
static void test_both(void) {
    setup();
    old_home(app_home);
    dxl_fs_mkdir(saves_home);
    scratch_write(saves_home, "config/dxlauncher/launcher.ini", "[Launcher]\nProfile=other\n");
    scratch_write(saves_home, "config/dxlauncher/profiles/other/profile.ini", "[Profile]\nName=Other\n");
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 0);
    CHECK_INT(scratch_exists(app_home, "data/dxlauncher/profiles/default/Save/Save0001/SaveInfo.dxs"), 1);
    char *said = log_of(saves_home);
    CHECK(strstr(said, "left as it is") != NULL);
    free(said);
    teardown();
}

/* An empty Saves folder of the launcher's takes the move. */
static void test_empty_new(void) {
    setup();
    old_home(app_home);
    dxl_fs_mkdir(saves_home);
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 1);
    CHECK_INT(dxl_path_exists(app_home), 0);
    teardown();
}

/* Saves on another file system: no copy that could stop half-way; the
 * app's home used where it is, a line said. */
static void test_other_card(void) {
    setup();
    old_home(app_home);
    char *bin = scratch_path(card, "bin/df");
    scratch_write(card, "bin/df",
                  "#!/bin/sh\n"
                  "echo 'Filesystem 1024-blocks Used Available Capacity Mounted on'\n"
                  "case \"$2\" in */Saves*) echo 'b 1 1 1 1% /b' ;; *) echo 'a 1 1 1 1% /a' ;; esac\n");
    chmod(bin, 0755);
    char *dir = scratch_path(card, "bin");
    const char *was = getenv("PATH");
    char *path_was = dxl_xstrdup(was ? was : "/usr/bin:/bin");
    char path[4096];
    snprintf(path, sizeof path, "%s:%s", dir, path_was);
    setenv("PATH", path, 1);
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    setenv("PATH", path_was, 1);
    check_launched(app_home, 1);
    CHECK_INT(scratch_exists(saves_home, "config/dxlauncher/launcher.ini"), 0);
    char *said = log_of(app_home);
    CHECK(strstr(said, "not moved") != NULL);
    free(said);
    free(path_was);
    free(dir);
    free(bin);
    teardown();
}

/* A stand-in for one of the script's tools, first on PATH, for one start:
 * its output, and its exit status. */
static int launch_with(const char *tool, const char *body, char *out, size_t size) {
    char *rel = scratch_path("bin", tool);
    char *bin = scratch_path(card, rel);
    char script[1024];
    snprintf(script, sizeof script, "#!/bin/sh\n%s\n", body);
    scratch_write(card, rel, script);
    chmod(bin, 0755);
    char *dir = scratch_path(card, "bin");
    const char *was = getenv("PATH");
    char *path_was = dxl_xstrdup(was ? was : "/usr/bin:/bin");
    char path[4096];
    snprintf(path, sizeof path, "%s:%s", dir, path_was);
    setenv("PATH", path, 1);
    int rc = launch(out, size);
    setenv("PATH", path_was, 1);
    free(path_was);
    free(dir);
    free(bin);
    free(rel);
    return rc;
}

/* A start of this release before the old home was put back leaves Saves'
 * folder with what a start writes and no profile: the old home still
 * moves, the other put aside, nothing removed. */
static void test_new_without_profile(void) {
    setup();
    old_home(app_home);
    dxl_fs_mkdir(saves_home);
    scratch_write(saves_home, "launch.log", "");
    scratch_write(saves_home, "state/dxlauncher/launcher.log", "Init\n");
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 1);
    CHECK_INT(dxl_path_exists(app_home), 0);
    char *aside = scratch_path(card, "Saves/DXLauncher.unused");
    CHECK_INT(scratch_exists(aside, "state/dxlauncher/launcher.log"), 1);
    free(aside);
    char *said = log_of(saves_home);
    CHECK(strstr(said, "put aside") != NULL);
    CHECK(strstr(said, "moved to") != NULL);
    free(said);
    teardown();
}

/* A rename that fails: the old home used where it is, a line said, and the
 * next start tries again. */
static void test_move_fails(void) {
    setup();
    old_home(app_home);
    char out[4096];
    CHECK_INT(launch_with("mv", "exit 1", out, sizeof out), 0);
    check_launched(app_home, 1);
    CHECK_INT(dxl_path_exists(saves_home), 0);
    char *said = log_of(app_home);
    CHECK(strstr(said, "could not be moved") != NULL);
    free(said);
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 1);
    CHECK_INT(dxl_path_exists(app_home), 0);
    teardown();
}

/* A df that says nothing: no move, which might have had to copy. */
static void test_df_silent(void) {
    setup();
    old_home(app_home);
    char out[4096];
    CHECK_INT(launch_with("df", "exit 1", out, sizeof out), 0);
    check_launched(app_home, 1);
    CHECK_INT(dxl_path_exists(saves_home), 0);
    teardown();
}

/* A first install: the home in Saves, made. */
static void test_fresh(void) {
    setup();
    char out[4096];
    CHECK_INT(launch(out, sizeof out), 0);
    check_launched(saves_home, 0);
    CHECK_INT(dxl_path_is_dir(saves_home), 1);
    CHECK_INT(dxl_path_exists(app_home), 0);
    teardown();
}

TEST_MAIN_BEGIN
    RUN(test_moved);
    RUN(test_both);
    RUN(test_empty_new);
    RUN(test_other_card);
    RUN(test_fresh);
    RUN(test_new_without_profile);
    RUN(test_move_fails);
    RUN(test_df_silent);
TEST_MAIN_END
