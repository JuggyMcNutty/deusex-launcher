#include "test.h"
#include "platform/target.h"

/* The device profile (platform/target_default.c). */

static void test_generic_profile(void) {
    const dxl_target *t = dxl_target_get();
    CHECK_STR(t->id, "generic");
    CHECK(t->about && *t->about);
    CHECK(t->panel_w > 0 && t->panel_h > 0);
}

TEST_MAIN_BEGIN
    RUN(test_generic_profile);
TEST_MAIN_END
