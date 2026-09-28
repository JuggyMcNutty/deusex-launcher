/* DeusEx: System/DeusEx.exe, on Linux.
 *
 * main is WinMain's first and last lines. The launcher finds itself -- its
 * folder is the game's System/, its name the package -- and runs the
 * original's launch (launch/launch.h) with its screens drawn here: the
 * splash, the wizard, the two message boxes. Then it ends as the run says,
 * with the game's code, or, after safe mode's Run!, as itself again with the
 * safe-mode flags (the original starts a new process as it ends; here the
 * process becomes it, once everything else is done).
 *
 * The screens need a display only when one is shown: a -server run, the
 * Renderer page's detection run and a forwarded command line open none.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* SDL's flags may have it already */
#endif
#include "launch/launch.h"

#include "core/log.h"
#include "core/paths.h"
#include "gui/msgbox.h"
#include "gui/splash.h"
#include "platform/launch.h"
#include "wizard/wizard.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    dxl_gui     gui;
    int         tried, up;
    dxl_splash *splash;
} screens;

static volatile int stop;

static void on_signal(int sig) {
    (void)sig;
    stop = 1;
}

/* SDL and the font, the first time a screen is wanted. */
static int up(screens *s) {
    if (!s->tried) {
        dxl_err e;
        s->tried = 1;
        s->up = dxl_gui_init(&s->gui, 1, &e) == 0;
        if (s->up) s->gui.quit = &stop;
        else dxl_log("no screens: %s", dxl_err_msg(&e));
    }
    return s->up;
}

static void splash_show(void *ctx, const char *bmp_path) {
    screens *s = ctx;
    if (!up(s)) return;
    if (!s->splash) s->splash = dxl_splash_new();
    dxl_splash_show(s->splash, bmp_path);
}

static void splash_hide(void *ctx) {
    screens *s = ctx;
    if (s->splash) dxl_splash_hide(s->splash);
}

static int wizard(void *ctx, dxl_wizard_request *req) {
    screens *s = ctx;
    if (!up(s)) return 0;
    dxl_err e;
    dxl_wizard *w = dxl_wizard_new(&s->gui, req, 0, &e);
    if (!w) {
        dxl_log("the wizard: %s", dxl_err_msg(&e));
        return 0;
    }
    int go = dxl_wizard_run(w);
    dxl_wizard_free(w);
    return go;
}

static int ok_cancel(void *ctx, const char *title, const char *text) {
    screens *s = ctx;
    if (!up(s)) return 0;
    dxl_msgbox_spec m = { .title = title, .text = text, .ok_cancel = 1 };
    return dxl_msgbox(&s->gui, &m);
}

static void critical(void *ctx, const char *title, const char *text) {
    screens *s = ctx;
    if (!up(s)) {
        fprintf(stderr, "%s: %s\n", title, text);
        return;
    }
    dxl_msgbox_spec m = { .title = title, .text = text, .error_icon = 1 };
    dxl_msgbox(&s->gui, &m);
}

/* While the game runs: the splash answers its window's events. */
static void pump(void *ctx) {
    screens *s = ctx;
    if (!s->up) return;
    SDL_Event e;
    while (SDL_PollEvent(&e))
        if (s->splash) dxl_splash_event(s->splash, &e);
}

int main(int argc, char **argv) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* GModuleFilename: the program itself, whatever path started it. */
    char *exe = dxl_path_self();
    if (!exe && argc > 0) exe = realpath(argv[0], NULL);
    if (!exe) {
        fprintf(stderr, "DeusEx: cannot find where this program is\n");
        return 1;
    }

    screens s;
    memset(&s, 0, sizeof s);
    dxl_launch_ui ui = {
        .ctx = &s,
        .splash_show = splash_show,
        .splash_hide = splash_hide,
        .wizard = wizard,
        .ok_cancel = ok_cancel,
        .critical = critical,
        .pump = pump,
    };
    dxl_launch_result r;
    dxl_launch_run(argc, argv, exe, &ui, &stop, &r);

    dxl_splash_free(s.splash);
    if (s.up) dxl_gui_quit(&s.gui);

    if (r.relaunch_flags && !stop) {
        /* Safe mode's Run!: the launcher again, with those flags alone, in
         * its own folder. */
        char *base = dxl_path_dirname(exe);
        dxl_err e;
        dxl_platform_launch(exe, r.relaunch_flags, base, &e);
        fprintf(stderr, "DeusEx: %s\n", dxl_err_msg(&e));
        free(base);
        r.exit_code = 1;
    }
    int code = r.exit_code;
    dxl_launch_result_free(&r);
    free(exe);
    return code;
}
