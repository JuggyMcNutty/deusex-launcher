/* A pick's answer back in its list (screens/shell.h): by its own place,
 * though two labels are alike, as two controllers of one model are; in a
 * list made again under it, the first of its label. */
#include "test.h"
#include "screens/shell.h"

static dxl_shell s;

static void test_picked_at(void) {
    const char *pads[] = { "Xbox Series X Controller", "Xbox Series X Controller", "DualSense Wireless Controller" };
    for (int a = 0; a < 3; a++) {
        dxl_shell_pick(&s, 7, "Controller", pads, 3, 0);
        CHECK_INT(dxl_shell_picked_at(&s, a, pads, 3), a);
    }
    /* The second pad gone while the list was up. */
    const char *now[] = { "Xbox Series X Controller", "DualSense Wireless Controller" };
    CHECK_INT(dxl_shell_picked_at(&s, 2, now, 2), 1);
    CHECK_INT(dxl_shell_picked_at(&s, 1, now, 2), 0);
    const char *other[] = { "Nintendo Switch Pro Controller" };
    CHECK_INT(dxl_shell_picked_at(&s, 0, other, 1), -1);
    CHECK_INT(dxl_shell_picked_at(&s, -1, pads, 3), -1);
}

TEST_MAIN_BEGIN
    RUN(test_picked_at);
TEST_MAIN_END
