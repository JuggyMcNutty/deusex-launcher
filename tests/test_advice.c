#include "test.h"
#include "ends.h"
#include "core/advice.h"

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
    dxl_lastrun l = run(1, 0, 0, "Failed to initialize OpenAL device");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NOSOUND);
    l = run(1, 0, 0, "Could not find package DeusExText");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_INSTALL);
    l = run(1, 0, 0, "Could not open DXEngine.pk3");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_APP);
    /* A native the engine lacks: a mod with native code of its own. */
    l = run(1, 0, 0, "Script execution error: Unknown native function SomeMod.DoIt");
    CHECK(strstr(dxl_advice_for(&l)->advice, "native code") != NULL);
    l = run(1, 0, 0, "Script execution error: Runaway loop detected (over 10000000 iterations)");
    CHECK(strstr(dxl_advice_for(&l)->advice, "native code") == NULL);
}

static void test_ends(void) {
    dxl_lastrun l = run(END_CRASH_STATUS, END_CRASH, 1, "");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(1, 0, 0, "No UE1 game found in the given folder");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_INSTALL);
    l = run(0, 0, 0, "");   /* ended before its first map, no error */
    CHECK(dxl_advice_for(&l) == NULL);
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
    l = run(1, 0, 0, "Failed to initialize OpenAL device");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NOSOUND);
    /* A crash, no game, or no error line: not the save's. */
    l = run(END_CRASH_STATUS, END_CRASH, 0, "");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(1, 0, 0, "No UE1 game found in the given folder");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_INSTALL);
    l = run(127, 0, 0, "");
    snprintf(l.start, sizeof l.start, "?loadgame=10");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NONE);
    l = run(0, 0, 1, "");   /* clean */
    CHECK(dxl_advice_for(&l) == NULL);
    l.known = 0;
    CHECK(dxl_advice_for(&l) == NULL);
}

/* The engine drew with its software renderer, the one chosen not having
 * started: a clean run, advised all the same; a run that went wrong after
 * it, by what went wrong. */
static void test_fallback(void) {
    dxl_lastrun l = run(0, 0, 1, "");
    snprintf(l.fallback, sizeof l.fallback,
             "The Vulkan device did not start (No Vulkan devices found): the software device draws in its place");
    CHECK(strstr(dxl_advice_for(&l)->advice, "software renderer") != NULL);
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l.fallback[0] = '\0';
    CHECK(dxl_advice_for(&l) == NULL);
    l = run(1, 0, 1, "Failed to initialize OpenAL device");
    snprintf(l.fallback, sizeof l.fallback, "The GL device did not start (x): the software device draws in its place");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NOSOUND);
}

/* The engine's audio device did not start and the game played silent, its
 * line in the log of a clean run: advised as the OpenAL error it names; a
 * run that went wrong after it, by what went wrong. Files and folders the
 * engine could not open or make: advice of their own. */
static void test_silent(void) {
    dxl_lastrun l = run(0, 0, 1, "");
    snprintf(l.audio, sizeof l.audio, "Failed to initialize OpenAL device");
    CHECK(dxl_advice_for(&l) != NULL);
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NOSOUND);
    l = run(END_CRASH_STATUS, END_CRASH, 1, "");
    snprintf(l.audio, sizeof l.audio, "Failed to initialize OpenAL device");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_RENDERER);
    l = run(1, 0, 0, "Could not open /home/p/.local/share/dxlauncher/x/Save0001/SaveInfo.dxs");
    CHECK(strstr(dxl_advice_for(&l)->advice, "could not open a file") != NULL);
    l = run(1, 0, 0, "Could not create directory /mnt/full/Save");
    CHECK(strstr(dxl_advice_for(&l)->advice, "could not write a file or make a folder") != NULL);
    l = run(1, 0, 0, "Could not open DXEngine.pk3");
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_APP);
    /* The GPU's and the sound's, which name no path: no file's advice. */
    l = run(1, 0, 0, "Could not create vulkan image: VK_ERROR_OUT_OF_DEVICE_MEMORY");
    CHECK(strstr(dxl_advice_for(&l)->advice, "stopped with an error") != NULL);
    l = run(1, 0, 0, "Could not open wav file");
    CHECK(strstr(dxl_advice_for(&l)->advice, "stopped with an error") != NULL);
    l = run(1, 0, 0, "Could not create C:\\Users\\p\\Saved Games\\Save0001\\SaveInfo.dxs");
    CHECK(strstr(dxl_advice_for(&l)->advice, "could not write a file") != NULL);
}

#ifndef _WIN32
/* A SIGKILL the launcher did not send: a kill from outside, no error of the
 * game's; one it sent, its own stop. */
static void test_killed_outside(void) {
    dxl_lastrun l = run(END_KILL_STATUS, END_KILL, 1, "");
    CHECK(strstr(dxl_advice_for(&l)->advice, "killed from outside") != NULL);
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NONE);
    l.killed = 1;
    CHECK(strstr(dxl_advice_for(&l)->advice, "The launcher stopped the game") != NULL);
}
#endif

/* A run the launcher stopped: nothing went wrong in the game. */
static void test_stopped_by_launcher(void) {
    dxl_lastrun l = run(1, 0, 1, "");
    l.stopped = 1;
    CHECK(strstr(dxl_advice_for(&l)->advice, "The launcher stopped the game") != NULL);
    CHECK_INT(dxl_advice_for(&l)->fix, DXL_FIX_NONE);
    l.killed = 1;
    CHECK(strstr(dxl_advice_for(&l)->advice, "The launcher stopped the game") != NULL);
    /* Stopped and ended cleanly: nothing to say. */
    dxl_lastrun c = run(0, 0, 1, "");
    c.stopped = 1;
    CHECK(dxl_advice_for(&c) == NULL);
}

TEST_MAIN_BEGIN
    RUN(test_known);
    RUN(test_ends);
    RUN(test_fallback);
    RUN(test_silent);
    RUN(test_stopped_by_launcher);
#ifndef _WIN32
    RUN(test_killed_outside);
#endif
TEST_MAIN_END
