#include "test.h"
#include "core/config.h"
#include "core/ini.h"

#include <unistd.h>
#include <sys/stat.h>

/* Copies the fixture DeusEx.ini into a scratch directory so the tests can
 * write to it. Returns the directory; caller removes it. */
static char *scratch_install(const char *tag) {
    static char dir[512];
    snprintf(dir, sizeof dir, "/tmp/dxl-test-%s-%d", tag, (int)getpid());
    mkdir(dir, 0755);

    char *src = fixture("DeusEx.ini");
    size_t len = 0;
    char *data = slurp(src, &len);
    free(src);

    char dst[640];
    snprintf(dst, sizeof dst, "%s/DeusEx.ini", dir);
    FILE *f = fopen(dst, "wb");
    if (f) { fwrite(data, 1, len, f); fclose(f); }
    free(data);
    return dir;
}

static void scrub(const char *dir) {
    char p[640];
    snprintf(p, sizeof p, "%s/DeusEx.ini", dir); remove(p);
    snprintf(p, sizeof p, "%s/Running.ini", dir); remove(p);
    rmdir(dir);
}

static void test_reads_the_gates(void) {
    char *dir = scratch_install("gates");
    dxl_config *c = dxl_config_open(dir, "DeusEx");

    CHECK_INT(dxl_config_first_run(c), 0);
    CHECK_STR(dxl_config_render_device(c), "GlideDrv.GlideRenderDevice");
    CHECK_STR(dxl_config_cd_path(c), "..\\");
    CHECK_STR(dxl_config_game_engine(c), "DeusEx.DeusExGameEngine");

    dxl_config_free(c);
    scrub(dir);
}

/* FirstRun only ever rises. A user who has an install newer than this build
 * must not be dragged backwards into the first-run wizard. */
static void test_first_run_clamps_up_only(void) {
    char *dir = scratch_install("clamp");
    dxl_config *c = dxl_config_open(dir, "DeusEx");

    dxl_config_clamp_first_run(c);
    CHECK_INT(dxl_config_first_run(c), DXL_FIRSTRUN_CURRENT);
    CHECK_INT(dxl_config_first_run(c), 1100);
    dxl_config_free(c);

    /* Already at or above the ceiling: untouched, and not even dirtied. */
    c = dxl_config_open(dir, "DeusEx");
    dxl_ini_set_int(dxl_config_ini(c), "FirstRun", "FirstRun", 2000);
    dxl_config_clamp_first_run(c);
    CHECK_INT(dxl_config_first_run(c), 2000);
    dxl_config_free(c);

    scrub(dir);
}

/* The clamp is a write like any other: it marks the config dirty and reaches
 * the file on save. */
static void test_first_run_clamp_persists(void) {
    char *dir = scratch_install("persist");

    dxl_config *c = dxl_config_open(dir, "DeusEx");
    dxl_config_clamp_first_run(c);
    CHECK_INT(dxl_config_dirty(c), 1);
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);

    c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_first_run(c), 1100);
    dxl_config_free(c);

    scrub(dir);
}

/* Saving a config nothing touched must not rewrite the file at all. */
static void test_untouched_config_is_byte_identical_after_save(void) {
    char *dir = scratch_install("notouch");
    char path[640];
    snprintf(path, sizeof path, "%s/DeusEx.ini", dir);

    size_t before_len = 0;
    char *before = slurp(path, &before_len);

    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_dirty(c), 0);
    dxl_config_save(c, NULL);
    dxl_config_free(c);

    size_t after_len = 0;
    char *after = slurp(path, &after_len);
    CHECK_INT(after_len, before_len);
    CHECK(before && after && after_len == before_len &&
          memcmp(before, after, before_len) == 0);

    free(before); free(after);
    scrub(dir);
}

/* A fresh install with no ini must still be usable: the wizard writes one. */
static void test_missing_ini_is_not_fatal(void) {
    char dir[512];
    snprintf(dir, sizeof dir, "/tmp/dxl-test-noini-%d", (int)getpid());
    mkdir(dir, 0755);

    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK(c != NULL);
    CHECK_INT(dxl_config_first_run(c), 0);         /* absent reads as 0 */
    dxl_config_clamp_first_run(c);
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);

    c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_first_run(c), DXL_FIRSTRUN_CURRENT);
    dxl_config_free(c);

    scrub(dir);
}


/* Writes text to <dir>/<name>. */
static void put_file(const char *dir, const char *name, const char *text) {
    char p[640];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

static void copy_fixture(const char *dir, const char *fixture_name, const char *as) {
    char *src = fixture(fixture_name);
    size_t len = 0;
    char *data = slurp(src, &len);
    free(src);
    char dst[640];
    snprintf(dst, sizeof dst, "%s/%s", dir, as);
    FILE *f = fopen(dst, "wb");
    if (f && data) fwrite(data, 1, len, f);
    if (f) fclose(f);
    free(data);
}

static char *empty_dir(const char *tag) {
    static char dir[512];
    snprintf(dir, sizeof dir, "/tmp/dxl-test-%s-%d", tag, (int)getpid());
    mkdir(dir, 0755);
    return dir;
}

static void scrub_all(const char *dir) {
    static const char *names[] = { "DeusEx.ini", "Default.ini", "User.ini", "DefUser.ini",
                                   "SE-DeusEx.ini", "SE-User.ini", "Running.ini" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char p[640];
        snprintf(p, sizeof p, "%s/%s", dir, names[i]);
        remove(p);
    }
    rmdir(dir);
}

/* UE1's Core creates <Game>.ini from Default.ini before the launcher runs.
 * A GOG install copied to the SD card has no DeusEx.ini, so we must too. */
static void test_missing_ini_is_created_from_default(void) {
    char *dir = empty_dir("seed");
    copy_fixture(dir, "Default.ini", "Default.ini");

    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_seeded(c), 1);
    CHECK(dxl_ini_get(dxl_config_ini(c), "Core.System", "Paths") != NULL);
    CHECK_INT(dxl_config_dirty(c), 1);
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);

    c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_seeded(c), 0);
    CHECK_STR(dxl_config_game_engine(c), "DeusEx.DeusExGameEngine");
    dxl_config_free(c);
    scrub_all(dir);
}

/* The state found on the device: an earlier launcher wrote a 212-byte
 * DeusEx.ini with only its own keys, and the engine died with "Could not
 * find package Core". Rebuild from Default.ini, keep the stub's values. */
static void test_stub_ini_is_rebuilt_keeping_its_values(void) {
    char *dir = empty_dir("stub");
    copy_fixture(dir, "Default.ini", "Default.ini");
    put_file(dir, "DeusEx.ini",
             "[Engine.Engine]\r\nGameRenderDevice=OpenGLDrv.OpenGLRenderDevice\r\n\r\n"
             "[FirstRun]\r\nFirstRun=1100\r\n");

    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_seeded(c), 2);
    CHECK_INT(dxl_config_first_run(c), 1100);
    CHECK_STR(dxl_config_render_device(c), "OpenGLDrv.OpenGLRenderDevice");
    CHECK(dxl_ini_get(dxl_config_ini(c), "Core.System", "Paths") != NULL);
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);

    c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_seeded(c), 0);
    CHECK_INT(dxl_config_first_run(c), 1100);
    dxl_config_free(c);
    scrub_all(dir);
}

/* Once the engine has written SE-DeusEx.ini it reads only that, with client
 * settings under its own section. Writing DeusEx.ini would change nothing. */
static void test_client_settings_follow_the_engine_file(void) {
    char *dir = scratch_install("client");
    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK_STR(dxl_config_client_section(c), "WinDrv.WindowsClient");
    dxl_config_set_brightness(c, 0.75);
    CHECK_STR(dxl_ini_get(dxl_config_ini(c), "WinDrv.WindowsClient", "Brightness"), "0.750000");
    dxl_config_free(c);

    put_file(dir, "SE-DeusEx.ini", "[Engine.SurrealClient]\nBrightness=0.500000\nDecals=True\n");
    c = dxl_config_open(dir, "DeusEx");
    CHECK_STR(dxl_config_client_section(c), "Engine.SurrealClient");
    CHECK(dxl_config_brightness(c) > 0.49 && dxl_config_brightness(c) < 0.51);
    dxl_config_set_brightness(c, 0.9);
    dxl_config_set_decals(c, 0);
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);

    c = dxl_config_open(dir, "DeusEx");
    CHECK(dxl_config_brightness(c) > 0.89 && dxl_config_brightness(c) < 0.91);
    CHECK_INT(dxl_config_decals(c), 0);
    /* The base ini's copy was not touched by the second session. */
    CHECK_STR(dxl_ini_get(dxl_config_ini(c), "WinDrv.WindowsClient", "Brightness"), "0.600000");
    dxl_config_free(c);
    scrub_all(dir);
}

static void test_user_ini_prefers_the_engine_copy(void) {
    char *dir = empty_dir("user");
    copy_fixture(dir, "Default.ini", "Default.ini");
    copy_fixture(dir, "DefUser.ini", "DefUser.ini");

    /* No User.ini: created from DefUser.ini. */
    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK(dxl_config_user_ini(c) != NULL);
    CHECK_STR(dxl_ini_get(dxl_config_user_ini(c), "Engine.Input", "Joy1"), "Fire");
    dxl_err e;
    CHECK_INT(dxl_config_save(c, &e), 0);
    dxl_config_free(c);
    char p[640];
    snprintf(p, sizeof p, "%s/User.ini", dir);
    CHECK(access(p, F_OK) == 0);

    /* SE-User.ini wins once it exists. */
    put_file(dir, "SE-User.ini", "[Engine.Input]\nJoy1=Jump\n");
    c = dxl_config_open(dir, "DeusEx");
    CHECK_STR(dxl_ini_get(dxl_config_user_ini(c), "Engine.Input", "Joy1"), "Jump");
    CHECK(strstr(dxl_config_user_path(c), "SE-User.ini") != NULL);
    dxl_config_free(c);
    scrub_all(dir);
}

static void test_reset_deletes_and_rebuilds(void) {
    char *dir = empty_dir("reset");
    copy_fixture(dir, "Default.ini", "Default.ini");
    copy_fixture(dir, "DefUser.ini", "DefUser.ini");
    put_file(dir, "SE-DeusEx.ini", "[Engine.SurrealClient]\nBrightness=0.1\n");
    put_file(dir, "SE-User.ini", "[Engine.Input]\nJoy1=Jump\n");

    dxl_err e;
    CHECK_INT(dxl_config_reset_files(dir, "DeusEx", &e), 0);
    dxl_config *c = dxl_config_open(dir, "DeusEx");
    CHECK_INT(dxl_config_seeded(c), 1);
    CHECK_STR(dxl_config_client_section(c), "WinDrv.WindowsClient");
    CHECK_STR(dxl_ini_get(dxl_config_user_ini(c), "Engine.Input", "Joy1"), "Fire");
    dxl_config_free(c);
    scrub_all(dir);
}

/* No Default.ini, nothing to rebuild from: refuse and delete nothing. */
static void test_reset_refuses_without_default(void) {
    char *dir = scratch_install("noreset");
    dxl_err e;
    CHECK_INT(dxl_config_reset_files(dir, "DeusEx", &e), -1);
    char p[640];
    snprintf(p, sizeof p, "%s/DeusEx.ini", dir);
    CHECK(access(p, F_OK) == 0);
    scrub_all(dir);
}

TEST_MAIN_BEGIN
    RUN(test_reads_the_gates);
    RUN(test_first_run_clamps_up_only);
    RUN(test_first_run_clamp_persists);
    RUN(test_untouched_config_is_byte_identical_after_save);
    RUN(test_missing_ini_is_not_fatal);
    RUN(test_missing_ini_is_created_from_default);
    RUN(test_stub_ini_is_rebuilt_keeping_its_values);
    RUN(test_client_settings_follow_the_engine_file);
    RUN(test_user_ini_prefers_the_engine_copy);
    RUN(test_reset_deletes_and_rebuilds);
    RUN(test_reset_refuses_without_default);
TEST_MAIN_END
