/* The Smart Pro's CPU modes (core/cpumode.h) and its game hook,
 * ports/trimui-smartpro/packaging/port-hooks.sh, run as the launcher runs
 * it: against stand-ins for spruceOS's helpers, which say what they were
 * asked, and a sysfs cpu folder of the test's own. */
#include "test.h"
#include "scratch.h"
#include "core/cpumode.h"
#include "platform/process.h"

#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef DXL_PORT_HOOKS
#define DXL_PORT_HOOKS "port-hooks.sh"
#endif

static char *dir, *calls, *cpu, *state, *helpers;

/* The helpers as spruceOS names them: each says it was called; a mode's
 * changes the CPU as the real one does. */
static const char *stub_helpers =
    "said() { echo \"$*\" >> \"$DXL_TEST_CALLS\"; }\n"
    "gov() { echo \"$1\" > $CPU/cpu0/cpufreq/scaling_governor; }\n"
    "set_smart() { said set_smart; gov conservative; }\n"
    "set_performance() { said set_performance; echo 0-3 > $CPU/online; gov performance;"
    " echo 1800000 > $CPU/cpu0/cpufreq/scaling_max_freq; }\n"
    "set_overclock() { said set_overclock; echo 0-3 > $CPU/online; gov performance;"
    " echo 2000000 > $CPU/cpu0/cpufreq/scaling_max_freq; }\n"
    "cores_online() { said cores_online \"$1\"; echo \"$1\" | sed 's/./&,/g; s/,$//' > $CPU/online; }\n"
    "unlock_governor() { :; }\n"
    "lock_governor() { :; }\n";

static void put_cpu(const char *online, const char *gov, const char *min, const char *max) {
    scratch_write(cpu, "online", online);
    scratch_write(cpu, "cpu0/cpufreq/scaling_governor", gov);
    scratch_write(cpu, "cpu0/cpufreq/scaling_min_freq", min);
    scratch_write(cpu, "cpu0/cpufreq/scaling_max_freq", max);
}

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return dxl_xstrdup("");
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    return dxl_xstrdup(buf);
}

static char *cpu_file(const char *rel) {
    char *p = scratch_path(cpu, rel);
    char *t = read_all(p);
    free(p);
    t[strcspn(t, "\n")] = '\0';
    return t;
}

/* The hook run: its exit status, its output into out. */
static int hook(const char *when, const char *mode, char *out, size_t size) {
    char *argv[] = { "/bin/sh", DXL_PORT_HOOKS, (char *)when, (char *)mode, NULL };
    int status = -1;
    dxl_err e;
    if (dxl_run_capture(argv, 10000, out, size, NULL, 0, &status, &e) != 0) return -1;
    return status;
}

static int called(const char *what) {
    char *text = read_all(calls);
    int n = 0;
    for (char *p = text; (p = strstr(p, what)); p += strlen(what)) n++;
    free(text);
    return n;
}

static void setup(int with_helpers) {
    dir = dxl_xstrdup(scratch_dir("cpumode"));
    calls = scratch_path(dir, "calls");
    cpu = scratch_path(dir, "cpu");
    state = scratch_path(dir, "state");
    helpers = scratch_path(dir, "helperFunctions.sh");
    remove(calls);
    remove(state);
    mkdir(cpu, 0755);
    put_cpu("0,3\n", "conservative\n", "600000\n", "1488000\n");
    if (with_helpers) scratch_write(dir, "helperFunctions.sh", stub_helpers);
    else remove(helpers);
    setenv("DXL_TEST_CALLS", calls, 1);
    setenv("DXL_HOOK_CPU", cpu, 1);
    setenv("DXL_HOOK_STATE", state, 1);
    setenv("DXL_HOOK_HELPERS", helpers, 1);
}

static void teardown(void) {
    scratch_remove(dir);
    free(dir); free(calls); free(cpu); free(state); free(helpers);
}

/* Each mode the launcher offers runs its own helper once; after puts the
 * CPU back as it was, spruce's cores_online given the cores, and forgets
 * the state. */
static void test_each_mode(void) {
    setup(1);
    char out[1024];
    for (int i = 0; dxl_cpu_modes[i].value; i++) {
        char helper[64];
        snprintf(helper, sizeof helper, "set_%s", dxl_cpu_modes[i].value);
        for (char *c = helper; *c; c++) *c = (char)tolower((unsigned char)*c);
        remove(calls);
        CHECK_INT(hook("before", dxl_cpu_modes[i].value, out, sizeof out), 0);
        CHECK_INT(called(helper), 1);
        CHECK(strstr(out, dxl_cpu_modes[i].value) != NULL);
        CHECK_INT(hook("after", NULL, out, sizeof out), 0);
        CHECK_INT(called("cores_online 03"), 1);
        char *gov = cpu_file("cpu0/cpufreq/scaling_governor"), *max = cpu_file("cpu0/cpufreq/scaling_max_freq");
        CHECK_STR(gov, "conservative");
        CHECK_STR(max, "1488000");
        free(gov);
        free(max);
        CHECK_INT(scratch_exists(dir, "state"), 0);
    }
    teardown();
}

/* A mode the hook does not know takes the default's helper: the launcher's
 * default and the hook's are one. */
static void test_unknown_mode(void) {
    setup(1);
    char out[1024];
    CHECK_INT(hook("before", "Turbo", out, sizeof out), 0);
    char helper[64];
    snprintf(helper, sizeof helper, "set_%s", DXL_CPU_MODE_DEFAULT);
    for (char *c = helper; *c; c++) *c = (char)tolower((unsigned char)*c);
    CHECK_INT(called(helper), 1);
    CHECK_INT(hook("after", NULL, out, sizeof out), 0);
    teardown();
}

/* A second before, as after a launcher killed mid-game, keeps the state
 * from before the first: the menu's, which after puts back. */
static void test_second_before(void) {
    setup(1);
    char out[1024];
    CHECK_INT(hook("before", "Overclock", out, sizeof out), 0);
    char *online = cpu_file("online");
    CHECK_STR(online, "0-3");
    free(online);
    CHECK_INT(hook("before", "Performance", out, sizeof out), 0);
    CHECK(strstr(out, "kept") != NULL);
    CHECK_INT(hook("after", NULL, out, sizeof out), 0);
    char *gov = cpu_file("cpu0/cpufreq/scaling_governor"), *max = cpu_file("cpu0/cpufreq/scaling_max_freq");
    CHECK_STR(gov, "conservative");
    CHECK_STR(max, "1488000");
    free(gov);
    free(max);
    CHECK_INT(called("cores_online 03"), 1);
    teardown();
}

/* No spruceOS helpers -- not a Smart Pro, or not spruceOS: nothing done,
 * and no failure. */
static void test_no_helpers(void) {
    setup(0);
    char out[1024];
    CHECK_INT(hook("before", "Overclock", out, sizeof out), 0);
    CHECK_INT(hook("after", NULL, out, sizeof out), 0);
    char *gov = cpu_file("cpu0/cpufreq/scaling_governor");
    CHECK_STR(gov, "conservative");
    free(gov);
    CHECK_INT(scratch_exists(dir, "state"), 0);
    teardown();
}

/* launcher.ini's mode, or the default for none or one not among them. */
static void test_mode_kept(void) {
    dxl_ini *ini = dxl_ini_new();
    CHECK_STR(dxl_cpu_mode(ini), DXL_CPU_MODE_DEFAULT);
    dxl_ini_set(ini, "Launcher", "CpuMode", "Smart");
    CHECK_STR(dxl_cpu_mode(ini), "Smart");
    dxl_ini_set(ini, "Launcher", "CpuMode", "Turbo");
    CHECK_STR(dxl_cpu_mode(ini), DXL_CPU_MODE_DEFAULT);
    dxl_ini_free(ini);
    CHECK_STR(dxl_cpu_mode(NULL), DXL_CPU_MODE_DEFAULT);
}

TEST_MAIN_BEGIN
    RUN(test_each_mode);
    RUN(test_unknown_mode);
    RUN(test_second_before);
    RUN(test_no_helpers);
    RUN(test_mode_kept);
TEST_MAIN_END
