#include "test.h"
#include "ui/focus.h"

static dxl_box box(float x, float y, float w, float h) {
    dxl_box b = { x, y, w, h };
    return b;
}

/* A 3 x 3 grid of buttons, ids 1..9 row by row. */
static void grid(dxl_focus_list *l) {
    dxl_focus_clear(l);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) dxl_focus_add(l, (uint32_t)(r * 3 + c + 1), box(c * 120.0f, r * 60.0f, 100, 40), 0);
}

static void test_grid(void) {
    static dxl_focus_list l;
    grid(&l);
    CHECK_INT(dxl_focus_move(&l, 5, DXL_DIR_UP), 2);
    CHECK_INT(dxl_focus_move(&l, 5, DXL_DIR_DOWN), 8);
    CHECK_INT(dxl_focus_move(&l, 5, DXL_DIR_LEFT), 4);
    CHECK_INT(dxl_focus_move(&l, 5, DXL_DIR_RIGHT), 6);
    /* Nothing that way: it stays. */
    CHECK_INT(dxl_focus_move(&l, 1, DXL_DIR_UP), 1);
    CHECK_INT(dxl_focus_move(&l, 9, DXL_DIR_RIGHT), 9);
    /* One it does not know: the first. */
    CHECK_INT(dxl_focus_move(&l, 42, DXL_DIR_DOWN), 1);
    CHECK_INT(dxl_focus_first(&l), 1);
}

/* Down from a button goes to the one under it, not to a nearer one off to
 * the side; across the move counts twice. */
static void test_overlap_first(void) {
    static dxl_focus_list l;
    dxl_focus_clear(&l);
    dxl_focus_add(&l, 1, box(0, 0, 100, 40), 0);
    dxl_focus_add(&l, 2, box(0, 200, 100, 40), 0);     /* under it, far */
    dxl_focus_add(&l, 3, box(130, 50, 100, 40), 0);    /* near, to the side */
    CHECK_INT(dxl_focus_move(&l, 1, DXL_DIR_DOWN), 2);
    /* With nothing under it, the nearest by distance + 2 x across. */
    static dxl_focus_list m;
    dxl_focus_clear(&m);
    dxl_focus_add(&m, 1, box(0, 0, 100, 40), 0);
    dxl_focus_add(&m, 2, box(400, 60, 100, 40), 0);    /* 20 down, 300 across */
    dxl_focus_add(&m, 3, box(150, 160, 100, 40), 0);   /* 120 down, 50 across */
    CHECK_INT(dxl_focus_move(&m, 1, DXL_DIR_DOWN), 3);
}

/* A dialog's scope takes the focus: moves stay in it, and what is under
 * it is not had. */
static void test_scopes(void) {
    static dxl_focus_list l;
    grid(&l);
    dxl_focus_add(&l, 100, box(100, 300, 100, 40), 1);
    dxl_focus_add(&l, 101, box(220, 300, 100, 40), 1);
    CHECK_INT(dxl_focus_top_scope(&l), 1);
    CHECK_INT(dxl_focus_has(&l, 5), 0);
    CHECK_INT(dxl_focus_has(&l, 101), 1);
    CHECK_INT(dxl_focus_first(&l), 100);
    CHECK_INT(dxl_focus_move(&l, 5, DXL_DIR_DOWN), 100);
    CHECK_INT(dxl_focus_move(&l, 100, DXL_DIR_RIGHT), 101);
    CHECK_INT(dxl_focus_move(&l, 100, DXL_DIR_UP), 100);
}

static void test_tab_order(void) {
    static dxl_focus_list l;
    grid(&l);
    CHECK_INT(dxl_focus_step(&l, 1, 1), 2);
    CHECK_INT(dxl_focus_step(&l, 9, 1), 1);
    CHECK_INT(dxl_focus_step(&l, 1, -1), 9);
    CHECK_INT(dxl_focus_step(&l, 42, 1), 1);
    CHECK_INT(dxl_focus_step(&l, 42, -1), 9);
}

/* The moves alone reach what a pad's d-pad would (bugs.md LUI.8): across
 * the grid, from where the focus is, or from the first where it is
 * nowhere; nothing under a dialog; not left or right out of one whose own
 * they are, a slider's; nor one that no move lands on, drawn where another
 * is. */
static void test_reaches(void) {
    static dxl_focus_list l;
    grid(&l);
    CHECK_INT(dxl_focus_reaches(&l, 1, 9), 1);
    CHECK_INT(dxl_focus_reaches(&l, 9, 1), 1);
    CHECK_INT(dxl_focus_reaches(&l, 5, 5), 1);
    CHECK_INT(dxl_focus_reaches(&l, 42, 9), 1);
    CHECK_INT(dxl_focus_reaches(&l, 1, 42), 0);
    dxl_focus_add(&l, 100, box(100, 300, 100, 40), 1);
    dxl_focus_add(&l, 101, box(220, 300, 100, 40), 1);
    CHECK_INT(dxl_focus_reaches(&l, 5, 101), 1);
    CHECK_INT(dxl_focus_reaches(&l, 100, 5), 0);

    /* Two sliders, one over the other, and a button beside the lower. */
    static dxl_focus_list s;
    dxl_focus_clear(&s);
    dxl_focus_add(&s, 1, box(0, 0, 300, 40), 0);
    dxl_focus_add(&s, 2, box(0, 60, 300, 40), 0);
    dxl_focus_add(&s, 3, box(400, 60, 100, 40), 0);
    CHECK_INT(dxl_focus_reaches(&s, 1, 3), 1);
    dxl_focus_own_lr(&s, 1);
    dxl_focus_own_lr(&s, 2);
    CHECK_INT(dxl_focus_reaches(&s, 1, 2), 1);
    CHECK_INT(dxl_focus_reaches(&s, 1, 3), 0);
    CHECK_INT(dxl_focus_reaches(&s, 3, 1), 1);
    /* A new frame's are moves again until they are taken. */
    dxl_focus_clear(&s);
    dxl_focus_add(&s, 2, box(0, 60, 300, 40), 0);
    dxl_focus_add(&s, 3, box(400, 60, 100, 40), 0);
    CHECK_INT(dxl_focus_reaches(&s, 2, 3), 1);

    /* One drawn where another is, after it: every move lands on the other. */
    static dxl_focus_list o;
    dxl_focus_clear(&o);
    dxl_focus_add(&o, 1, box(0, 0, 100, 40), 0);
    dxl_focus_add(&o, 2, box(0, 60, 100, 40), 0);
    dxl_focus_add(&o, 3, box(0, 60, 100, 40), 0);
    CHECK_INT(dxl_focus_reaches(&o, 1, 2), 1);
    CHECK_INT(dxl_focus_reaches(&o, 1, 3), 0);
}

static void test_overflow(void) {
    static dxl_focus_list l;
    dxl_focus_clear(&l);
    for (int i = 0; i < DXL_FOCUS_MAX + 3; i++) dxl_focus_add(&l, (uint32_t)i + 1, box((float)i, 0, 1, 1), 0);
    CHECK_INT(l.count, DXL_FOCUS_MAX);
    CHECK_INT(l.overflowed, 1);
    dxl_focus_clear(&l);
    CHECK_INT(l.count, 0);
    CHECK_INT(l.overflowed, 0);
}

TEST_MAIN_BEGIN
    RUN(test_grid);
    RUN(test_overlap_first);
    RUN(test_scopes);
    RUN(test_tab_order);
    RUN(test_reaches);
    RUN(test_overflow);
TEST_MAIN_END
