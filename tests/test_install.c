#include "test.h"
#include "scratch.h"
#include "core/finder_places.h"
#include "core/install.h"
#include "platform/fs.h"
#include "core/paths.h"

#include <stdint.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#endif

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

/* A package's header alone, of version 68: its tag, version, flags, the
 * tables' counts and offsets (none), its GUID, as four little-endian
 * words, and no generations. */
static void write_package(const char *g, const char *rel, const uint32_t guid[4]) {
    uint32_t words[15] = { 0x9E2A83C1u, 68, 1, 0, 0, 0, 0, 0, 0, guid[0], guid[1], guid[2], guid[3], 0, 0 };
    unsigned char b[sizeof words];
    for (size_t i = 0; i < sizeof words / sizeof *words; i++)
        for (int k = 0; k < 4; k++) b[i * 4 + k] = (unsigned char)(words[i] >> (8 * k));
    char *p = scratch_path(g, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fwrite(b, 1, sizeof b, f); fclose(f); }
    free(p);
}

/* 1112fm's Core.u, Engine.u and DeusEx.u (core/gamedb.c), in the case a
 * copy can leave them; other, one of them another build's. */
static void write_packages(const char *g, int other) {
    static const uint32_t core[4] = { 0xBFC9A4E4u, 0x44E3B749u, 0xB688A995u, 0x29BD75C1u };
    static const uint32_t engine[4] = { 0x51EA2E91u, 0x4B953003u, 0x5CB94784u, 0xFF411102u };
    static const uint32_t deusex[4] = { 0xCADECA6Bu, 0x4EB3B7FFu, 0x1F0CB592u, 0x21ECCC8Eu };
    static const uint32_t another[4] = { 1, 2, 3, 4 };
    write_package(g, "System/core.u", core);
    write_package(g, "System/Engine.u", engine);
    write_package(g, "System/DeusEx.u", other ? another : deusex);
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
    scratch_remove(g);
    free(g);
}

/* A copy recased on a case-sensitive file system (innoextract --lowercase,
 * a disc read without its long names): its exe found in any case, as the
 * engine finds it; the finder's test as the check's. */
static void test_recased(void) {
    const char *exes[] = { "system/deusex.exe", "SYSTEM/DEUSEX.EXE", "System/deusex.exe" };
    for (size_t i = 0; i < sizeof exes / sizeof *exes; i++) {
        char *g = dxl_xstrdup(scratch_dir("install-recased"));
        scratch_write(g, exes[i], "abc");
        dxl_install in;
        dxl_install_check(g, &in);
        CHECK_INT(in.found, 1);
        CHECK_STR(in.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
        CHECK(strstr(in.problem, "no System/DeusEx.exe") == NULL);
        dxl_install_free(&in);
        CHECK_INT(dxl_finder_has_game(g), 1);
        scratch_remove(g);
        free(g);
    }
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
    CHECK(strstr(in.problem, "is not a stock 1112fm one (SHA1 a9993e") != NULL);
    CHECK(strstr(in.problem, "System/Core.u is no package") != NULL);
    CHECK_INT(in.by_packages, 0);
    char id[64];
    dxl_install_id(&in, id, sizeof id);
    CHECK(strncmp(id, "unknown-", 8) == 0 && strlen(id) == 16);
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

/* An exe of no hash the engine knows beside 1112fm's packages (another
 * launcher's in the stock one's place, a patched or a localized one):
 * playable, 1112fm, with a warning that names the exe (bugs.md D.89). */
static void test_unknown_exe_1112fm_packages(void) {
    char *g = dxl_xstrdup(scratch_dir("install-bypackages"));
    write_files(g);
    write_packages(g, 0);
    scratch_write(g, "System/DeusEx.exe", "abc");
    dxl_install in;
    CHECK_INT(dxl_install_check(g, &in), 0);
    CHECK_INT(in.ok, 1);
    CHECK_INT(in.by_packages, 1);
    CHECK(in.build == dxl_gamedb_by_packages());
    if (in.build) { CHECK_STR(in.build->version, "1112fm"); CHECK_INT(in.build->current, 1); }
    CHECK_STR(in.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
    CHECK(strstr(in.problem, "System/DeusEx.exe is not a stock one (SHA1 a9993e") != NULL);
    CHECK(strstr(in.problem, "packages 1112fm's") != NULL);
    char id[64];
    dxl_install_id(&in, id, sizeof id);
    CHECK(strncmp(id, "1112fm-", 7) == 0);
    dxl_install_free(&in);

    /* One file missing beside them: named, as with a known exe. */
    char *ui = scratch_path(g, "System/DeusExUI.u");
    remove(ui);
    free(ui);
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK_INT(in.by_packages, 1);
    CHECK_STR(in.problem, "files missing: System/DeusExUI.u");
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

/* An exe of no hash the engine knows beside packages of another build:
 * refused, the message saying which package is not 1112fm's. */
static void test_unknown_exe_other_packages(void) {
    char *g = dxl_xstrdup(scratch_dir("install-otherpackages"));
    write_files(g);
    write_packages(g, 1);
    scratch_write(g, "System/DeusEx.exe", "abc");
    dxl_install in;
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK_INT(in.ok, 0);
    CHECK_INT(in.by_packages, 0);
    CHECK(in.build == NULL);
    CHECK(strstr(in.problem, "System/DeusEx.u is not 1112fm's either") != NULL);
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
    /* An exe of no hash the engine knows and no packages: refused, the
     * first package not there named. */
    char *core = scratch_path(g, "System/Core.u");
    remove(core);
    free(core);
    CHECK_INT(dxl_install_check(g, &in), -1);
    CHECK(in.build == NULL);
    CHECK(strstr(in.problem, "and System/Core.u is not there: the engine wants 1112fm") != NULL);
    dxl_install_free(&in);
    scratch_remove(g);
    free(g);
}

/* A System folder named for the game: the game its parent's, the install's
 * root the parent; one with no game above it as it is. */
static void test_system_folder(void) {
    char *g = dxl_xstrdup(scratch_dir("install-system"));
    write_files(g);
    scratch_write(g, "System/DeusEx.exe", "abc");
    char *sys = scratch_path(g, "System");
    dxl_install in;
    dxl_install_check(sys, &in);
    CHECK_INT(in.found, 1);
    CHECK_STR(in.sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
    char *real = dxl_fs_real(g);
    CHECK_STR(in.root, real);
    free(real);
    dxl_install_free(&in);
    free(sys);
    scratch_remove(g);
    free(g);
    g = dxl_xstrdup(scratch_dir("install-system-none"));
    write_files(g);
    sys = scratch_path(g, "System");
    CHECK_INT(dxl_install_check(sys, &in), -1);
    CHECK_INT(in.found, 0);
    CHECK(strstr(in.root, "/System") != NULL);
    dxl_install_free(&in);
    free(sys);
    scratch_remove(g);
    free(g);
}

/* A file of 2 GiB and one byte, as an install's or a save's can be: there,
 * a file, its size whole (bugs.md LPORT.19, mingw-w64's 32-bit stat). Made
 * sparse where the file system lets it; not tried where it will not. */
static void test_large_file(void) {
    char *g = dxl_xstrdup(scratch_dir("install-large"));
    scratch_write(g, "big", "");
    char *big = scratch_path(g, "big");
    long long want = 2147483648LL + 1;
#ifdef _WIN32
    int fd = _open(big, _O_WRONLY | _O_BINARY);
    int made = fd >= 0 && _chsize_s(fd, want) == 0;
    if (fd >= 0) _close(fd);
#else
    int made = truncate(big, (off_t)want) == 0;
#endif
    if (made) {
        CHECK(dxl_path_exists(big));
        CHECK(!dxl_path_is_dir(big));
        CHECK(dxl_path_size(big) == want);
        CHECK(dxl_path_mtime(big) > 0);
    } else {
        printf("  no file of 2 GiB here: not tried\n");
    }
    free(big);
    scratch_remove(g);
    free(g);
}

TEST_MAIN_BEGIN
    RUN(test_not_an_install);
    RUN(test_recased);
    RUN(test_unknown_build);
    RUN(test_unknown_exe_1112fm_packages);
    RUN(test_unknown_exe_other_packages);
    RUN(test_missing);
    RUN(test_system_folder);
    RUN(test_large_file);
TEST_MAIN_END
