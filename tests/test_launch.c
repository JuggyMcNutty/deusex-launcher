#include "test.h"
#include "scratch.h"
#include "launch/launch.h"
#include "core/instance.h"
#include "core/renderdev.h"
#include "platform/launch.h"

#include <limits.h>
#include <sys/wait.h>

/* The whole run, dx-reverse-info/launch-flow.md, down every road it can
 * end by. The screens are stand-ins that record what they were asked and
 * answer as each test says; the game is a shell script that records its
 * arguments, one a line, talks on the launcher's line when told to, and
 * exits as told. */

static char root[256], sys[300], exe[320];

/* ---- the stand-in screens ----------------------------------------------- */

typedef struct {
    int  splash_shows, splash_hides;
    char splash_path[512];
    int  wizards;
    dxl_screen wizard_start;
    char wizard_title[128];
    int  wizard_answer;             /* 1 Run, 0 Cancel */
    const char *wizard_relaunch;    /* flags to leave, or NULL */
    const char *wizard_renderer;    /* a GameRenderDevice to write, or NULL */
    int  ok_cancels;
    char ok_cancel_title[128];
    int  ok_cancel_answer;
    int  ok_cancel_inserts_disc;    /* the first OK comes with the palette in place */
    int  criticals;
    char critical_text[256];
    int  forward_after_ready;       /* pump: forward this once the splash is gone */
    const char *forward_line;
    dxl_instance *release_first;    /* pump: let this go before the forward */
    int  forward_tried, forwarded;
    int  stop_after_ready;          /* pump: ask the run to stop once the splash is gone */
    volatile int stop;
} stub;

static stub S;

static void s_splash_show(void *c, const char *p) {
    (void)c;
    S.splash_shows++;
    if (p) snprintf(S.splash_path, sizeof S.splash_path, "%s", p);
}
static void s_splash_hide(void *c) { (void)c; S.splash_hides++; }
static int s_wizard(void *c, dxl_wizard_request *r) {
    (void)c;
    S.wizards++;
    S.wizard_start = r->start;
    snprintf(S.wizard_title, sizeof S.wizard_title, "%s", r->title);
    if (S.wizard_renderer)
        dxl_ini_set(dxl_config_ini(r->config), "Engine.Engine", "GameRenderDevice", S.wizard_renderer);
    if (S.wizard_relaunch) r->relaunch_flags = strdup(S.wizard_relaunch);
    return S.wizard_answer;
}
static int s_ok_cancel(void *c, const char *title, const char *text) {
    (void)c; (void)text;
    S.ok_cancels++;
    snprintf(S.ok_cancel_title, sizeof S.ok_cancel_title, "%s", title);
    if (S.ok_cancel_inserts_disc) scratch_write(root, "Textures/Palettes.utx", "palette");
    return S.ok_cancel_answer;
}
static void s_critical(void *c, const char *title, const char *text) {
    (void)c; (void)title;
    S.criticals++;
    snprintf(S.critical_text, sizeof S.critical_text, "%s", text);
}
static void s_pump(void *c) {
    (void)c;
    if (S.stop_after_ready && S.splash_hides > 0) S.stop = 1;
    if (S.forward_after_ready && !S.forward_tried && S.splash_hides > 0) {
        dxl_instance_release(S.release_first);
        S.release_first = NULL;
        dxl_err e;
        S.forward_tried = 1;
        S.forwarded = dxl_instance_forward(S.forward_line, 1000, &e) == 0;
        /* Taken by no one: the game, which waits for it, is stopped. */
        if (!S.forwarded) S.stop = 1;
    }
}

static const dxl_launch_ui UI = {
    NULL, s_splash_show, s_splash_hide, s_wizard, s_ok_cancel, s_critical, s_pump,
};

/* ---- a stand-in install ------------------------------------------------- */

static const char *GAME =
    "#!/bin/sh\n"
    "here=$(dirname \"$0\")\n"
    "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$here/game-args.txt\"\n"
    "echo \"engine output line\"\n"
    "fd=$DXL_LAUNCHER_FD\n"
    "case \"$(cat \"$here/game-mode.txt\" 2>/dev/null)\" in\n"
    "ready) printf 'hello\\nready\\n' >&$fd ;;\n"
    "relay) printf 'hello\\nready\\n' >&$fd\n"
    "       read a <&$fd; read b <&$fd\n"
    "       printf '%s|%s' \"$a\" \"$b\" > \"$here/game-got.txt\" ;;\n"
    "stubborn) trap '' TERM; printf 'hello\\nready\\n' >&$fd; exec sleep 30 ;;\n"
    "relaunch) line=$(cat \"$here/game-line.txt\" 2>/dev/null || printf %s -changevideo)\n"
    "          printf 'hello\\nready\\nrelaunch %s\\n' \"$line\" >&$fd ;;\n"
    "esac\n"
    "exit $(cat \"$here/game-exit.txt\" 2>/dev/null || echo 0)\n";

static void install(const char *tag, const char *ini) {
    memset(&S, 0, sizeof S);
    snprintf(root, sizeof root, "%s", scratch_dir(tag));
    snprintf(sys, sizeof sys, "%s/System", root);
    snprintf(exe, sizeof exe, "%s/DeusEx", sys);
    scratch_write(root, "System/DeusEx.ini", ini ? ini :
                  "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nCdPath=..\\\r\nLanguage=int\r\n");
    scratch_write(root, "System/Startup.int",
                  "[General]\r\nFirstTime=FT\r\nSafeMode=SM\r\nVideo=VC\r\nRecoveryMode=RM\r\n");
    scratch_write(root, "System/Window.int",
                  "[General]\r\nInsertCdTitle=Cd Required At Startup\r\nInsertCdText=Insert it\r\n"
                  "[Errors]\r\nCritical=Critical Error\r\n");
    scratch_write(root, "System/run-game.sh", GAME);
    char *p = scratch_path(root, "System/run-game.sh");
    chmod(p, 0755);
    free(p);
    scratch_write(root, "Textures/Palettes.utx", "palette");
    scratch_write(root, "Help/Logo.bmp", "BM");
}

static char *read_file(const char *rel) {
    char *p = scratch_path(root, rel);
    char *s = slurp(p, NULL);
    free(p);
    return s ? s : strdup("");
}

static int ini_first_run(void) {
    char *s = read_file("System/DeusEx.ini");
    const char *f = strstr(s, "FirstRun=");
    int v = f ? atoi(f + 9) : -1;
    free(s);
    return v;
}

static void launch(const char **args, dxl_launch_result *res) {
    char *argv[16];
    int argc = 0;
    argv[argc++] = exe;
    for (; args && *args && argc < 15; args++) argv[argc++] = (char *)*args;
    argv[argc] = NULL;
    dxl_launch_run(argc, argv, exe, &UI, &S.stop, res);
}

/* ---- the roads ---------------------------------------------------------- */

static void test_settled_install_runs_the_game(void) {
    install("l-settled", NULL);
    dxl_launch_result r;
    launch((const char *[]){ "01_NYC_UNATCOIsland", "-hax0r", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(r.exit_code, 0);
    char *args = read_file("System/game-args.txt");
    CHECK_STR(args, "01_NYC_UNATCOIsland -hax0r\n");
    free(args);
    CHECK_INT(S.wizards, 0);
    CHECK_INT(S.splash_shows, 1);
    CHECK(strstr(S.splash_path, "Help/Logo.bmp") != NULL);
    CHECK(S.splash_hides >= 1);
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 0);   /* a clean end */
    char *log = read_file("System/DeusEx.log");
    CHECK(strstr(log, "Init: command line: 01_NYC_UNATCOIsland -hax0r") != NULL);
    CHECK(strstr(log, "engine output line") != NULL);            /* the game's output */
    free(log);
    CHECK_INT(ini_first_run(), 1100);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A game that ends badly leaves Running.ini: the next launch's recovery. */
static void test_a_crash_leaves_running_ini(void) {
    install("l-crash", NULL);
    scratch_write(root, "System/game-exit.txt", "3");
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_INT(r.exit_code, 3);
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 1);
    dxl_launch_result_free(&r);

    /* The next launch finds it, with nothing else running: RecoveryMode. */
    scratch_write(root, "System/game-exit.txt", "0");
    memset(&S, 0, sizeof S);
    S.wizard_answer = 1;
    launch(NULL, &r);
    CHECK_INT(S.wizards, 1);
    CHECK_INT(S.wizard_start, DXL_SCREEN_MAIN_RECOVERY);
    CHECK_STR(S.wizard_title, "RM");
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.splash_shows, 2);     /* closed for the wizard, shown again after */
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The first run's wizard, cancelled: no game, Running.ini gone, and what the
 * pages wrote kept -- the original writes its configuration back at exit. */
static void test_first_run_cancelled(void) {
    install("l-first", "[FirstRun]\r\nFirstRun=0\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    scratch_write(root, "System/Running.ini", "");
    S.wizard_renderer = "SoftDrv.SoftwareRenderDevice";
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_INT(S.wizard_start, DXL_SCREEN_RENDERER_FIRST);
    CHECK_STR(S.wizard_title, "FT");
    CHECK_STR(r.ended, "wizard cancelled");
    CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 0);
    char *ini = read_file("System/DeusEx.ini");
    CHECK(strstr(ini, "GameRenderDevice=SoftDrv.SoftwareRenderDevice") != NULL);
    free(ini);
    CHECK_INT(ini_first_run(), 0);    /* raised only on the way to the game */
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

static void test_safe_mode_relaunch(void) {
    install("l-safe", NULL);
    S.wizard_relaunch = " -nosound -nojoy";
    dxl_launch_result r;
    launch((const char *[]){ "-safe", NULL }, &r);
    CHECK_INT(S.wizard_start, DXL_SCREEN_MAIN_SAFE);
    CHECK_STR(S.wizard_title, "SM");
    CHECK_STR(r.ended, "relaunch");
    CHECK_STR(r.relaunch_flags, " -nosound -nojoy");
    CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The CD prompt's Cancel ends everything on the spot: Running.ini stays and
 * nothing is written back -- not even FirstRun's rise. */
static void test_cd_prompt_cancelled(void) {
    install("l-cd", "[FirstRun]\r\nFirstRun=500\r\n[Engine.Engine]\r\nCdPath=Z:\\nowhere\\\r\n");
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_INT(S.ok_cancels, 1);
    CHECK_STR(S.ok_cancel_title, "Cd Required At Startup");
    CHECK_STR(r.ended, "CD check cancelled");
    CHECK_INT(r.exit_code, 0);
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 1);
    CHECK_INT(ini_first_run(), 500);
    CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* OK once the disc is in: on to the game, FirstRun raised to 1100. */
static void test_cd_prompt_ok(void) {
    install("l-cdok", "[FirstRun]\r\nFirstRun=500\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    char *p = scratch_path(root, "Textures/Palettes.utx");
    remove(p);
    free(p);
    S.ok_cancel_answer = 1;
    S.ok_cancel_inserts_disc = 1;
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_INT(S.ok_cancels, 1);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(ini_first_run(), 1100);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

static void test_make_is_fatal(void) {
    install("l-make", NULL);
    dxl_launch_result r;
    launch((const char *[]){ "-make", NULL }, &r);
    CHECK_INT(r.exit_code, 1);
    CHECK_INT(S.criticals, 1);
    CHECK_STR(S.critical_text, "'DeusEx -make' is obsolete, use 'ucc make' now\r\n\r\nHistory: ");
    CHECK_INT(S.splash_shows, 0);
    CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* -consolecommand= and -testrendev= end the run before any wizard. */
static void test_the_two_exits_that_never_launch(void) {
    install("l-exits", "[FirstRun]\r\nFirstRun=0\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    dxl_launch_result r;
    launch((const char *[]){ "-consolecommand=ShowLog", NULL }, &r);
    CHECK_STR(r.ended, "-consolecommand");
    CHECK_INT(S.wizards, 0);
    dxl_launch_result_free(&r);

    /* Typed as two words, and as the Renderer page starts it, its line as
     * one: the device marked in its own section, the log Detected.log. */
    for (int as_line = 0; as_line < 2; as_line++) {
        memset(&S, 0, sizeof S);
        if (as_line) launch((const char *[]){ DXL_DETECT_LINE, NULL }, &r);
        else launch((const char *[]){ "testrendev=D3DDrv.D3DRenderDevice", "log=Detected.log", NULL }, &r);
        CHECK_STR(r.ended, "-testrendev");
        CHECK_INT(S.splash_shows, 0);     /* TestRenDev keeps the splash off */
        CHECK_INT(scratch_exists(root, "System/Detected.ini"), 1);
        CHECK_INT(scratch_exists(root, "System/Detected.log"), 1);   /* log= names the log */
        char *ini = read_file("System/DeusEx.ini");
        CHECK(strstr(ini, "[D3DDrv.D3DRenderDevice]\r\nDescFlags=2") != NULL);
        CHECK(strstr(ini, "log=Detected.log]") == NULL);
        free(ini);
        CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
        dxl_launch_result_free(&r);
        char *detected = scratch_path(root, "System/Detected.ini");
        remove(detected);
        free(detected);
        detected = scratch_path(root, "System/Detected.log");
        remove(detected);
        free(detected);
    }
    scratch_remove(root);
}

/* LOG= cut as Parse cuts it, at a comma too (Core.dll 0x10146c96): the log
 * is the file before it. */
static void test_log_cut_at_a_comma(void) {
    install("l-logcomma", NULL);
    dxl_launch_result r;
    launch((const char *[]){ "-consolecommand=ShowLog", "LOG=Comma.log,x", NULL }, &r);
    CHECK_STR(r.ended, "-consolecommand");
    CHECK_INT(scratch_exists(root, "System/Comma.log"), 1);
    CHECK_INT(scratch_exists(root, "System/Comma.log,x"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* An empty LOG= or ABSLOG= is the empty name, as the original's log device
 * takes it (DeusEx.exe 0x10903880, 0x109038a8): the base folder itself, or
 * no name, which no file opens as -- no log, the run going on; a LOG= found
 * is the one taken, an ABSLOG= after it unread. */
static void test_empty_log_names_no_log(void) {
    install("l-emptylog", NULL);
    char abslog[600];
    snprintf(abslog, sizeof abslog, "ABSLOG=%s/Abs.log", sys);
    dxl_launch_result r;
    launch((const char *[]){ "-consolecommand=ShowLog", "LOG=", abslog, NULL }, &r);
    CHECK_STR(r.ended, "-consolecommand");
    CHECK_INT(scratch_exists(root, "System/DeusEx.log"), 0);
    CHECK_INT(scratch_exists(root, "System/Abs.log"), 0);
    dxl_launch_result_free(&r);
    launch((const char *[]){ "-consolecommand=ShowLog", "ABSLOG=", NULL }, &r);
    CHECK_STR(r.ended, "-consolecommand");
    CHECK_INT(scratch_exists(root, "System/DeusEx.log"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A running instance that listens takes the command line; with a bypass
 * token the launch goes on instead. */
static void test_forwarding(void) {
    install("l-fwd", NULL);
    dxl_instance *primary = dxl_instance_acquire();
    dxl_launch_result r;
    launch((const char *[]){ "DXMP_Cathedral", NULL }, &r);
    CHECK_STR(r.ended, "forwarded");
    char got[256];
    CHECK_INT(dxl_instance_poll(primary, got, sizeof got), 1);
    CHECK_STR(got, "DXMP_Cathedral");
    CHECK_INT(scratch_exists(root, "System/DeusEx.log"), 0);   /* it never got that far */
    dxl_launch_result_free(&r);

    /* NewWindow: no forwarding. Running.ini is the live one's, not a crash. */
    scratch_write(root, "System/Running.ini", "");
    launch((const char *[]){ "-NewWindow", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.wizards, 0);
    dxl_launch_result_free(&r);
    dxl_instance_release(primary);
    scratch_remove(root);
}

/* The bypass reads the line the program's path starts: an install whose
 * folder holds a token is never forwarded, as the original's is not. */
static void test_forwarding_bypassed_by_the_path(void) {
    install("l-fwd-NewWindow", NULL);
    dxl_instance *primary = dxl_instance_acquire();
    scratch_write(root, "System/Running.ini", "");
    dxl_launch_result r;
    launch((const char *[]){ "DXMP_Cathedral", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    char got[256];
    CHECK_INT(dxl_instance_poll(primary, got, sizeof got), 0);
    dxl_launch_result_free(&r);
    dxl_instance_release(primary);
    scratch_remove(root);
}

/* -server: no splash, no wizard even on a first run. */
static void test_server(void) {
    install("l-server", "[FirstRun]\r\nFirstRun=0\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    dxl_launch_result r;
    launch((const char *[]){ "DXMP_Cathedral", "-server", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.wizards, 0);
    CHECK_INT(S.splash_shows, 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

static void test_splash_bitmaps(void) {
    install("l-splash", NULL);
    scratch_write(root, "Help/DeusExLogo.bmp", "BM");
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK(strstr(S.splash_path, "Help/DeusExLogo.bmp") != NULL);
    dxl_launch_result_free(&r);

    memset(&S, 0, sizeof S);
    launch((const char *[]){ "-log", NULL }, &r);
    CHECK_INT(S.splash_shows, 0);
    dxl_launch_result_free(&r);

    /* Neither bitmap: the original asserts and dies; this goes on without. */
    char *p = scratch_path(root, "Help/DeusExLogo.bmp");
    remove(p); free(p);
    p = scratch_path(root, "Help/Logo.bmp");
    remove(p); free(p);
    memset(&S, 0, sizeof S);
    launch(NULL, &r);
    CHECK_INT(S.splash_shows, 0);
    CHECK_STR(r.ended, "the game ran");
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The engine says it is up: the splash closes. A second launch's command
 * line then reaches it as the original's does: TakeFocus, then Open. */
static void test_ready_and_relay(void) {
    install("l-relay", NULL);
    scratch_write(root, "System/game-mode.txt", "relay");
    S.forward_after_ready = 1;
    S.forward_line = "DX.dx?Name=JC -x";
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.forwarded, 1);
    char *got = read_file("System/game-got.txt");
    CHECK_STR(got, "TakeFocus|Open DX.dx?Name=JC");
    free(got);
    char *log = read_file("System/DeusEx.log");
    CHECK(strstr(log, "WM_COPYDATA: DX.dx?Name=JC -x") != NULL);
    CHECK(strstr(log, "the engine is up") != NULL);
    free(log);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A client started beside a -server listens all the same, as the
 * original's every client's log window is IsBrowser and a server's never:
 * a third launch's line reaches its game. */
static void test_relay_beside_a_server(void) {
    install("l-relay-server", NULL);
    scratch_write(root, "System/game-mode.txt", "relay");
    dxl_instance *server = dxl_instance_acquire_lock();
    CHECK(server != NULL);
    S.forward_after_ready = 1;
    S.forward_line = "DX.dx?Name=JC -x";
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.forwarded, 1);
    char *got = read_file("System/game-got.txt");
    CHECK_STR(got, "TakeFocus|Open DX.dx?Name=JC");
    free(got);
    dxl_launch_result_free(&r);
    dxl_instance_release(server);
    scratch_remove(root);
}

/* A client started with a bypass word beside another listens too: with the
 * first gone, a launch's line reaches it, as the original's finds the log
 * window still there. */
static void test_relay_to_the_client_left(void) {
    install("l-relay-left", NULL);
    scratch_write(root, "System/game-mode.txt", "relay");
    S.release_first = dxl_instance_acquire();
    CHECK(S.release_first != NULL);
    S.forward_after_ready = 1;
    S.forward_line = "DX.dx?Name=JC -x";
    dxl_launch_result r;
    launch((const char *[]){ "-NewWindow", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(S.forwarded, 1);
    char *got = read_file("System/game-got.txt");
    CHECK_STR(got, "TakeFocus|Open DX.dx?Name=JC");
    free(got);
    dxl_launch_result_free(&r);
    dxl_instance_release(S.release_first);
    scratch_remove(root);
}

/* The engine's RELAUNCH: once the game has ended cleanly, the launcher is
 * started again with the line; after a crash, not. */
static void test_engine_relaunch(void) {
    install("l-relaunch", NULL);
    scratch_write(root, "System/game-mode.txt", "relaunch");
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_STR(r.relaunch_flags, "-changevideo");
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 0);
    char *log = read_file("System/DeusEx.log");
    CHECK(strstr(log, "Relaunch: -changevideo") != NULL);
    free(log);
    dxl_launch_result_free(&r);

    scratch_write(root, "System/game-exit.txt", "1");
    launch(NULL, &r);
    CHECK(r.relaunch_flags == NULL);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A RELAUNCH line of any length comes whole, as the original passes the
 * rest of the line on: none cut at 1,023 bytes, the rest a line of its own. */
static void test_engine_relaunch_long(void) {
    install("l-relaunch-long", NULL);
    scratch_write(root, "System/game-mode.txt", "relaunch");
    char want[3016];
    memset(want, 'a', 3000);
    memcpy(want + 3000, " -server", 9);
    scratch_write(root, "System/game-line.txt", want);
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(r.relaunch_flags ? (int)strlen(r.relaunch_flags) : 0, 3008);
    CHECK(r.relaunch_flags && strcmp(r.relaunch_flags, want) == 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* INI= and USERINI=, as the original's appInit takes them: the
 * configuration from those files, which the game is started with too.
 * INI= first: Parse finds "INI=" anywhere, so after USERINI= it would find
 * USERINI='s value, in the original as here. */
static void test_other_inis(void) {
    install("l-inis", "[FirstRun]\r\nFirstRun=0\r\n");
    scratch_write(root, "System/Mod.ini",
                  "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    scratch_write(root, "System/ModUser.ini", "[DeusEx.DeusExPlayer]\r\n");
    const char *args[] = { "INI=Mod.ini", "USERINI=ModUser.ini", NULL };
    dxl_launch_result r;
    launch(args, &r);
    CHECK_INT(S.wizards, 0);                    /* Mod.ini's FirstRun, not DeusEx.ini's */
    CHECK_STR(r.ended, "the game ran");
    char *args_got = read_file("System/game-args.txt");
    CHECK_STR(args_got, "INI=Mod.ini USERINI=ModUser.ini\n");
    free(args_got);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* Asked to stop, the launcher passes SIGTERM on; a game that takes no
 * notice of it is killed 5 s later, and Running.ini stays. */
static void test_stop_kills_a_stubborn_game(void) {
    install("l-stop", NULL);
    scratch_write(root, "System/game-mode.txt", "stubborn");
    S.stop_after_ready = 1;
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_STR(r.ended, "the game ran");
    CHECK_INT(r.exit_code, 128 + 9);
    CHECK(scratch_exists(root, "System/Running.ini"));
    char *log = read_file("System/DeusEx.log");
    CHECK(strstr(log, "stopping the game") != NULL);
    CHECK(strstr(log, "the game did not stop: killing it") != NULL);
    free(log);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A quoted value reaches the game as given, quotes and all: the command
 * line is the game's one argument, and the configuration is the named
 * file's, its space and all. */
static void test_quoted_ini(void) {
    install("l-quoted", "[FirstRun]\r\nFirstRun=0\r\n");
    scratch_write(root, "System/My Mod.ini",
                  "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    dxl_launch_result r;
    launch((const char *[]){ "INI=\"My Mod.ini\" -nosound", NULL }, &r);
    CHECK_INT(S.wizards, 0);                    /* My Mod.ini's FirstRun */
    CHECK_STR(r.ended, "the game ran");
    char *args = read_file("System/game-args.txt");
    CHECK_STR(args, "INI=\"My Mod.ini\" -nosound\n");
    free(args);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The real run-game.sh, with an engine that records its arguments: the
 * line reaches the engine whole, as --cmdline=. */
static void test_run_game_script(void) {
    install("l-script", NULL);
    scratch_write(root, "System/My Mod.ini",
                  "[FirstRun]\r\nFirstRun=1100\r\n[Engine.Engine]\r\nCdPath=..\\\r\n");
    size_t n = 0;
    char *script = slurp(DXL_RUN_GAME, &n);
    CHECK(script != NULL);
    if (!script) return;
    scratch_write(root, "System/run-game.sh", script);
    free(script);
    scratch_write(root, "System/DXEngine",
                  "#!/bin/sh\n"
                  "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$(dirname \"$0\")/engine-args.txt\"\n");
    char *p = scratch_path(root, "System/run-game.sh");
    chmod(p, 0755);
    free(p);
    p = scratch_path(root, "System/DXEngine");
    chmod(p, 0755);
    free(p);
    unsetenv("DXL_ENGINE_ARGS");
    dxl_launch_result r;
    launch((const char *[]){ "INI=\"My Mod.ini\"", "-log", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    char *args = read_file("System/engine-args.txt");
    CHECK(strstr(args, "--verbose\n") != NULL);   /* the log streamed, the engine's error window kept */
    CHECK(strstr(args, "--no-launcher\n") == NULL);
    CHECK(strstr(args, "--ini=") != NULL);
    CHECK(strstr(args, "\n--cmdline=INI=\"My Mod.ini\" -log\n") != NULL);
    free(args);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The real run-game.sh on an install whose inis are spelled in lower case:
 * the engine is given the files the launcher opened, deusex.ini and
 * user.ini, not names of another case that are not there. */
static void test_run_game_inis_in_any_case(void) {
    install("l-script-case", NULL);
    char *was = scratch_path(root, "System/DeusEx.ini"), *now = scratch_path(root, "System/deusex.ini");
    CHECK_INT(rename(was, now), 0);
    free(was);
    free(now);
    scratch_write(root, "System/user.ini", "[Engine.Input]\r\nF1=Help\r\n");
    size_t n = 0;
    char *script = slurp(DXL_RUN_GAME, &n);
    CHECK(script != NULL);
    if (!script) return;
    scratch_write(root, "System/run-game.sh", script);
    free(script);
    scratch_write(root, "System/DXEngine",
                  "#!/bin/sh\n"
                  "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$(dirname \"$0\")/engine-args.txt\"\n");
    char *p = scratch_path(root, "System/run-game.sh");
    chmod(p, 0755);
    free(p);
    p = scratch_path(root, "System/DXEngine");
    chmod(p, 0755);
    free(p);
    unsetenv("DXL_ENGINE_ARGS");
    dxl_launch_result r;
    launch((const char *[]){ "-log", NULL }, &r);
    CHECK_STR(r.ended, "the game ran");
    char *args = read_file("System/engine-args.txt");
    char want[1100];
    snprintf(want, sizeof want, "\n--ini=%s/deusex.ini\n", sys);
    CHECK(strstr(args, want) != NULL);
    snprintf(want, sizeof want, "\n--userini=%s/user.ini\n", sys);
    CHECK(strstr(args, want) != NULL);
    free(args);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* The real run-game.sh with no engine beside it: its exit of 127 is a
 * start that failed, said in the critical box, Running.ini kept. */
static void test_run_game_without_an_engine(void) {
    install("l-noengine", NULL);
    size_t n = 0;
    char *script = slurp(DXL_RUN_GAME, &n);
    CHECK(script != NULL);
    if (!script) return;
    scratch_write(root, "System/run-game.sh", script);
    free(script);
    char *p = scratch_path(root, "System/run-game.sh");
    chmod(p, 0755);
    free(p);
    dxl_launch_result r;
    launch(NULL, &r);
    CHECK_INT(r.exit_code, 127);
    CHECK_INT(S.criticals, 1);
    CHECK(strstr(S.critical_text, "no engine at ") != NULL && strstr(S.critical_text, "/System/DXEngine") != NULL);
    CHECK_INT(scratch_exists(root, "System/Running.ini"), 1);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* Safe mode's relaunch, and the engine's RELAUNCH: the launcher started
 * again with the line as its one argument, as it is. */
static void test_relaunch_line(void) {
    install("l-line", NULL);
    scratch_write(root, "System/relaunched",
                  "#!/bin/sh\n"
                  "for a in \"$@\"; do printf '%s\\n' \"$a\"; done > \"$(dirname \"$0\")/relaunch-args.txt\"\n");
    char *p = scratch_path(root, "System/relaunched");
    chmod(p, 0755);
    pid_t pid = fork();
    if (pid == 0) {
        dxl_err e;
        dxl_platform_launch(p, "INI=\"My Mod.ini\" -nosound -nojoy", sys, &e);
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    free(p);
    CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    char *args = read_file("System/relaunch-args.txt");
    CHECK_STR(args, "INI=\"My Mod.ini\" -nosound -nojoy\n");
    free(args);
    scratch_remove(root);
}

TEST_MAIN_BEGIN
    scratch_runtime_dir();
    RUN(test_quoted_ini);
    RUN(test_run_game_script);
    RUN(test_run_game_inis_in_any_case);
    RUN(test_run_game_without_an_engine);
    RUN(test_relaunch_line);
    RUN(test_other_inis);
    RUN(test_stop_kills_a_stubborn_game);
    RUN(test_settled_install_runs_the_game);
    RUN(test_a_crash_leaves_running_ini);
    RUN(test_first_run_cancelled);
    RUN(test_safe_mode_relaunch);
    RUN(test_cd_prompt_cancelled);
    RUN(test_cd_prompt_ok);
    RUN(test_make_is_fatal);
    RUN(test_the_two_exits_that_never_launch);
    RUN(test_log_cut_at_a_comma);
    RUN(test_empty_log_names_no_log);
    RUN(test_forwarding);
    RUN(test_forwarding_bypassed_by_the_path);
    RUN(test_server);
    RUN(test_splash_bitmaps);
    RUN(test_ready_and_relay);
    RUN(test_relay_beside_a_server);
    RUN(test_relay_to_the_client_left);
    RUN(test_engine_relaunch);
    RUN(test_engine_relaunch_long);
TEST_MAIN_END
