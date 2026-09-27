/* Entry point.
 *
 * The launcher opens on its home screen every time: on a handheld the
 * settings have to be reachable with the pad, and a launcher that only
 * appears after a crash or with a command-line flag cannot be. START on the
 * home screen goes straight into the game.
 *
 * DXL_NO_HOME=1 keeps the original's behaviour of asking nothing when there
 * is nothing to ask, and exec'ing straight into the game. That is for
 * unattended runs -- scripts/dx.sh deploy over SSH, where no one can press
 * a button.
 */
#include "app.h"
#include "core/log.h"
#include "ui/screens.h"
#include "ui/ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The handoff timeout the original uses for its WM_COPYDATA
 * (SendMessageTimeout, 30 000 ms). Nothing should take that long; the limit
 * exists so a wedged primary cannot hang the messenger forever. */
#define HANDOFF_TIMEOUT_MS 30000

static int fail(const char *what, const dxl_err *e) {
    fprintf(stderr, "deusex-launcher: %s: %s\n", what, dxl_err_msg(e));
    dxl_log("fatal: %s: %s", what, dxl_err_msg(e));
    return 1;
}

static int want_home(const dxl_app *app) {
    const char *v = getenv("DXL_NO_HOME");
    int no_home = v && strcmp(v, "1") == 0;
    /* A missing install always needs the screen that says so; otherwise
     * DXL_NO_HOME skips the home screen unless the entry decision itself
     * has a question to ask (-safe, first run, a crash). */
    if (dxl_app_needs_install(app)) return 1;
    if (!no_home) return 1;
    return app->decision.screen != DXL_SCREEN_NONE;
}

int main(int argc, char **argv) {
    dxl_app app;
    dxl_err err;

    if (dxl_app_init(&app, argc, argv, &err) != 0)
        return fail("startup", &err);

    int rc = 0;

    switch (app.decision.action) {

    /* A second launch is only a messenger: hand the command line to the live
     * instance and exit without starting anything. */
    case DXL_ACTION_FORWARD:
        if (dxl_instance_forward(app.game_dir, app.cmdline,
                                 HANDOFF_TIMEOUT_MS, &err) != 0) {
            dxl_log("handoff failed: %s", dxl_err_msg(&err));
            /* The original gives up and exits rather than starting a second
             * copy; a failed handoff is not a reason to run two games. */
            rc = 1;
        } else {
            dxl_log("forwarded to running instance: \"%s\"", app.cmdline);
        }
        goto done;

    /* Both of these exit without ever launching. Neither is reachable from
     * the UI; they exist because the flags are part of the contract. */
    case DXL_ACTION_CONSOLE_CMD:
        dxl_log("console command requested: %s", app.decision.console_command);
        printf("%s\n", app.decision.console_command);
        fprintf(stderr, "deusex-launcher: -consolecommand needs a running "
                        "engine; nothing to run it against yet.\n");
        goto done;

    case DXL_ACTION_TEST_RENDEV:
        /* The original loads the render-device class, writes Detected.ini and
         * exits. dxl-cli --probe is this launcher's equivalent. */
        dxl_log("render device probe requested: %s", app.decision.test_rendev);
        fprintf(stderr, "deusex-launcher: -testrendev is not supported; "
                        "run dxl-cli --probe instead.\n");
        goto done;

    case DXL_ACTION_SCREEN:
    case DXL_ACTION_LAUNCH:
        break;
    }

    /* Before any display exists: the probe's child talks to the same GPU
     * stack the vendor SDL driver is about to open. */
    dxl_app_probe_gpu(&app);

    if (want_home(&app)) {
        dxl_ui *ui = dxl_ui_init(&err);
        if (!ui) {
            /* No display is not automatically fatal: a complete install can
             * still start with the settings it has. A missing one cannot. */
            if (dxl_app_needs_install(&app)) {
                fprintf(stderr, "deusex-launcher: game files not found under %s\n",
                        app.game_dir);
                for (int i = 0; i < app.install.item_count; i++)
                    if (!app.install.items[i].found)
                        fprintf(stderr, "  missing: %s\n", app.install.items[i].relative);
                rc = fail("no display", &err);
                goto done;
            }
            dxl_log("no display (%s); launching with the current settings",
                    dxl_err_msg(&err));
        } else {
            if (dxl_ui_headless(ui))
                dxl_log("warning: no usable font found; screens will be blank");

            dxl_session s;
            dxl_session_start(&s, &app, ui);
            while (!dxl_session_step(&s)) { /* one frame per iteration */ }
            dxl_end end = s.end;
            dxl_session_free(&s);
            dxl_ui_quit(ui);

            if (end == DXL_END_QUIT) {
                /* Leaving without playing keeps what was chosen, but creates
                 * no sentinel: nothing ran, so nothing can have crashed. */
                if (!dxl_app_needs_install(&app) && dxl_app_save_settings(&app, &err) != 0)
                    rc = fail("saving settings", &err);
                dxl_log("quit from the home screen; not launching");
                goto done;
            }
        }
    }

    /* Sentinel, FirstRun clamp, config write -- in that order. */
    if (dxl_app_commit(&app, &err) != 0) { rc = fail("commit", &err); goto done; }

    if (dxl_app_launch(&app, &err) != 0) {
        /* exec failed, so we are still here and own the sentinel. Clear it:
         * the game never started, and a stale marker would greet the next
         * launch with a crash notice for a crash that never happened. */
        dxl_sentinel_remove(&app.sentinel);
        rc = fail("launch", &err);
    }

done:
    dxl_app_shutdown(&app);
    return rc;
}
