/* System: what went wrong, and ways to put it right.
 *
 * This replaces the original's safe mode. Its eight checkboxes became flags
 * on a relaunch (-nosound, -nohard, -window, ...), and Surreal Engine honours
 * none of them, so they would be buttons that do nothing. What actually
 * recovers a broken install here is below: the engine's own log, clearing
 * the crash marker, and putting each kind of configuration back.
 */
#include "screens_internal.h"
#include "core/log.h"
#include "core/paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DXL_VERSION
#define DXL_VERSION "unversioned"
#endif

static void last_run_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    int code;
    if (!last_engine_exit(s->app, &code))
        snprintf(out, n, "No run recorded");
    else if (code == 0)
        snprintf(out, n, "Clean exit");
    else
        snprintf(out, n, "Failed (exit code %d)", code);
}

static void last_run_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    char err[192];
    int code;
    if (last_engine_error(s->app, err, sizeof err))
        snprintf(out, n, "The engine reported: %s", err);
    else if (last_engine_exit(s->app, &code) && code != 0)
        snprintf(out, n, "The engine stopped without reporting an error. The log may say "
                 "more.");
    else
        snprintf(out, n, "From %s.", s->app->run_log);
}

static void log_activate(dxl_session *s, const dxl_row *r) {
    dxl_buf b;
    dxl_buf_init(&b);
    char *engine = read_tail(s->app->engine_log, 24 * 1024);
    char *run = read_tail(s->app->run_log, 8 * 1024);
    dxl_buf_printf(&b, "== %s\n%s\n", s->app->engine_log,
                   engine ? engine : "(not found -- the engine has not run yet)");
    dxl_buf_printf(&b, "\n== %s\n%s", s->app->run_log, run ? run : "(not found)");
    free(engine);
    free(run);
    open_text(s, "Engine log", b.data);
}

static int crash_marker_present(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (dxl_sentinel_exists(&s->app->sentinel)) return 1;
    snprintf(why, n, "There is no crash marker: the last run ended cleanly, or the "
             "game has not been run.");
    return 0;
}

static void clear_marker(dxl_session *s, const dxl_row *r) {
    dxl_sentinel_remove(&s->app->sentinel);
    if (dxl_sentinel_exists(&s->app->sentinel)) {
        notice(s, 1, "Could not delete %s.", dxl_sentinel_path(&s->app->sentinel));
        return;
    }
    s->crashed = 0;
    dxl_log("crash marker cleared from the System tab");
    notice(s, 0, "Crash marker cleared.");
}

/* ---- resets ------------------------------------------------------------ */

static void do_reset_video(dxl_session *s) {
    dxl_es_reset_section(s->app->es, "RenderDevice");
    dxl_es_reset_section(s->app->es, "Performance");   /* Distant AI is a Video tab row */
    const dxl_cpu_mode_info *cpu = dxl_target_cpu_mode(dxl_target_get(), NULL);
    if (cpu) dxl_app_set_cpu_mode(s->app, cpu->name);
    dxl_config_set_brightness(s->app->cfg, 0.5);
    dxl_config_set_decals(s->app->cfg, 1);
    dxl_log("video settings reset");
    notice(s, 0, "Video settings are back to this device's defaults.");
}
static void reset_video(dxl_session *s, const dxl_row *r) {
    open_confirm(s, "Reset video settings?",
                 "Every Video tab setting goes back to the defaults the launcher ships "
                 "with for this device; brightness goes to 50% and decals on.",
                 "Reset video", do_reset_video);
}

static void do_reset_controls(dxl_session *s) {
    dxl_es_reset_section(s->app->es, "Gamepad");
    dxl_app_choose_layout(s->app, dxl_pad_preset_default());
    dxl_log("controls reset");
    notice(s, 0, "Controls are back to the %s layout and default speeds.",
           dxl_pad_preset_default()->label);
}
static void reset_controls(dxl_session *s, const dxl_row *r) {
    open_confirm(s, "Reset controls?",
                 "The pad layout goes back to Modern, and look speed, dead zone, invert and "
                 "pointer speed to their defaults. Keyboard bindings are not touched.",
                 "Reset controls", do_reset_controls);
}

static void do_reset_game(dxl_session *s) {
    dxl_err err;
    if (dxl_app_reset_game_config(s->app, &err) != 0) {
        notice(s, 1, "Reset failed: %s", dxl_err_msg(&err));
        return;
    }
    notice(s, 0, "Game configuration rebuilt from Default.ini and DefUser.ini.");
}
static void reset_game(dxl_session *s, const dxl_row *r) {
    open_confirm(s, "Reset game configuration?",
                 "Deletes DeusEx.ini and User.ini, and the engine's SE-DeusEx.ini and "
                 "SE-User.ini, then rebuilds them from Default.ini and DefUser.ini. Options "
                 "set in the game and all key bindings return to their defaults. Saved "
                 "games are not touched.", "Delete and rebuild", do_reset_game);
}

static int have_default_ini(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    char *def = dxl_path_resolve_ci(s->app->system_dir, "Default.ini");
    int ok = def != NULL;
    free(def);
    if (!ok) snprintf(why, n, "Default.ini is missing from the game's System folder, so "
                      "there is nothing to rebuild from.");
    return ok;
}

/* ---- information ------------------------------------------------------- */

static void files_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s", s->app->install.ok ? "Complete" : "Incomplete");
}
static void files_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s. Configuration: %s; the engine reads client settings from %s.",
             s->app->game_dir, dxl_config_path(s->app->cfg),
             dxl_path_basename(dxl_config_client_path(s->app->cfg)));
}

static void version_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s", DXL_VERSION);
}

static void version_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    snprintf(out, n, "%s", dxl_target_get()->about);
}

static const dxl_row rows[] = {
    { .label = "Last run", .value = last_run_value, .describe = last_run_describe },
    { .label = "Engine log",
      .help = "The end of Surreal Engine's log and of the launch script's, to read what "
              "happened without a terminal.",
      .activate = log_activate },
    { .label = "Clear crash marker",
      .help = "Deletes Running.ini, which the game leaves behind when it does not exit "
              "cleanly and which brings up the crash notice on the next launch.",
      .enabled = crash_marker_present, .activate = clear_marker },
    { .label = "Reset video settings",
      .help = "Puts every Video tab setting back to this device's defaults.",
      .activate = reset_video },
    { .label = "Reset controls",
      .help = "Puts the pad layout and the Controls tab settings back to their defaults.",
      .activate = reset_controls },
    { .label = "Reset game configuration",
      .help = "Rebuilds the game's ini files from the shipped defaults. For when the game "
              "will not start or an in-game option has broken it.",
      .enabled = have_default_ini, .activate = reset_game },
    { .label = "Game files", .value = files_value, .describe = files_describe },
    { .label = "Launcher version", .value = version_value, .describe = version_describe },
};

dxl_rows tab_system_rows(void) {
    return (dxl_rows){ rows, (int)(sizeof rows / sizeof *rows) };
}
