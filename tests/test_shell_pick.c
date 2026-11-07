/* A pick's answer back in its list (screens/shell.h): by its own place,
 * though two labels are alike, as two controllers of one model are; in a
 * list made again under it, the first of its label. An answer its control
 * does not read the frame after it is given, gone. */
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

/* An answer as a dialog gives it (dialogs.c), in the kit's frame now. */
static void answered(uint32_t id, int a) {
    const char *two[] = { "One", "Two" };
    dxl_shell_pick(&s, id, "Which", two, 2, 0);
    s.answer = a;
    s.answered = 1;
    s.answered_in = s.ui.frame;
}

/* Read the frame after, it is its control's; unread by that frame's end --
 * the control gone with its page or its list -- dropped, and the control
 * drawn again later reads none (bugs.md LUI.8). */
static void test_answer_expires(void) {
    answered(9, 1);
    dxl_shell_answer_expire(&s);   /* the frame it was given in ends */
    s.ui.frame++;
    CHECK_INT(dxl_shell_answer(&s, 9), 1);
    CHECK_INT(dxl_shell_answer(&s, 9), -1);

    answered(9, 1);
    dxl_shell_answer_expire(&s);
    s.ui.frame++;
    dxl_shell_answer_expire(&s);   /* the next ends, nothing read */
    CHECK_INT(s.answered, 0);
    s.ui.frame++;
    CHECK_INT(dxl_shell_answer(&s, 9), -1);
}

TEST_MAIN_BEGIN
    RUN(test_picked_at);
    RUN(test_answer_expires);
TEST_MAIN_END
