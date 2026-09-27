#include "test.h"
#include "platform/target.h"

/* One port's device profile (ports/<id>/target.c), checked against the rest
 * of that port. Built once per port that has a target.c; DXL_PORT_ID and
 * DXL_PORT_DIR say which. */

static void test_profile_is_this_ports(void) {
    const dxl_target *t = dxl_target_get();
    CHECK_STR(t->id, DXL_PORT_ID);
    CHECK(t->about && *t->about);
    CHECK(t->panel_w > 0 && t->panel_h > 0);
    for (int i = 0; t->fonts && t->fonts[i]; i++)
        CHECK(t->fonts[i][0] == '/');
}

/* Every CPU mode the Video tab offers must be one the port's run-game hooks
 * know, or choosing it would silently do something else. */
static void test_cpu_modes_are_handled_by_the_hooks(void) {
    const dxl_target *t = dxl_target_get();
    int n = dxl_target_cpu_mode_count(t);
    if (n == 0) return;
    CHECK(t->cpu_mode_default != NULL);
    CHECK_STR(dxl_target_cpu_mode(t, t->cpu_mode_default)->name, t->cpu_mode_default);

    size_t len;
    char *hooks = slurp(DXL_PORT_DIR "/packaging/port-hooks.sh", &len);
    CHECK(hooks != NULL);
    if (!hooks) return;
    for (int i = 0; i < n; i++) {
        CHECK(t->cpu_modes[i].help && *t->cpu_modes[i].help);
        if (!strstr(hooks, t->cpu_modes[i].name))
            fprintf(stderr, "  CPU mode %s is not in port-hooks.sh\n", t->cpu_modes[i].name);
        CHECK(strstr(hooks, t->cpu_modes[i].name) != NULL);
    }
    CHECK(strstr(hooks, "CpuMode") != NULL);

    /* With no CpuMode in launcher.ini the hooks fall back to a mode of their
     * own; it must be the one the Video tab shows as the default. */
    char want[128];
    snprintf(want, sizeof want, "CPU_MODE_DEFAULT=%s\n", t->cpu_mode_default);
    if (!strstr(hooks, want))
        fprintf(stderr, "  port-hooks.sh does not set %s", want);
    CHECK(strstr(hooks, want) != NULL);
    free(hooks);
}

TEST_MAIN_BEGIN
    RUN(test_profile_is_this_ports);
    RUN(test_cpu_modes_are_handled_by_the_hooks);
TEST_MAIN_END
