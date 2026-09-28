#define _GNU_SOURCE
#include "launch/launch.h"

#include "core/cmdline.h"
#include "core/install.h"
#include "core/instance.h"
#include "core/log.h"
#include "core/migrate.h"
#include "core/paths.h"
#include "core/renderdev.h"
#include "core/sentinel.h"
#include "platform/machine.h"
#include "platform/process.h"

#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_SCRIPT        "run-game.sh"
#define HANDOFF_TIMEOUT_MS 30000   /* the original's SendMessageTimeout */
#define WAIT_STEP_MS       50      /* how often the windows are kept answering */
#define HELLO_TIMEOUT_MS   2000    /* an engine silent this long does not speak the line */
#define ENGINE_VERSION     1100    /* what FirstRun is raised to */

typedef struct {
    const dxl_launch_ui *ui;
    char        *raw;          /* the command line as Windows would give it: program and all */
    char        *cmd;          /* appCmdLine: without the program */
    char        *exe;
    char        *base;         /* appBaseDir: the game's System/ */
    char        *package;      /* appPackage */
    char        *id;           /* the install, for the single-instance lock */
    char        *log_path;
    dxl_config  *cfg;
    dxl_loc     *loc;
    dxl_sentinel sentinel;
    dxl_instance *inst;
    int          splash;       /* shown now */
} run;

static void splash_show(run *r, const char *path) {
    if (r->ui->splash_show) r->ui->splash_show(r->ui->ctx, path);
    r->splash = 1;
}

static void splash_hide(run *r) {
    if (r->splash && r->ui->splash_hide) r->ui->splash_hide(r->ui->ctx);
    r->splash = 0;
}

/* The command line Windows hands a program: its own path first. Three of
 * the original's checks read this one (dx-reverse-info/cli-flags.md). */
static char *raw_cmdline(int argc, char **argv, const char *exe) {
    dxl_buf b;
    dxl_buf_init(&b);
    dxl_buf_puts(&b, argc > 0 && argv[0] ? argv[0] : exe);
    for (int i = 1; i < argc; i++) dxl_buf_word(&b, argv[i]);
    return b.data;
}

static char *package_of(const char *exe) {
    char *name = dxl_xstrdup(dxl_path_basename(exe));
    char *dot = strrchr(name, '.');
    if (dot && dot != name) *dot = '\0';
    return name;
}

/* LOG= (from the base directory), else ABSLOG=, else <Package>.log. Like the
 * original's, the first is looked for first, and as a substring. */
static char *log_path_of(const run *r) {
    char v[PATH_MAX];
    if (dxl_cmd_value(r->cmd, "LOG", v, sizeof v) && *v) {
        char *p = dxl_path_from_ini(v);
        char *joined = dxl_path_join(r->base, p);
        free(p);
        return joined;
    }
    if (dxl_cmd_value(r->cmd, "ABSLOG", v, sizeof v) && *v) return dxl_path_from_ini(v);
    size_t n = strlen(r->package) + 5;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, "%s.log", r->package);
    char *p = dxl_path_join(r->base, leaf);
    free(leaf);
    return p;
}

/* ..\Help\<Package>Logo.bmp, else ..\Help\Logo.bmp; NULL when neither is
 * there -- where the original asserts and dies, this goes on without a
 * splash, as the RE advises. */
static char *splash_bitmap(const run *r) {
    char *game = dxl_path_dirname(r->base);
    size_t n = strlen(r->package) + 16;
    char *leaf = dxl_xmalloc(n);
    snprintf(leaf, n, "Help/%sLogo.bmp", r->package);
    char *p = dxl_path_resolve_ci(game, leaf);
    if (!p) p = dxl_path_resolve_ci(game, "Help/Logo.bmp");
    free(leaf);
    free(game);
    return p;
}

/* ParseToken: the first word of a line, or a quoted one. */
static int first_token(const char *s, char *out, size_t size) {
    while (*s && isspace((unsigned char)*s)) s++;
    if (!*s || !size) return 0;
    size_t n = 0;
    if (*s == '"') {
        for (s++; *s && *s != '"'; s++) if (n + 1 < size) out[n++] = *s;
    } else {
        for (; *s && !isspace((unsigned char)*s); s++) if (n + 1 < size) out[n++] = *s;
    }
    out[n] = '\0';
    return 1;
}

static void end(dxl_launch_result *out, int code, const char *where) {
    out->exit_code = code;
    out->ended = where;
    dxl_log("ended: %s (%d)", where, code);
}

/* What every run that made no game ends with, as WinMain's last steps: the
 * configuration written back, Running.ini deleted. */
static void end_without_game(run *r) {
    splash_hide(r);
    dxl_err e;
    if (r->cfg && dxl_config_save(r->cfg, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    dxl_sentinel_remove(&r->sentinel);
}

/* The wizard, as InitEngine runs it. 1 to go on to the game. */
static int wizard(run *r, const dxl_decision *d, dxl_launch_result *out) {
    dxl_wizard_request req;
    memset(&req, 0, sizeof req);
    req.start = d->screen;
    req.title = dxl_loc_general(r->loc, "Startup", d->caption_key);
    req.config = r->cfg;
    req.loc = r->loc;
    req.system_dir = r->base;
    req.package = r->package;
    req.exe_path = r->exe;
    req.cmdline = r->cmd;
    dxl_machine_probe(&req.machine);

    dxl_log("wizard: %s", req.title);
    splash_hide(r);
    int go = r->ui->wizard ? r->ui->wizard(r->ui->ctx, &req) : 0;
    out->relaunch_flags = req.relaunch_flags;   /* safe mode's, or NULL */
    if (go) splash_show(r, NULL);
    return go;
}

/* The CD check: loop until <CdPath>Textures\Palettes.utx is there and not
 * empty. 0 when the player cancels: the original then ends its process on
 * the spot. */
static int cd_check(run *r) {
    const char *cd = dxl_config_cd_path(r->cfg);
    if (!cd || !*cd) return 1;
    char *game = dxl_path_dirname(r->base);
    int ok;
    while (!(ok = dxl_install_cd_ok(game, cd))) {
        dxl_log("CD check: no %sTextures\\Palettes.utx", cd);
        const char *title = dxl_loc_general(r->loc, "Window", "InsertCdTitle");
        const char *text = dxl_loc_general(r->loc, "Window", "InsertCdText");
        if (!r->ui->ok_cancel || !r->ui->ok_cancel(r->ui->ctx, title, text)) break;
        cd = dxl_config_cd_path(r->cfg);
    }
    free(game);
    return ok;
}

/* The game, and the launcher staying with it as the original's process
 * stays: the splash closes when the engine is up, a forwarded command line
 * is handed on, and Running.ini goes once the game has ended cleanly. */
static int play(run *r, volatile int *stop) {
    char *script = dxl_path_join(r->base, GAME_SCRIPT);
    dxl_err e;
    dxl_game g;
    dxl_log("starting the game: %s %s", script, r->cmd);
    /* The game's output goes into the same log, after the launcher's. */
    int started = dxl_game_start(&g, script, r->cmd, r->base, r->log_path, &e);
    free(script);
    if (started != 0) {
        dxl_log("cannot start the game: %s", dxl_err_msg(&e));
        splash_hide(r);
        if (r->ui->critical)
            r->ui->critical(r->ui->ctx, dxl_loc_get(r->loc, "Window", "Errors", "Critical", 0),
                            dxl_err_msg(&e));
        return 1;                    /* Running.ini stays: the game failed */
    }

    int hello = 0, ready = 0, signalled = 0, waited = 0;
    for (;;) {
        char line[1024];
        int ev = dxl_game_wait(&g, dxl_instance_fd(r->inst), WAIT_STEP_MS, line, sizeof line);
        if (ev & DXL_GAME_LINE) {
            if (strcmp(line, "hello") == 0) {
                hello = 1;
            } else if (strcmp(line, "ready") == 0 && !ready) {
                ready = 1;
                dxl_log("the engine is up");
                splash_hide(r);
            }
        }
        if (ev & DXL_GAME_EXTRA) {
            char msg[DXL_HANDOFF_MAX];
            while (dxl_instance_poll(r->inst, msg, sizeof msg)) {
                dxl_log("WM_COPYDATA: %s", msg);
                /* The original's log window runs it only once the main loop
                 * has given it the engine. */
                if (!ready) continue;
                dxl_game_send(&g, "TakeFocus");
                char url[DXL_HANDOFF_MAX];
                if (first_token(msg, url, sizeof url) && url[0] != '-') {
                    char open[DXL_HANDOFF_MAX + 8];
                    snprintf(open, sizeof open, "Open %s", url);
                    dxl_game_send(&g, open);
                }
            }
        }
        if (ev & DXL_GAME_ENDED) break;
        if (!hello && r->splash && (waited += WAIT_STEP_MS) >= HELLO_TIMEOUT_MS) {
            dxl_log("the engine does not answer on its line; the splash closes now");
            splash_hide(r);
        }
        if (stop && *stop && !signalled) {
            dxl_log("stopping the game");
            dxl_game_signal(&g, SIGTERM);
            signalled = 1;
        }
        if (r->ui->pump) r->ui->pump(r->ui->ctx);
    }
    splash_hide(r);
    dxl_log("the game ended: %d", g.status);
    if (dxl_game_clean(&g)) dxl_sentinel_remove(&r->sentinel);
    else dxl_log("not a clean end: Running.ini stays for the next launch");
    return g.status;
}

void dxl_launch_run(int argc, char **argv, const char *exe_path, const dxl_launch_ui *ui,
                    volatile int *stop, dxl_launch_result *out) {
    memset(out, 0, sizeof *out);
    run r;
    memset(&r, 0, sizeof r);
    r.ui = ui;
    r.exe = dxl_xstrdup(exe_path);
    r.base = dxl_path_dirname(exe_path);
    r.package = package_of(exe_path);
    r.raw = raw_cmdline(argc, argv, exe_path);
    r.cmd = dxl_cmdline_join(argc, argv);
    char *real = realpath(r.base, NULL);
    r.id = real ? real : dxl_xstrdup(r.base);
    dxl_sentinel_init(&r.sentinel, r.base);

    /* 1. Forwarding, before anything else. */
    static const char *const bypass[] = { "Server", "NewWindow", "changevideo", "TestRenDev" };
    int skip = 0;
    for (size_t i = 0; i < sizeof bypass / sizeof *bypass; i++)
        if (dxl_cmd_find(r.raw, bypass[i])) skip = 1;
    if (!skip) {
        dxl_err e;
        if (dxl_instance_forward(r.id, r.cmd, HANDOFF_TIMEOUT_MS, &e) == 0) {
            out->exit_code = 0;
            out->ended = "forwarded";
            goto done;
        }
    }

    /* 2. appInit: the log, the configuration; -make; client or server. */
    r.log_path = log_path_of(&r);
    dxl_log_open_new(r.log_path);
    dxl_log("Init: command line: %s", r.cmd);
    dxl_log("Init: base directory: %s", r.base);
    r.cfg = dxl_config_open(r.base, r.package);
    if (dxl_config_seeded(r.cfg)) {
        dxl_err e;
        dxl_log("%s created from Default.ini", dxl_config_path(r.cfg));
        if (dxl_config_save(r.cfg, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    }
    r.loc = dxl_loc_open(r.base, dxl_ini_get(dxl_config_ini(r.cfg), "Engine.Engine", "Language"));
    if (dxl_cmd_param(r.cmd, "make")) {
        const char *msg = "'DeusEx -make' is obsolete, use 'ucc make' now";
        dxl_log("Critical: %s", msg);
        if (ui->critical)
            ui->critical(ui->ctx, dxl_loc_get(r.loc, "Window", "Errors", "Critical", 0), msg);
        end(out, 1, "-make");
        goto done;
    }
    int client = !dxl_cmd_param(r.cmd, "server");

    /* 3. The splash. */
    if (!dxl_cmd_param(r.raw, "log") && !dxl_cmd_param(r.raw, "server") &&
        !dxl_cmd_find(r.raw, "TestRenDev")) {
        char *bmp = splash_bitmap(&r);
        if (bmp) splash_show(&r, bmp);
        else dxl_log("no splash bitmap");
        free(bmp);
    }

    /* 4 and 5.3. The lock, and for a client the line a second launch
     * forwards to (the original's IsBrowser window). */
    int other = dxl_instance_other_running(r.id);
    r.inst = client ? dxl_instance_acquire(r.id) : dxl_instance_acquire_lock(r.id);

    /* 5. InitEngine. */
    dxl_policy_input in = {
        .cmdline = r.cmd,
        .first_run = dxl_config_first_run(r.cfg),
        .other_instance = other,
        .running_ini_exists = dxl_sentinel_exists(&r.sentinel),
        .is_client = client,
        .forward_tried = 1,
    };
    dxl_decision d;
    dxl_policy_decide(&in, &d);
    dxl_log("decision: %s, %s; FirstRun %d; another instance %d; Running.ini %d",
            dxl_action_name(d.action), dxl_screen_name(d.screen), d.effective_first_run,
            other, in.running_ini_exists);

    if (d.migrate_saves) {
        int n = dxl_migrate_saves(r.base, dxl_config_user_ini(r.cfg));
        if (n) dxl_log("save migration: %d slot names", n);
    }
    if (d.action == DXL_ACTION_CONSOLE_CMD) {
        /* The launcher's exec hook: ShowLog, TakeFocus, EditActor and the
         * rest act on windows and an engine this run has not made. */
        dxl_log("Executing console command %s", d.console_command);
        end_without_game(&r);
        end(out, 0, "-consolecommand");
        goto done;
    }
    if (d.action == DXL_ACTION_TEST_RENDEV) {
        dxl_err e;
        dxl_log("Detecting %s", d.test_rendev);
        if (dxl_renderdev_testrendev(r.cfg, r.base, d.test_rendev, &e) != 0)
            dxl_log("warning: %s", dxl_err_msg(&e));
        end_without_game(&r);
        end(out, 0, "-testrendev");
        goto done;
    }
    if (d.action == DXL_ACTION_SCREEN && !wizard(&r, &d, out)) {
        end_without_game(&r);
        end(out, 0, out->relaunch_flags ? "relaunch" : "wizard cancelled");
        goto done;
    }

    dxl_err e;
    if (dxl_sentinel_create(&r.sentinel, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    int first_run = d.effective_first_run < ENGINE_VERSION ? ENGINE_VERSION : d.effective_first_run;
    dxl_ini_set_int(dxl_config_ini(r.cfg), "FirstRun", "FirstRun", first_run);

    if (!cd_check(&r)) {
        /* ExitProcess: nothing written back, Running.ini left. */
        splash_hide(&r);
        end(out, 0, "CD check cancelled");
        goto done;
    }

    /* The engine reads the configuration from disk: it is written before it
     * starts, and never after -- by then the engine keeps it. */
    if (dxl_config_save(r.cfg, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    int status = play(&r, stop);
    end(out, status, "the game ran");

done:
    dxl_log("Exit");
    dxl_log_close();
    if (r.inst) dxl_instance_release(r.inst);
    dxl_loc_free(r.loc);
    dxl_config_free(r.cfg);
    dxl_sentinel_free(&r.sentinel);
    free(r.raw); free(r.cmd); free(r.exe); free(r.base); free(r.package);
    free(r.id); free(r.log_path);
}

void dxl_launch_result_free(dxl_launch_result *r) {
    if (!r) return;
    free(r->relaunch_flags);
    r->relaunch_flags = NULL;
}
