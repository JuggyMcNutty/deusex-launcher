/* The launcher's decision tree.
 *
 * This is InitEngine's policy (dx-reverse-info/launch-flow.md section 5 and
 * dx-reverse-info/wizard.md "Entry decision tree") expressed as a pure function: given
 * the command line and four facts about the world, decide what happens. No
 * I/O, no globals, no UI -- which is what makes the whole entry matrix
 * testable in a unit test rather than by launching a game.
 *
 * The evaluation order is load-bearing. FIRST MATCH WINS, and "-safe" beats
 * a pending crash sentinel, not the other way round.
 */
#ifndef DXL_POLICY_H
#define DXL_POLICY_H

#include "core/common.h"

typedef enum {
    DXL_ACTION_LAUNCH,        /* no wizard; start the game directly */
    DXL_ACTION_SCREEN,        /* show a screen first */
    DXL_ACTION_FORWARD,       /* hand the command line to a running instance */
    DXL_ACTION_CONSOLE_CMD,   /* -consolecommand=: run it, never launch */
    DXL_ACTION_TEST_RENDEV    /* -testrendev=: probe, write Detected.ini, exit */
} dxl_action;

typedef enum {
    DXL_SCREEN_NONE,
    DXL_SCREEN_MAIN_SAFE,       /* menu page, explicit safe-mode wording */
    DXL_SCREEN_MAIN_RECOVERY,   /* menu page, "was not shut down properly" */
    DXL_SCREEN_RENDERER_FIRST,  /* renderer page, first-run caption */
    DXL_SCREEN_RENDERER_VIDEO   /* renderer page, -changevideo caption */
} dxl_screen;

typedef struct {
    const char *cmdline;        /* argv[1..] joined; never NULL */
    int first_run;              /* [FirstRun] FirstRun as read from config */
    int other_instance;         /* another launcher/game is live */
    int running_ini_exists;     /* the crash sentinel survived */
    int is_client;              /* GIsClient: 0 under -server */
} dxl_policy_input;

typedef struct {
    dxl_action action;
    dxl_screen screen;

    /* Startup.int [General] key for this screen's caption, or NULL.
     * One of: SafeMode, RecoveryMode, FirstTime, Video. */
    const char *caption_key;

    int effective_first_run;    /* after -firstrun forces it to 0 */
    int migrate_saves;          /* effective_first_run < 220 */
    int show_splash;            /* suppressed by -log, -server, TestRenDev */
    int skip_handoff;           /* one of the four bypass tokens present */

    char console_command[512];  /* DXL_ACTION_CONSOLE_CMD */
    char test_rendev[160];      /* DXL_ACTION_TEST_RENDEV */
    char exec_file[256];        /* -EXEC=, run after the engine starts */
} dxl_decision;

void dxl_policy_decide(const dxl_policy_input *in, dxl_decision *out);

/* Human-readable names, for --dry-run output and logs. */
const char *dxl_action_name(dxl_action a);
const char *dxl_screen_name(dxl_screen s);

#endif
