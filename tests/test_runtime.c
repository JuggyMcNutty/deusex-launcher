#include "test.h"
#include "core/sentinel.h"
#include "core/install.h"
#include "core/instance.h"
#include "core/paths.h"

#include <sys/stat.h>
#include <unistd.h>

static char g_root[512];

static void mkdirs(const char *rel) {
    char p[768];
    snprintf(p, sizeof p, "%s/%s", g_root, rel);
    mkdir(p, 0755);
}

static void touch(const char *rel, const char *content) {
    char p[768];
    snprintf(p, sizeof p, "%s/%s", g_root, rel);
    FILE *f = fopen(p, "wb");
    if (f) { fputs(content, f); fclose(f); }
}

static void unlink_rel(const char *rel) {
    char p[768];
    snprintf(p, sizeof p, "%s/%s", g_root, rel);
    remove(p);
}

/* A minimal but complete-looking install, as a user would have copied it. */
static void build_install(void) {
    snprintf(g_root, sizeof g_root, "/tmp/dxl-rt-%d", (int)getpid());
    mkdir(g_root, 0755);
    mkdirs("System");
    mkdirs("Textures");
    mkdirs("Maps");
    touch("System/DeusEx.u", "package");
    touch("System/Engine.u", "package");
    touch("System/Core.u",   "package");
    touch("Textures/Palettes.utx", "palette");
    touch("Maps/01_NYC_UNATCOIsland.dx", "map");
}

static void teardown(void) {
    unlink_rel("System/DeusEx.u"); unlink_rel("System/Engine.u");
    unlink_rel("System/Core.u");   unlink_rel("System/Running.ini");
    unlink_rel("Textures/Palettes.utx");
    unlink_rel("Maps/01_NYC_UNATCOIsland.dx");
    char p[768];
    snprintf(p, sizeof p, "%s/System", g_root);   rmdir(p);
    snprintf(p, sizeof p, "%s/Textures", g_root); rmdir(p);
    snprintf(p, sizeof p, "%s/Maps", g_root);     rmdir(p);
    rmdir(g_root);
}

/* --- install validation ------------------------------------------------ */

static void test_complete_install_passes(void) {
    dxl_install in;
    dxl_install_probe(g_root, &in);
    CHECK_INT(in.ok, 1);
    CHECK_INT(in.missing_count, 0);
    CHECK(in.system_dir != NULL);
    dxl_install_free(&in);
}

/* The install screen has to say WHAT is missing, not just that something is. */
static void test_missing_item_is_named(void) {
    unlink_rel("Textures/Palettes.utx");

    dxl_install in;
    dxl_install_probe(g_root, &in);
    CHECK_INT(in.ok, 0);
    CHECK_INT(in.missing_count, 1);

    int found_the_right_one = 0;
    for (int i = 0; i < in.item_count; i++)
        if (!in.items[i].found &&
            strcmp(in.items[i].relative, "Textures/Palettes.utx") == 0)
            found_the_right_one = 1;
    CHECK_INT(found_the_right_one, 1);
    dxl_install_free(&in);

    touch("Textures/Palettes.utx", "palette");
}

static void test_empty_directory_reports_everything_missing(void) {
    char empty[512];
    snprintf(empty, sizeof empty, "/tmp/dxl-rt-empty-%d", (int)getpid());
    mkdir(empty, 0755);

    dxl_install in;
    dxl_install_probe(empty, &in);
    CHECK_INT(in.ok, 0);
    CHECK_INT(in.missing_count, in.item_count);
    dxl_install_free(&in);
    rmdir(empty);
}

/* A zero-byte package is not a package. Half-finished copies to an SD card
 * are common enough to be worth catching here rather than in the engine. */
static void test_zero_byte_package_is_missing(void) {
    touch("System/DeusEx.u", "");
    dxl_install in;
    dxl_install_probe(g_root, &in);
    CHECK_INT(in.ok, 0);
    dxl_install_free(&in);
    touch("System/DeusEx.u", "package");
}

/* The shipped CdPath is "..\" -- relative to System/, so it resolves back to
 * the game directory and the check passes. That is why nobody ever saw the
 * insert-CD prompt on a GOG install. */
static void test_cd_check_with_shipped_path(void) {
    CHECK_INT(dxl_install_cd_ok(g_root, "..\\"), 1);
    CHECK_INT(dxl_install_cd_ok(g_root, ""), 1);      /* unset: nothing to check */
    CHECK_INT(dxl_install_cd_ok(g_root, NULL), 1);
    CHECK_INT(dxl_install_cd_ok(g_root, "Z:\\nope\\"), 0);
}

/* --- the crash sentinel ------------------------------------------------ */

static void test_sentinel_lifecycle(void) {
    char sysdir[768];
    snprintf(sysdir, sizeof sysdir, "%s/System", g_root);

    dxl_sentinel s;
    dxl_sentinel_init(&s, sysdir);

    CHECK_INT(dxl_sentinel_exists(&s), 0);
    CHECK_INT(dxl_sentinel_create(&s, NULL), 0);
    CHECK_INT(dxl_sentinel_exists(&s), 1);
    CHECK_INT(s.created, 1);

    dxl_sentinel_remove(&s);
    CHECK_INT(dxl_sentinel_exists(&s), 0);

    /* Removing twice is fine: the exit path calls it unconditionally. */
    dxl_sentinel_remove(&s);
    dxl_sentinel_free(&s);
}

/* A sentinel left behind by a killed process is exactly what the next launch
 * must notice. That is the whole of crash detection. */
static void test_sentinel_survives_to_next_launch(void) {
    char sysdir[768];
    snprintf(sysdir, sizeof sysdir, "%s/System", g_root);

    dxl_sentinel a;
    dxl_sentinel_init(&a, sysdir);
    dxl_sentinel_create(&a, NULL);
    dxl_sentinel_free(&a);            /* process "dies" without removing it */

    dxl_sentinel b;
    dxl_sentinel_init(&b, sysdir);
    CHECK_INT(dxl_sentinel_exists(&b), 1);
    dxl_sentinel_remove(&b);
    dxl_sentinel_free(&b);
}

/* --- single instance and handoff --------------------------------------- */

static void test_lock_is_exclusive(void) {
    CHECK_INT(dxl_instance_other_running(g_root), 0);

    dxl_instance *first = dxl_instance_acquire(g_root);
    CHECK(first != NULL);
    CHECK_INT(dxl_instance_other_running(g_root), 1);

    dxl_instance *second = dxl_instance_acquire(g_root);
    CHECK(second == NULL);

    dxl_instance_release(first);
    CHECK_INT(dxl_instance_other_running(g_root), 0);
}

/* Two installs on the same card must not lock each other out. */
static void test_lock_is_per_install(void) {
    char other[600];
    snprintf(other, sizeof other, "%s-other", g_root);

    dxl_instance *a = dxl_instance_acquire(g_root);
    dxl_instance *b = dxl_instance_acquire(other);
    CHECK(a != NULL);
    CHECK(b != NULL);
    dxl_instance_release(a);
    dxl_instance_release(b);
}

/* The handoff protocol is unchanged from WM_COPYDATA: one message carrying
 * the command line. Only the transport differs. */
static void test_handoff_delivers_the_command_line(void) {
    dxl_instance *primary = dxl_instance_acquire(g_root);
    CHECK(primary != NULL);

    char buf[DXL_HANDOFF_MAX];
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);   /* nothing yet */

    dxl_err e;
    CHECK_INT(dxl_instance_forward(g_root, "deusex://host/Map?NewWindow", 1000, &e), 0);

    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 1);
    CHECK_STR(buf, "deusex://host/Map?NewWindow");

    /* Drained. */
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);
    dxl_instance_release(primary);
}

TEST_MAIN_BEGIN
    build_install();
    RUN(test_complete_install_passes);
    RUN(test_missing_item_is_named);
    RUN(test_empty_directory_reports_everything_missing);
    RUN(test_zero_byte_package_is_missing);
    RUN(test_cd_check_with_shipped_path);
    RUN(test_sentinel_lifecycle);
    RUN(test_sentinel_survives_to_next_launch);
    RUN(test_lock_is_exclusive);
    RUN(test_lock_is_per_install);
    RUN(test_handoff_delivers_the_command_line);
    teardown();
TEST_MAIN_END
