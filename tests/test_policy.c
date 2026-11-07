#include "test.h"
#include "core/policy.h"

/* The entry matrix from dx-reverse-info/wizard.md, exhaustively. The ordering
 * assertions are the point: "first match wins" is the whole specification,
 * and getting it wrong produces a launcher that looks right until someone
 * passes two flags at once. */

static dxl_decision decide(const char *cmdline, int first_run,
                           int other_instance, int running_ini) {
    dxl_policy_input in = {
        .cmdline = cmdline,
        .first_run = first_run,
        .other_instance = other_instance,
        .running_ini_exists = running_ini,
        .is_client = 1,
    };
    dxl_decision out;
    dxl_policy_decide(&in, &out);
    return out;
}

/* A settled install with nothing unusual: straight to the game, no screen. */
static void test_no_wizard_when_nothing_matches(void) {
    dxl_decision d = decide("", 1100, 0, 0);
    CHECK_INT(d.action, DXL_ACTION_LAUNCH);
    CHECK_INT(d.screen, DXL_SCREEN_NONE);
}

static void test_row1_safe(void) {
    dxl_decision d = decide("-safe", 1100, 0, 0);
    CHECK_INT(d.action, DXL_ACTION_SCREEN);
    CHECK_INT(d.screen, DXL_SCREEN_MAIN_SAFE);
}

/* "readini" is an appStrfind match: no dash needed, and it fires from inside
 * a map name. That is not a bug in this port; it is the original's behaviour,
 * and a port that "fixed" it would diverge silently. */
static void test_row1_readini_matches_anywhere(void) {
    CHECK_INT(decide("readini", 1100, 0, 0).screen, DXL_SCREEN_MAIN_SAFE);
    CHECK_INT(decide("-readini", 1100, 0, 0).screen, DXL_SCREEN_MAIN_SAFE);
    CHECK_INT(decide("MyMap_readini.dx", 1100, 0, 0).screen, DXL_SCREEN_MAIN_SAFE);
}

static void test_row2_first_run(void) {
    /* Shipped DeusEx.ini has FirstRun=0. */
    dxl_decision d = decide("", 0, 0, 0);
    CHECK_INT(d.action, DXL_ACTION_SCREEN);
    CHECK_INT(d.screen, DXL_SCREEN_RENDERER_FIRST);

    /* The gate is < 400, so 399 shows it and 400 does not. */
    CHECK_INT(decide("", 399, 0, 0).screen, DXL_SCREEN_RENDERER_FIRST);
    CHECK_INT(decide("", 400, 0, 0).screen, DXL_SCREEN_NONE);
}

static void test_firstrun_flag_forces_the_gate(void) {
    dxl_decision d = decide("-firstrun", 1100, 0, 0);
    CHECK_INT(d.effective_first_run, 0);
    CHECK_INT(d.screen, DXL_SCREEN_RENDERER_FIRST);
}

static void test_row3_changevideo(void) {
    dxl_decision d = decide("-changevideo", 1100, 0, 0);
    CHECK_INT(d.screen, DXL_SCREEN_RENDERER_VIDEO);
}

static void test_row4_recovery(void) {
    dxl_decision d = decide("", 1100, 0, 1);
    CHECK_INT(d.screen, DXL_SCREEN_MAIN_RECOVERY);
}

/* A surviving Running.ini means a crash only if nothing else is live. With a
 * concurrent instance it belongs to that instance. */
static void test_recovery_needs_no_other_instance(void) {
    dxl_decision d = decide("-NewWindow", 1100, 1, 1);   /* bypass the handoff */
    CHECK_INT(d.action, DXL_ACTION_LAUNCH);
    CHECK_INT(d.screen, DXL_SCREEN_NONE);
}

/* --- ordering: first match wins --- */

static void test_safe_beats_first_run(void) {
    CHECK_INT(decide("-safe", 0, 0, 0).screen, DXL_SCREEN_MAIN_SAFE);
}

static void test_safe_beats_recovery(void) {
    CHECK_INT(decide("-safe", 1100, 0, 1).screen, DXL_SCREEN_MAIN_SAFE);
}

/* -changevideo is row 3 and first-run is row 2, so a pristine install given
 * -changevideo still runs the full first-time flow. */
static void test_first_run_beats_changevideo(void) {
    dxl_decision d = decide("-changevideo", 0, 0, 0);
    CHECK_INT(d.screen, DXL_SCREEN_RENDERER_FIRST);
}

static void test_changevideo_beats_recovery(void) {
    CHECK_INT(decide("-changevideo", 1100, 0, 1).screen, DXL_SCREEN_RENDERER_VIDEO);
}

/* --- handoff --- */

/* A caller that forwarded first, the forwarding being its own (app/run.h),
 * and found no one to take it decides on -- but a surviving Running.ini
 * still belongs to the live instance, not to a crash. */
static void test_other_instance_decides_on(void) {
    dxl_decision d = decide("", 1100, 1, 1);
    CHECK_INT(d.action, DXL_ACTION_LAUNCH);
    CHECK_INT(d.screen, DXL_SCREEN_NONE);
    CHECK_INT(decide("-changevideo", 1100, 1, 1).screen, DXL_SCREEN_RENDERER_VIDEO);
}

/* The four bypass tokens must still skip forwarding, or -changevideo aimed at
 * a running game would be swallowed by it instead of opening the screen. */
static void test_bypass_tokens_skip_forwarding(void) {
    const char *tokens[] = { "-server", "-NewWindow", "-changevideo",
                             "-testrendev=D3DDrv.D3DRenderDevice" };
    for (size_t i = 0; i < sizeof tokens / sizeof *tokens; i++) {
        dxl_decision d = decide(tokens[i], 1100, 1, 0);
        CHECK_INT(d.skip_handoff, 1);
    }
    CHECK_INT(decide("deusex://host/Map", 1100, 1, 0).skip_handoff, 0);
    /* changevideo against a live instance opens the screen, not a handoff. */
    CHECK_INT(decide("-changevideo", 1100, 1, 0).screen, DXL_SCREEN_RENDERER_VIDEO);
}

/* --- exits that never launch --- */

static void test_consolecommand_exits(void) {
    dxl_decision d = decide("-consolecommand=ShowLog", 0, 0, 0);
    CHECK_INT(d.action, DXL_ACTION_CONSOLE_CMD);
    CHECK_STR(d.console_command, "ShowLog");
    /* Even with FirstRun=0, no wizard: this path returns before the tree. */
    CHECK_INT(d.screen, DXL_SCREEN_NONE);
}

static void test_testrendev_exits(void) {
    dxl_decision d = decide("-testrendev=D3DDrv.D3DRenderDevice", 0, 0, 0);
    CHECK_INT(d.action, DXL_ACTION_TEST_RENDEV);
    CHECK_STR(d.test_rendev, "D3DDrv.D3DRenderDevice");
    CHECK_INT(d.screen, DXL_SCREEN_NONE);
}

/* Under -server there is nobody to show a screen to. */
static void test_server_shows_no_screen(void) {
    dxl_policy_input in = { .cmdline = "-server", .first_run = 0, .is_client = 0 };
    dxl_decision out;
    dxl_policy_decide(&in, &out);
    CHECK_INT(out.action, DXL_ACTION_LAUNCH);
    CHECK_INT(out.screen, DXL_SCREEN_NONE);
}

TEST_MAIN_BEGIN
    RUN(test_no_wizard_when_nothing_matches);
    RUN(test_row1_safe);
    RUN(test_row1_readini_matches_anywhere);
    RUN(test_row2_first_run);
    RUN(test_firstrun_flag_forces_the_gate);
    RUN(test_row3_changevideo);
    RUN(test_row4_recovery);
    RUN(test_recovery_needs_no_other_instance);
    RUN(test_safe_beats_first_run);
    RUN(test_safe_beats_recovery);
    RUN(test_first_run_beats_changevideo);
    RUN(test_changevideo_beats_recovery);
    RUN(test_bypass_tokens_skip_forwarding);
    RUN(test_other_instance_decides_on);
    RUN(test_consolecommand_exits);
    RUN(test_testrendev_exits);
    RUN(test_server_shows_no_screen);
TEST_MAIN_END
