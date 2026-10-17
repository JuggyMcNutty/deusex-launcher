/* The engine's processing of the sticks and triggers (core/padcurve.h):
 * the values the Controller page draws are the ones the game gets. */
#include "test.h"
#include "core/padcurve.h"

#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 1e-5f; }

static void test_dead_zone(void) {
    float x, y;
    dxl_pad_stick(0.15f, 0.0f, 0.2f, 1.0f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
    /* The zone's edge is 0, full travel 1, rescaled between. */
    dxl_pad_stick(0.6f, 0.0f, 0.2f, 1.0f, &x, &y);
    CHECK(near(x, 0.5f) && near(y, 0.0f));
    dxl_pad_stick(1.0f, 0.0f, 0.2f, 1.0f, &x, &y);
    CHECK(near(x, 1.0f));
    /* Radial: a diagonal keeps its direction, past full travel held to 1. */
    dxl_pad_stick(0.8f, 0.8f, 0.2f, 1.0f, &x, &y);
    CHECK(near(x, y) && near(sqrtf(x * x + y * y), 1.0f));
    dxl_pad_stick(0.3f, 0.3f, 0.2f, 1.0f, &x, &y);
    CHECK(x > 0.0f && near(x, y));
    /* The zone held to 0.9, as the engine holds it. */
    dxl_pad_stick(0.95f, 0.0f, 2.0f, 1.0f, &x, &y);
    CHECK(near(x, 0.5f));
    dxl_pad_stick(0.0f, 0.0f, 0.0f, 1.0f, &x, &y);
    CHECK(x == 0.0f && y == 0.0f);
}

static void test_curves(void) {
    dxl_pad_tuning t = { 0.2f, 1.0f, 1.0f, 0 };
    float x, y;
    /* Moving is linear, up positive. */
    dxl_pad_move(0.0f, -0.6f, &t, &x, &y);
    CHECK(near(x, 0.0f) && near(y, 0.5f));
    /* Looking is curved: half a push is 0.5^1.6. */
    dxl_pad_look(0.6f, 0.0f, &t, &x, &y);
    CHECK(near(x, powf(0.5f, 1.6f)));
    t.look_x = 2.0f;
    dxl_pad_look(0.6f, 0.0f, &t, &x, &y);
    CHECK(near(x, 2.0f * powf(0.5f, 1.6f)));
    /* Down on the stick looks down; inverted, up. */
    dxl_pad_look(0.0f, 1.0f, &t, &x, &y);
    CHECK(near(y, -1.0f));
    t.invert_y = 1;
    dxl_pad_look(0.0f, 1.0f, &t, &x, &y);
    CHECK(near(y, 1.0f));
    float px, py;
    dxl_pad_stick(0.6f, 0.0f, 0.2f, DXL_PAD_CURVE_POINTER, &px, &py);
    CHECK(near(px, 0.25f));
    CHECK_INT(dxl_pad_sent(0.5f), 50);
    CHECK_INT(dxl_pad_sent(-1.0f), -100);
}

static void test_triggers(void) {
    CHECK_INT(dxl_pad_trigger_down(0.45f, 0), 0);
    CHECK_INT(dxl_pad_trigger_down(0.55f, 0), 1);
    /* Held down until under 0.35: no chatter at the press point. */
    CHECK_INT(dxl_pad_trigger_down(0.45f, 1), 1);
    CHECK_INT(dxl_pad_trigger_down(0.30f, 1), 0);
}

TEST_MAIN_BEGIN
    RUN(test_dead_zone);
    RUN(test_curves);
    RUN(test_triggers);
TEST_MAIN_END
