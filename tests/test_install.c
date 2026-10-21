#include "test.h"
#include "scratch.h"
#include "core/install.h"

/* What the engine needs besides DeusEx.exe, in the odd cases a copy can
 * leave them in. */
static void write_files(const char *g) {
    scratch_write(g, "System/core.u", "x");
    scratch_write(g, "System/Engine.u", "x");
    scratch_write(g, "System/EXTENSION.U", "x");
    scratch_write(g, "System/DeusEx.u", "x");
    scratch_write(g, "System/DeusExUI.u", "x");
    scratch_write(g, "System/Default.ini", "x");
    scratch_write(g, "System/DefUser.ini", "x");
    scratch_write(g, "maps/DX.dx", "x");
}

/* No System/DeusEx.exe: not an install at all. */
static void test_not_an_install(void) {
    char *g = dxl_xstrdup(scratch_dir("install-none"));
    write_files(g);
    dxl_install in;
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK_INT(in.found, 0);
    CHECK(strstr(in.problem, "no System/DeusEx.exe") != NULL);
    dxl_install_free(&in);
    /* deusex.exe in the wrong case is not the engine's file either -- but on
     * Windows, whose names take any case, the engine opens it as its own. */
    scratch_write(g, "System/deusex.exe", "x");
    CHECK_INT(dxl_install_check(g, &in), -1);
#ifdef _WIN32
    CHECK_INT(in.found, 1);
#else
    CHECK_INT(in.found, 0);
#endif
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

/* A DeusEx.exe the engine does not know: found, never playable; the files
 * found in any case. */
static void test_unknown_build(void) {
    char *g = dxl_xstrdup(scratch_dir("install-unknown"));
    write_files(g);
    scratch_write(g, "System/DeusEx.exe", "abc");
    dxl_install in;
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK_INT(in.found, 1);
    CHECK_INT(in.ok, 0);
    CHECK(in.build == NULL);
    CHECK_STR(in.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK_STR(in.missing, "");
    CHECK(strstr(in.problem, "not a build the engine plays") != NULL);
    char id[64];
    dxl_install_id(&in, id, sizeof id);
    CHECK(strncmp(id, "unknown-", 8) == 0 && strlen(id) == 16);
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

/* What is missing, named. */
static void test_missing(void) {
    char *g = dxl_xstrdup(scratch_dir("install-missing"));
    scratch_write(g, "System/DeusEx.exe", "abc");
    scratch_write(g, "System/Core.u", "x");
    scratch_write(g, "Maps/DX.dx", "x");
    dxl_install in;
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK_STR(in.missing, "System/Engine.u, System/Extension.u, System/DeusEx.u, System/DeusExUI.u, "
                          "System/Default.ini, System/DefUser.ini");
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

TEST_MAIN_BEGIN
    RUN(test_not_an_install);
    RUN(test_unknown_build);
    RUN(test_missing);
TEST_MAIN_END
