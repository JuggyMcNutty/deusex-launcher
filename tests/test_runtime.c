#include "test.h"
#include "scratch.h"
#include "core/sentinel.h"
#include "core/install.h"
#include "core/instance.h"
#include "core/log.h"
#include "core/paths.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_root[512];

/* The install's System folder, as it is spelled. */
static char g_system[768];
static const char *sys_dir(void) { return g_system; }

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
    snprintf(g_system, sizeof g_system, "%s/System", g_root);
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

/* --- the CD check ------------------------------------------------------- */

/* GOG's shipped CdPath is "..\" -- relative to System/, so it resolves back
 * to the game directory and the check passes. That is why nobody ever saw
 * the insert-CD prompt on a GOG install. */
static void test_cd_check_with_shipped_path(void) {
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\"), 1);
    CHECK_INT(dxl_install_cd_ok(sys_dir(), ""), 1);      /* unset: nothing to check */
    CHECK_INT(dxl_install_cd_ok(sys_dir(), NULL), 1);
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "Z:\\nope\\"), 0);
    /* An empty palette file is no disc: the original wants more than 0 bytes. */
    touch("Textures/Palettes.utx", "");
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\"), 0);
    touch("Textures/Palettes.utx", "palette");
}

/* Steam's is "..\GOTY_1", with no backslash at its end: a folder of its own
 * beside System/ holding Textures\Palettes.utx, joined with the backslash
 * CdPath lacks (dx-reverse-info/launch-flow.md section 8). */
static void test_cd_check_with_steams_path(void) {
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\GOTY_1"), 0);   /* no such folder */
    mkdirs("GOTY_1");
    mkdirs("GOTY_1/Textures");
    touch("GOTY_1/Textures/Palettes.utx", "palette");
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\GOTY_1"), 1);
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\GOTY_1\\"), 1);
    unlink_rel("GOTY_1/Textures/Palettes.utx");
    char p[768];
    snprintf(p, sizeof p, "%s/GOTY_1/Textures", g_root); rmdir(p);
    snprintf(p, sizeof p, "%s/GOTY_1", g_root);          rmdir(p);
}

/* An install whose folders a copy recased, its System spelled system (an
 * innoextract --lowercase GOG install): the shipped CdPath passes, from the
 * launcher's own folder, and Steam's GOTY_1 in another case too, as
 * Windows takes its names. */
static void test_cd_check_recased(void) {
    char was[768];
    snprintf(was, sizeof was, "%s/System", g_root);
    snprintf(g_system, sizeof g_system, "%s/system", g_root);
    CHECK_INT(rename(was, g_system), 0);
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\"), 1);
    mkdirs("goty_1");
    mkdirs("goty_1/textures");
    touch("goty_1/textures/palettes.utx", "palette");
    CHECK_INT(dxl_install_cd_ok(sys_dir(), "..\\GOTY_1"), 1);
    unlink_rel("goty_1/textures/palettes.utx");
    char p[768];
    snprintf(p, sizeof p, "%s/goty_1/textures", g_root); rmdir(p);
    snprintf(p, sizeof p, "%s/goty_1", g_root);          rmdir(p);
    CHECK_INT(rename(g_system, was), 0);
    snprintf(g_system, sizeof g_system, "%s", was);
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
    CHECK_INT(dxl_instance_other_running(), 0);

    dxl_instance *first = dxl_instance_acquire();
    CHECK(first != NULL);
    CHECK_INT(dxl_instance_other_running(), 1);

    dxl_instance *second = dxl_instance_acquire();
    CHECK(second == NULL);

    dxl_instance_release(first);
    CHECK_INT(dxl_instance_other_running(), 0);
}

/* One instance whatever the install, as the original's mutex is: a second
 * install's launch finds the first's game live. */
static void test_lock_is_the_users(void) {
    dxl_instance *a = dxl_instance_acquire();
    CHECK(a != NULL);
    CHECK(dxl_instance_acquire() == NULL);
    dxl_instance_release(a);
}

/* The handoff protocol is unchanged from WM_COPYDATA: one message carrying
 * the command line. Only the transport differs. */
static void test_handoff_delivers_the_command_line(void) {
    dxl_instance *primary = dxl_instance_acquire();
    CHECK(primary != NULL);

    char buf[DXL_HANDOFF_MAX + 1];
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);   /* nothing yet */

    dxl_err e;
    CHECK_INT(dxl_instance_forward("deusex://host/Map?NewWindow", 1000, &e), 0);

    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 1);
    CHECK_STR(buf, "deusex://host/Map?NewWindow");

    /* The longest line whole, its last byte too. */
    char *line = dxl_xmalloc(DXL_HANDOFF_MAX + 1);
    memset(line, 'x', DXL_HANDOFF_MAX);
    line[DXL_HANDOFF_MAX] = '\0';
    CHECK_INT(dxl_instance_forward(line, 1000, &e), 0);
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 1);
    CHECK_INT((int)strlen(buf), DXL_HANDOFF_MAX);
    free(line);

    /* Drained. */
    CHECK_INT(dxl_instance_poll(primary, buf, sizeof buf), 0);
    dxl_instance_release(primary);
}

/* The lock alone: live for the Running.ini question, but nothing to forward
 * to -- a forward finds no one and fails. */
static void test_lock_without_channel(void) {
    dxl_instance *server = dxl_instance_acquire_lock();
    CHECK(server != NULL);
    CHECK_INT(dxl_instance_fd(server), -1);
    CHECK_INT(dxl_instance_other_running(), 1);
    dxl_err e;
    CHECK(dxl_instance_forward("map", 1000, &e) != 0);
    dxl_instance_release(server);
    dxl_instance *client = dxl_instance_acquire();
    CHECK(dxl_instance_fd(client) >= 0);
    dxl_instance_release(client);
}

/* A client beside another instance listens without the lock, as the
 * original's every client's log window is IsBrowser: beside a -server's
 * lock alone, a forward reaches it. */
static void test_listen_without_the_lock(void) {
    dxl_instance *server = dxl_instance_acquire_lock();
    CHECK(server != NULL);
    CHECK(dxl_instance_acquire() == NULL);
    dxl_instance *client = dxl_instance_listen();
    CHECK(client != NULL);
    CHECK(dxl_instance_fd(client) >= 0);
    dxl_err e;
    CHECK_INT(dxl_instance_forward("map", 1000, &e), 0);
    char buf[DXL_HANDOFF_MAX + 1];
    CHECK_INT(dxl_instance_poll(client, buf, sizeof buf), 1);
    CHECK_STR(buf, "map");
    dxl_instance_release(client);
    CHECK_INT(dxl_instance_other_running(), 1);   /* the lock still the server's */
    dxl_instance_release(server);
}

/* A Running.ini in another case, a copied install's, goes with the run's
 * own: left, it opened RecoveryMode at every launch. */
/* The log emptied by the launcher that holds it alone: one opened while
 * another launcher holds it, started at the same moment, has none, as the
 * original's second writer fails to open the first's log and runs without
 * one, the first's left as it was. */
static void test_log_held(void) {
    char path[768];
    snprintf(path, sizeof path, "%s/System/DeusEx.log", g_root);
    touch("System/DeusEx.log", "the first's\n");
    int first = open(path, O_RDWR);
    CHECK_INT(flock(first, LOCK_EX | LOCK_NB), 0);
    dxl_log_open_new(path);
    dxl_log("the second's");
    dxl_log_close();
    char *text = slurp(path, NULL);
    CHECK(text && strstr(text, "the first's\n") && !strstr(text, "the second's"));
    free(text);
    close(first);
    /* Held by none: afresh. */
    dxl_log_open_new(path);
    dxl_log("the third's");
    dxl_log_close();
    text = slurp(path, NULL);
    CHECK(text && !strstr(text, "the first's") && strstr(text, "the third's"));
    free(text);
    unlink_rel("System/DeusEx.log");
}

static void test_sentinel_removes_another_case(void) {
    char sysdir[768], other[900];
    snprintf(sysdir, sizeof sysdir, "%s/System", g_root);
    snprintf(other, sizeof other, "%s/RUNNING.INI", sysdir);
    FILE *f = fopen(other, "wb");
    CHECK(f != NULL);
    if (f) fclose(f);
    dxl_sentinel s;
    dxl_sentinel_init(&s, sysdir);
    CHECK_INT(dxl_sentinel_exists(&s), 1);
    CHECK_INT(dxl_sentinel_create(&s, NULL), 0);
    dxl_sentinel_remove(&s);
    CHECK_INT(dxl_sentinel_exists(&s), 0);
    CHECK(access(other, F_OK) != 0);
    dxl_sentinel_free(&s);
}

TEST_MAIN_BEGIN
    scratch_runtime_dir();
    build_install();
    RUN(test_cd_check_with_shipped_path);
    RUN(test_cd_check_with_steams_path);
    RUN(test_cd_check_recased);
    RUN(test_sentinel_lifecycle);
    RUN(test_sentinel_survives_to_next_launch);
    RUN(test_sentinel_removes_another_case);
    RUN(test_lock_is_exclusive);
    RUN(test_lock_is_the_users);
    RUN(test_handoff_delivers_the_command_line);
    RUN(test_lock_without_channel);
    RUN(test_listen_without_the_lock);
    RUN(test_log_held);
    teardown();
TEST_MAIN_END
