#include "test.h"
#include "core/advice.h"

#include <signal.h>

static dxl_lastrun run(int status, int sig, int ready, const char *error) {
    dxl_lastrun l;
    memset(&l, 0, sizeof l);
    l.known = 1;
    l.status = status;
    l.signal = sig;
    l.ready = ready;
    l.clean = status == 0 && ready;
    snprintf(l.error, sizeof l.error, "%s", error);
    return l;
}

static void test_known(void) {
    dxl_lastrun l = run(1, 0, 0, "Could not create vulkan renderer: vkCreateInstance failed");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(1, 0, 0, "Failed to initialize OpenAL device");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NOSOUND);
    l = run(1, 0, 0, "Could not find package DeusExText");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_INSTALL);
    l = run(1, 0, 0, "Could not open SurrealEngine.pk3");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_APP);
}

static void test_ends(void) {
    dxl_lastrun l = run(139, SIGSEGV, 1, "");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(0, 0, 0, "");   /* no game found */
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_INSTALL);
    l = run(1, 0, 1, "Something else");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NONE);
    /* Started on a save that did not load: its own advice; up first, or
     * an error the engine names, not. */
    l = run(1, 0, 0, "Expected an import table entry: 01_NYC_UNATCOIsland");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK(strstr(dxl_advice_for(&l)->advice, "The save it started on") != NULL);
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_SAVES);
    l.ready = 1;
    CHECK(strstr(dxl_advice_for(&l)->advice, "The save it started on") == NULL);
    l = run(1, 0, 0, "Could not create vulkan renderer");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(0, 0, 1, "");   /* clean */
    CHECK(dxl_advice_for(&l) == NULL);
    l.known = 0;
    CHECK(dxl_advice_for(&l) == NULL);
}

TEST_MAIN_BEGIN
    RUN(test_known);
    RUN(test_ends);
TEST_MAIN_END
