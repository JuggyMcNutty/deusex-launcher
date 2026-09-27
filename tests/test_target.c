#include "test.h"
#include "platform/target.h"

/* The generic profile (platform/target_default.c) and the lookup rules every
 * profile relies on. Each port's own profile has test_target_port.c. */

static void test_generic_profile(void) {
    const dxl_target *t = dxl_target_get();
    CHECK_STR(t->id, "generic");
    CHECK(t->about && *t->about);
    CHECK(t->panel_w > 0 && t->panel_h > 0);
    /* A desktop offers no CPU modes: no row, nothing written. */
    CHECK_INT(dxl_target_cpu_mode_count(t), 0);
    CHECK(dxl_target_cpu_mode(t, "Performance") == NULL);
    CHECK(dxl_target_cpu_mode(t, NULL) == NULL);
}

static const dxl_cpu_mode_info modes[] = {
    { "Slow", "s" }, { "Fast", "f" }, { "Faster", "ff" }, { NULL, NULL }
};

static void test_cpu_mode_lookup(void) {
    dxl_target t = { .id = "t", .cpu_modes = modes, .cpu_mode_default = "Fast" };
    CHECK_INT(dxl_target_cpu_mode_count(&t), 3);
    CHECK_STR(dxl_target_cpu_mode(&t, "faster")->name, "Faster");   /* case-insensitive */
    CHECK_STR(dxl_target_cpu_mode(&t, "Slow")->name, "Slow");
    /* Unknown or unset: the default, as launcher.ini may hold anything. */
    CHECK_STR(dxl_target_cpu_mode(&t, "Turbo")->name, "Fast");
    CHECK_STR(dxl_target_cpu_mode(&t, NULL)->name, "Fast");
}

static void test_cpu_mode_default_falls_back_to_first(void) {
    dxl_target t = { .id = "t", .cpu_modes = modes, .cpu_mode_default = "Missing" };
    CHECK_STR(dxl_target_cpu_mode(&t, NULL)->name, "Slow");
    t.cpu_mode_default = NULL;
    CHECK_STR(dxl_target_cpu_mode(&t, "nope")->name, "Slow");
}

TEST_MAIN_BEGIN
    RUN(test_generic_profile);
    RUN(test_cpu_mode_lookup);
    RUN(test_cpu_mode_default_falls_back_to_first);
TEST_MAIN_END
