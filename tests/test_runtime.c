#include "test.h"
#include "core/sentinel.h"
#include "platform/instance.h"
#include "core/paths.h"
#include "platform/os.h"
#include "platform/fs.h"

#include <sys/stat.h>
#include <unistd.h>

static char g_root[512];

static void mkdirs(const char *rel) {
    char p[768];
    snprintf(p, sizeof p, "%s/%s", g_root, rel);
    dxl_fs_mkdir(p);
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
    snprintf(g_root, sizeof g_root, "%s/dxl-rt-%ld", dxl_test_tmp(), dxl_os_pid());
    dxl_fs_mkdir(g_root);
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

/* A state folder gone (removed, or another machine's config): made with
 * the sentinel, which is there for the next start. */
static void test_sentinel_makes_its_folder(void) {
    char dir[768];
    snprintf(dir, sizeof dir, "%s/state/profiles/default", g_root);
    dxl_sentinel s;
    dxl_sentinel_init(&s, dir);
    CHECK_INT(dxl_sentinel_create(&s, NULL), 0);
    CHECK_INT(dxl_sentinel_exists(&s), 1);
    dxl_sentinel_remove(&s);
    dxl_sentinel_free(&s);
    rmdir(dir);
    snprintf(dir, sizeof dir, "%s/state/profiles", g_root);
    rmdir(dir);
    snprintf(dir, sizeof dir, "%s/state", g_root);
    rmdir(dir);
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

/* Every launcher marks itself running, the first and one started beside
 * it, as the original's every instance keeps its mutex: each finds the
 * other, neither itself, and with the first gone a later start finds the
 * one beside it, whose Running.ini is no crash (bugs.md LAPP.11). */
static void test_every_launcher_marked(void) {
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 0);

    dxl_instance *first = dxl_instance_acquire(g_root);
    CHECK(first != NULL);
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 1);
    CHECK_INT(dxl_instance_other_running(first, g_root), 0);

    dxl_instance *second = dxl_instance_acquire(g_root);
    CHECK(second != NULL);
    CHECK_INT(dxl_instance_locked(second), 1);
    CHECK_INT(dxl_instance_listens(second), 0);   /* the channel the first's */
    CHECK_INT(dxl_instance_other_running(first, g_root), 1);
    CHECK_INT(dxl_instance_other_running(second, g_root), 1);

    dxl_instance_release(first);
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 1);
    CHECK_INT(dxl_instance_other_running(second, g_root), 0);
    dxl_instance_release(second);
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 0);
}

/* Two installs on the same card must not lock each other out. */
static void test_lock_is_per_install(void) {
    char other[600];
    snprintf(other, sizeof other, "%s-other", g_root);

    dxl_instance *a = dxl_instance_acquire(g_root);
    dxl_instance *b = dxl_instance_acquire(other);
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK_INT(dxl_instance_listens(b), 1);
    CHECK_INT(dxl_instance_other_running(a, g_root), 0);
    dxl_instance_release(a);
    dxl_instance_release(b);
}

/* The handoff protocol is unchanged from WM_COPYDATA: one message carrying
 * the command line. Only the transport differs. */
static void test_handoff_delivers_the_command_line(void) {
    dxl_instance *primary = dxl_instance_acquire(g_root);
    CHECK(primary != NULL);

    char buf[DXL_HANDOFF_MAX + 1];
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);   /* nothing yet */

    dxl_err e;
    CHECK_INT(dxl_instance_forward(g_root, "deusex://host/Map?NewWindow", 1000, &e), 0);

    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 1);
    CHECK_STR(buf, "deusex://host/Map?NewWindow");

    /* Drained. */
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);
    dxl_instance_release(primary);
}

/* The mark alone: live for the Running.ini question, but nothing to forward
 * to -- a forward finds no one and fails, at once, whatever time it was
 * given (the hand-over's 30 s). */
static void test_lock_without_channel(void) {
    dxl_instance *server = dxl_instance_acquire_lock(g_root);
    CHECK(server != NULL);
    CHECK_INT(dxl_instance_listens(server), 0);
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 1);
    dxl_err e;
    long long t0 = dxl_os_ms();
    CHECK(dxl_instance_forward(g_root, "map", 30000, &e) != 0);
    CHECK(dxl_os_ms() - t0 < 2000);
    dxl_instance_release(server);
    dxl_instance *client = dxl_instance_acquire(g_root);
    CHECK(dxl_instance_listens(client));
    dxl_instance_release(client);
}

/* A client beside a server, which holds the mark alone, takes the channel,
 * as the original's every client's log window is IsBrowser: a forward
 * reaches it, and the channel is its own while it listens. */
static void test_listen_beside_the_lock(void) {
    dxl_instance *server = dxl_instance_acquire_lock(g_root);
    CHECK(server != NULL);
    dxl_instance *client = dxl_instance_acquire(g_root);
    CHECK(client != NULL);
    CHECK_INT(dxl_instance_listens(client), 1);
    CHECK_INT(dxl_instance_locked(client), 1);
    CHECK_INT(dxl_instance_locked(server), 1);
    dxl_instance *third = dxl_instance_acquire(g_root);
    CHECK(third != NULL);
    CHECK_INT(dxl_instance_listens(third), 0);
    dxl_instance_release(third);
    dxl_err e;
    CHECK_INT(dxl_instance_forward(g_root, "map", 1000, &e), 0);
    char buf[DXL_HANDOFF_MAX + 1];
    CHECK_INT(dxl_instance_poll(client, buf, sizeof buf), 1);
    CHECK_STR(buf, "map");
    dxl_instance_release(client);
    CHECK_INT(dxl_instance_other_running(NULL, g_root), 1);   /* the server's mark */
    dxl_instance_release(server);
}

TEST_MAIN_BEGIN
    build_install();
    RUN(test_sentinel_lifecycle);
    RUN(test_sentinel_survives_to_next_launch);
    RUN(test_sentinel_makes_its_folder);
    RUN(test_every_launcher_marked);
    RUN(test_lock_is_per_install);
    RUN(test_handoff_delivers_the_command_line);
    RUN(test_lock_without_channel);
    RUN(test_listen_beside_the_lock);
    teardown();
TEST_MAIN_END
