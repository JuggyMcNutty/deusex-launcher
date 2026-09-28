#include "test.h"
#include "scratch.h"
#include "launch/launch.h"
#include "core/instance.h"

#include <limits.h>

/* The whole run, dx-reverse-info/launch-flow.md, down every road it can
 * end by. The screens are stand-ins that record what they were asked and
 * answer as each test says; the game is a shell script that records its
 * arguments, talks on the launcher's line when told to, and exits as told. */

static char root[256], sys[300], exe[320], id[PATH_MAX];

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
    int  forwarded;
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
    if (S.forward_after_ready && !S.forwarded && S.splash_hides > 0) {
        dxl_err e;
        S.forwarded = dxl_instance_forward(id, S.forward_line, 1000, &e) == 0;
    }
}

static const dxl_launch_ui UI = {
    NULL, s_splash_show, s_splash_hide, s_wizard, s_ok_cancel, s_critical, s_pump,
};

/* ---- a stand-in install ------------------------------------------------- */

static const char *GAME =
    "#!/bin/sh\n"
    "here=$(dirname \"$0\")\n"
    "echo \"$@\" > \"$here/game-args.txt\"\n"
    "echo \"engine output line\"\n"
    "fd=$DXL_LAUNCHER_FD\n"
    "case \"$(cat \"$here/game-mode.txt\" 2>/dev/null)\" in\n"
    "ready) printf 'hello\\nready\\n' >&$fd ;;\n"
    "relay) printf 'hello\\nready\\n' >&$fd\n"
    "       read a <&$fd; read b <&$fd\n"
    "       printf '%s|%s' \"$a\" \"$b\" > \"$here/game-got.txt\" ;;\n"
    "stubborn) trap '' TERM; printf 'hello\\nready\\n' >&$fd; exec sleep 30 ;;\n"
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
    if (!realpath(sys, id)) snprintf(id, sizeof id, "%s", sys);
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

    memset(&S, 0, sizeof S);
    launch((const char *[]){ "testrendev=D3DDrv.D3DRenderDevice", "log=Detected.log", NULL }, &r);
    CHECK_STR(r.ended, "-testrendev");
    CHECK_INT(S.splash_shows, 0);     /* TestRenDev keeps the splash off */
    CHECK_INT(scratch_exists(root, "System/Detected.ini"), 1);
    CHECK_INT(scratch_exists(root, "System/Detected.log"), 1);   /* log= names the log */
    char *ini = read_file("System/DeusEx.ini");
    CHECK(strstr(ini, "[D3DDrv.D3DRenderDevice]\r\nDescFlags=2") != NULL);
    free(ini);
    CHECK_INT(scratch_exists(root, "System/game-args.txt"), 0);
    dxl_launch_result_free(&r);
    scratch_remove(root);
}

/* A running instance that listens takes the command line; with a bypass
 * token the launch goes on instead. */
static void test_forwarding(void) {
    install("l-fwd", NULL);
    dxl_instance *primary = dxl_instance_acquire(id);
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

TEST_MAIN_BEGIN
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
    RUN(test_forwarding);
    RUN(test_server);
    RUN(test_splash_bitmaps);
    RUN(test_ready_and_relay);
TEST_MAIN_END
