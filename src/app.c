#define _GNU_SOURCE
#include "app.h"

#include "core/cmdline.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/gpu_probe.h"
#include "platform/launch.h"
#include "platform/target.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The launcher's own settings, kept separate from the game's config so that
 * resetting the game ini cannot lose the path to the game files. */
#define LAUNCHER_INI  "launcher.ini"
#define RENDERERS_INI "renderers.ini"

/* Relative to the launcher's directory; run-game.sh pins HOME to <app>/home,
 * so this is where the engine looks for its settings. */
#define DEFAULT_ENGINE_SETTINGS "home/.config/SurrealEngine/Settings.json"
#define DEFAULT_ENGINE_DEFAULTS "engine-settings.json.default"
#define DEFAULT_ENGINE_LOG      "home/.config/SurrealEngine/SE-Log-LastRun.txt"
#define DEFAULT_RUN_LOG         "run-game.log"

/* The GPU probe gets this long. vkCreateInstance on the PowerVR stack takes
 * well under a second; anything past this is a hang, not a slow driver. */
#define GPU_PROBE_TIMEOUT_MS 4000

static char *dup_or(const char *v, const char *fallback) {
    return dxl_xstrdup(v && *v ? v : fallback);
}

/* A launcher.ini path: absolute as given, otherwise relative to the app. */
static char *app_path(const dxl_app *app, const dxl_ini *l, const char *key,
                      const char *fallback) {
    const char *v = l ? dxl_ini_get(l, "Launcher", key) : NULL;
    char *p = dxl_path_from_ini(v && *v ? v : fallback);
    if (p[0] == '/') return p;
    char *joined = dxl_path_join(app->app_dir, p[0] == '.' && p[1] == '/' ? p + 2 : p);
    free(p);
    return joined;
}

/* The first time the launcher meets an install whose pad bindings are still
 * the ones Deus Ex shipped, switch to the default layout: the shipped set
 * binds three buttons and nothing else. A layout an earlier launcher applied
 * and a later one revised is moved to the revision. A layout chosen on
 * purpose -- the Gamepad.Layout member exists -- is otherwise left alone. */
static void apply_first_contact_layout(dxl_app *app) {
    dxl_ini *user = dxl_config_user_ini(app->cfg);
    if (!user) return;

    const dxl_pad_preset *now = dxl_bindings_detect(user);
    if (!now) {
        /* A preset from an earlier launcher that has since been revised. */
        const char *what = NULL;
        const dxl_pad_preset *next = dxl_bindings_detect_retired(user, &what);
        if (next) {
            dxl_app_choose_layout(app, next);
            app->pad_layout_update = what;
            dxl_log("pad layout: earlier '%s' layout found; updated it", next->id);
            return;
        }
    }
    if (dxl_es_was_present(app->es, DXL_ES_PAD_LAYOUT)) return;

    if (now && strcmp(now->id, "classic") == 0) {
        dxl_app_choose_layout(app, dxl_pad_preset_default());
        app->pad_layout_applied = 1;
        dxl_log("pad layout: shipped bindings found; applied '%s'",
                dxl_pad_preset_default()->id);
    } else {
        /* Custom or already a preset: just record what is there. */
        dxl_es_set_choice(app->es, DXL_ES_PAD_LAYOUT, now ? now->id : "custom");
    }
}

int dxl_app_init(dxl_app *app, int argc, char **argv, dxl_err *err) {
    memset(app, 0, sizeof *app);

    app->exe_path = dxl_path_self();
    if (!app->exe_path) app->exe_path = dxl_xstrdup(argv[0]);
    app->app_dir  = dxl_path_dirname(app->exe_path);
    app->cmdline  = dxl_cmdline_join(argc, argv);

    /* --- launcher.ini ------------------------------------------------- */
    app->launcher_ini_path = dxl_path_join(app->app_dir, LAUNCHER_INI);
    dxl_ini *l = dxl_ini_load(app->launcher_ini_path, NULL);

    app->package      = dup_or(l ? dxl_ini_get(l, "Launcher", "Package") : NULL, "DeusEx");
    app->game_command = dup_or(l ? dxl_ini_get(l, "Launcher", "GameCommand") : NULL,
                               "./run-game.sh");
    {
        const char *gd = l ? dxl_ini_get(l, "Launcher", "GameDir") : NULL;
        /* Default to a "game" directory beside the launcher, which is what
         * the install screen tells the user to fill. */
        app->game_dir = (gd && *gd) ? dxl_path_from_ini(gd)
                                    : dxl_path_join(app->app_dir, "game");
    }
    char *es_path  = app_path(app, l, "EngineSettings", DEFAULT_ENGINE_SETTINGS);
    char *es_def   = app_path(app, l, "EngineSettingsDefault", DEFAULT_ENGINE_DEFAULTS);
    app->engine_log = app_path(app, l, "EngineLog", DEFAULT_ENGINE_LOG);
    app->run_log    = app_path(app, l, "RunLog", DEFAULT_RUN_LOG);
    /* Kept: the launcher writes its own settings (CpuMode) back to it. */
    app->launcher_ini = l;

    /* --- install ------------------------------------------------------ */
    dxl_install_probe(app->game_dir, &app->install);
    app->system_dir = dxl_xstrdup(app->install.system_dir);

    /* Log next to the game's own, where anyone debugging would look. */
    char *logp = dxl_path_join(app->system_dir, "DeusExLauncher.log");
    if (dxl_path_is_dir(app->system_dir)) dxl_log_open(logp);
    free(logp);
    dxl_log("launcher starting: exe=%s", app->exe_path);
    dxl_log("game dir: %s (%s)", app->game_dir,
            app->install.ok ? "complete" : "INCOMPLETE");

    /* --- config ------------------------------------------------------- */
    app->cfg = dxl_config_open(app->system_dir, app->package);
    switch (dxl_config_seeded(app->cfg)) {
    case 1: dxl_log("%s missing; created from Default.ini", dxl_config_path(app->cfg)); break;
    case 2: dxl_log("%s had no [Core.System] Paths; rebuilt from Default.ini",
                    dxl_config_path(app->cfg)); break;
    default: break;
    }
    dxl_log("engine reads client settings from %s [%s]",
            dxl_config_client_path(app->cfg), dxl_config_client_section(app->cfg));

    app->es = dxl_es_open(es_path, es_def);
    if (dxl_es_was_corrupt(app->es))
        dxl_log("warning: %s was unreadable; it will be rewritten", es_path);
    free(es_path);
    free(es_def);

    char *rpath = dxl_path_join(app->app_dir, RENDERERS_INI);
    dxl_renderers_load(&app->renderers, rpath);
    free(rpath);
    dxl_renderers_resolve(&app->renderers, &app->gpu);
    dxl_log("renderers declared: %zu", app->renderers.count);

    if (app->install.ok) apply_first_contact_layout(app);

    dxl_sentinel_init(&app->sentinel, app->system_dir);

    /* --- single instance ---------------------------------------------- */
    int other = dxl_instance_other_running(app->game_dir);

    /* --- the decision -------------------------------------------------- */
    dxl_policy_input in = {
        .cmdline            = app->cmdline,
        .first_run          = dxl_config_first_run(app->cfg),
        .other_instance     = other,
        .running_ini_exists = dxl_sentinel_exists(&app->sentinel),
        .is_client          = !dxl_cmd_param(app->cmdline, "server"),
    };
    dxl_policy_decide(&in, &app->decision);
    dxl_log("decision: action=%s screen=%s firstrun=%d other=%d sentinel=%d",
            dxl_action_name(app->decision.action),
            dxl_screen_name(app->decision.screen),
            app->decision.effective_first_run, other, in.running_ini_exists);

    /* Hold the lock for the rest of the session unless we are the messenger. */
    if (app->decision.action != DXL_ACTION_FORWARD)
        app->instance = dxl_instance_acquire(app->game_dir);

    (void)err;
    return 0;
}

void dxl_app_shutdown(dxl_app *app) {
    if (!app) return;
    if (app->instance) dxl_instance_release(app->instance);
    dxl_renderers_free(&app->renderers);
    dxl_install_free(&app->install);
    dxl_sentinel_free(&app->sentinel);
    dxl_es_free(app->es);
    dxl_config_free(app->cfg);
    dxl_ini_free(app->launcher_ini);
    free(app->launcher_ini_path);
    free(app->app_dir); free(app->exe_path); free(app->game_dir);
    free(app->system_dir); free(app->package); free(app->game_command);
    free(app->engine_log); free(app->run_log);
    free(app->cmdline);
    dxl_log_close();
    memset(app, 0, sizeof *app);
}

void dxl_app_probe_gpu(dxl_app *app) {
    if (dxl_gpu_probe_run(&app->gpu, GPU_PROBE_TIMEOUT_MS) != 0)
        dxl_log("gpu probe: %s", app->gpu.note);
    else
        dxl_log("gpu probe: vulkan=%d (%s %s) gles=%d (%s | %s) gl=%d",
                app->gpu.vulkan, app->gpu.vulkan_device, app->gpu.vulkan_version,
                app->gpu.gles, app->gpu.gles_renderer, app->gpu.gles_version,
                app->gpu.gl);
    dxl_renderers_resolve(&app->renderers, &app->gpu);
}

int dxl_app_needs_install(const dxl_app *app) { return !app->install.ok; }

void dxl_app_recheck_install(dxl_app *app) {
    dxl_install_free(&app->install);
    dxl_install_probe(app->game_dir, &app->install);
    dxl_log("install re-probed: %s", app->install.ok ? "complete" : "still incomplete");
    if (!app->install.ok) return;

    /* The files arrived while we were running: everything read from the
     * System directory has to be read again. */
    free(app->system_dir);
    app->system_dir = dxl_xstrdup(app->install.system_dir);
    dxl_config_free(app->cfg);
    app->cfg = dxl_config_open(app->system_dir, app->package);
    dxl_sentinel_free(&app->sentinel);
    dxl_sentinel_init(&app->sentinel, app->system_dir);
    apply_first_contact_layout(app);
}

int dxl_app_current_renderer(const dxl_app *app) {
    return dxl_renderers_find(&app->renderers, dxl_es_choice(app->es, DXL_ES_RENDER_TYPE));
}

int dxl_app_choose_renderer(dxl_app *app, int index) {
    if (index < 0 || (size_t)index >= app->renderers.count) return -1;
    const dxl_renderer *r = &app->renderers.items[index];
    if (!r->selectable) return -1;
    if (dxl_es_set_choice(app->es, DXL_ES_RENDER_TYPE, r->engine_type) != 0) return -1;
    dxl_log("renderer: %s", r->engine_type);
    return 0;
}

void dxl_app_choose_layout(dxl_app *app, const dxl_pad_preset *p) {
    dxl_ini *user = dxl_config_user_ini(app->cfg);
    if (!user || !p) return;
    dxl_bindings_apply(user, p);
    dxl_es_set_choice(app->es, DXL_ES_PAD_LAYOUT, p->id);
}

const dxl_pad_preset *dxl_app_current_layout(dxl_app *app) {
    dxl_ini *user = dxl_config_user_ini(app->cfg);
    return user ? dxl_bindings_detect(user) : NULL;
}

const dxl_pad_preset *dxl_app_base_layout(dxl_app *app) {
    const char *id = dxl_es_choice(app->es, DXL_ES_PAD_LAYOUT);
    for (size_t i = 0; i < dxl_pad_preset_count(); i++)
        if (strcmp(dxl_pad_preset_at(i)->id, id) == 0) return dxl_pad_preset_at(i);
    return dxl_pad_preset_default();
}

void dxl_app_bind(dxl_app *app, const char *joy_key, const char *command) {
    dxl_ini *user = dxl_config_user_ini(app->cfg);
    if (!user) return;
    dxl_bindings_set(user, joy_key, command);
    /* Gamepad.Layout keeps naming the preset the player started from, so X
     * on a button can still put back that preset's binding. */
    dxl_log("pad: %s -> %s", joy_key, command && *command ? command : "(nothing)");
}

const char *dxl_app_cpu_mode(const dxl_app *app) {
    const char *v = app->launcher_ini ? dxl_ini_get(app->launcher_ini, "Launcher", "CpuMode") : NULL;
    const dxl_cpu_mode_info *m = dxl_target_cpu_mode(dxl_target_get(), v);
    return m ? m->name : NULL;
}

void dxl_app_set_cpu_mode(dxl_app *app, const char *mode) {
    if (!dxl_app_cpu_mode(app)) return;   /* the device offers none */
    if (!app->launcher_ini) app->launcher_ini = dxl_ini_new();
    if (strcmp(dxl_app_cpu_mode(app), mode) == 0 &&
        dxl_ini_get(app->launcher_ini, "Launcher", "CpuMode")) return;
    dxl_ini_set(app->launcher_ini, "Launcher", "CpuMode", mode);
    dxl_log("cpu mode: %s", mode);
}

int dxl_app_reset_game_config(dxl_app *app, dxl_err *err) {
    int first_run = dxl_config_first_run(app->cfg);
    if (dxl_config_reset_files(app->system_dir, app->package, err) != 0) return -1;
    dxl_log("game configuration deleted; rebuilding from Default.ini and DefUser.ini");

    dxl_config_free(app->cfg);
    app->cfg = dxl_config_open(app->system_dir, app->package);
    /* Resetting settings is not "this is a new install": keep the gate where
     * it was, so the next launch does not treat saves as needing migration. */
    if (first_run > dxl_config_first_run(app->cfg))
        dxl_ini_set_int(dxl_config_ini(app->cfg), "FirstRun", "FirstRun", first_run);
    /* The rebuilt User.ini has the shipped pad bindings again. */
    const dxl_pad_preset *p = dxl_pad_preset_default();
    dxl_app_choose_layout(app, p);
    return dxl_config_save(app->cfg, err);
}

/* If Settings.json names a renderer that cannot run here, the engine would
 * fail to start (or silently fall back to Vulkan). Pick one that can, but
 * only on evidence: without a probe nothing is known to be unavailable. */
static void settle_renderer(dxl_app *app) {
    if (!app->gpu.probed || app->renderers.count == 0) return;
    int cur = dxl_app_current_renderer(app);
    if (cur >= 0 && app->renderers.items[cur].selectable) return;
    int alt = dxl_renderers_first_selectable(&app->renderers);
    if (alt < 0) return;
    dxl_log("renderer '%s' cannot run here; using %s",
            dxl_es_choice(app->es, DXL_ES_RENDER_TYPE),
            app->renderers.items[alt].engine_type);
    dxl_app_choose_renderer(app, alt);
}

/* A GPU whose multisample resolve is broken (dxl_gpu_msaa_broken): the Video
 * tab locks the row, and this catches a file edited by hand or carried over
 * from another device. */
static void settle_device_limits(dxl_app *app) {
    if (!dxl_gpu_msaa_broken(&app->gpu)) return;
    if (strcmp(dxl_es_choice(app->es, DXL_ES_ANTIALIAS), "Off") == 0) return;
    dxl_log("anti-aliasing %s turned off: not usable on %s",
            dxl_es_choice(app->es, DXL_ES_ANTIALIAS), app->gpu.vulkan_device);
    dxl_es_set_choice(app->es, DXL_ES_ANTIALIAS, "Off");
}

int dxl_app_save_settings(dxl_app *app, dxl_err *err) {
    settle_renderer(app);
    settle_device_limits(app);
    if (dxl_config_dirty(app->cfg) && dxl_config_save(app->cfg, err) != 0) {
        dxl_log("error: %s", dxl_err_msg(err));
        return -1;
    }
    if (dxl_es_dirty(app->es) && dxl_es_save(app->es, err) != 0) {
        dxl_log("error: %s", dxl_err_msg(err));
        return -1;
    }
    if (app->launcher_ini && dxl_ini_dirty(app->launcher_ini) &&
        dxl_ini_save(app->launcher_ini, app->launcher_ini_path, err) != 0) {
        dxl_log("error: %s", dxl_err_msg(err));
        return -1;
    }
    return 0;
}

int dxl_app_commit(dxl_app *app, dxl_err *err) {
    /* Order is the original's (dx-reverse-info/launch-flow.md section 5, steps 9-11):
     * sentinel first, then the FirstRun clamp, then the config write. */
    if (dxl_sentinel_create(&app->sentinel, err) != 0) {
        /* A game that will not start because a marker file failed is the
         * worse outcome than losing crash detection for one run. */
        dxl_log("warning: %s", dxl_err_msg(err));
    }
    dxl_config_clamp_first_run(app->cfg);

    if (dxl_app_save_settings(app, err) != 0) return -1;

    /* CdPath is almost always "..\", which resolves back to the game dir; the
     * check is kept for installs that still point at a disc. */
    if (!dxl_install_cd_ok(app->game_dir, dxl_config_cd_path(app->cfg)))
        dxl_log("warning: CdPath check failed for '%s'",
                dxl_config_cd_path(app->cfg) ? dxl_config_cd_path(app->cfg) : "");
    return 0;
}

/* The game command is resolved relative to the launcher directory when it is
 * not absolute, so launcher.ini can say "./run-game.sh". */
static char *resolve_command(const dxl_app *app) {
    if (app->game_command[0] == '/') return dxl_xstrdup(app->game_command);
    const char *c = app->game_command;
    if (c[0] == '.' && c[1] == '/') c += 2;
    return dxl_path_join(app->app_dir, c);
}

int dxl_app_launch(dxl_app *app, dxl_err *err) {
    char *cmd = resolve_command(app);
    dxl_log("launching: %s %s", cmd, app->cmdline);

    /* The sentinel must survive into the game: it is deleted on clean exit by
     * whoever owns the run. Release the lock first so the game can take it. */
    if (app->instance) { dxl_instance_release(app->instance); app->instance = NULL; }
    dxl_log_close();

    int rc = dxl_platform_launch(cmd, app->cmdline, app->game_dir, err);
    free(cmd);
    return rc;   /* only here on failure */
}

void dxl_app_dry_run(dxl_app *app) {
    const dxl_decision *d = &app->decision;

    printf("launcher\n");
    printf("  exe            %s\n", app->exe_path);
    printf("  app dir        %s\n", app->app_dir);
    printf("  game dir       %s\n", app->game_dir);
    printf("  system dir     %s\n", app->system_dir);
    printf("  package        %s\n", app->package);
    printf("  game command   %s\n", app->game_command);
    printf("  command line   \"%s\"\n", app->cmdline);

    printf("\ninstall (%s)\n", app->install.ok ? "complete" : "INCOMPLETE");
    for (int i = 0; i < app->install.item_count; i++) {
        const dxl_install_item *it = &app->install.items[i];
        printf("  [%s] %-24s %s\n", it->found ? "ok" : "--",
               it->relative, it->found ? it->resolved : it->label);
    }

    printf("\ndecision\n");
    printf("  action         %s\n", dxl_action_name(d->action));
    printf("  screen         %s\n", dxl_screen_name(d->screen));
    printf("  first run      %d (effective)\n", d->effective_first_run);
    printf("  migrate saves  %s\n", d->migrate_saves ? "yes" : "no");
    printf("  skip handoff   %s\n", d->skip_handoff ? "yes" : "no");
    if (d->console_command[0]) printf("  console cmd    %s\n", d->console_command);
    if (d->test_rendev[0])     printf("  test rendev    %s\n", d->test_rendev);
    if (d->exec_file[0])       printf("  exec file      %s\n", d->exec_file);

    static const char *seeded[] = { "as found", "created from Default.ini",
                                    "rebuilt from Default.ini (had no [Core.System] Paths)" };
    printf("\ngame config\n");
    printf("  %-18s %s (%s)\n", "system ini", dxl_config_path(app->cfg),
           seeded[dxl_config_seeded(app->cfg)]);
    printf("  %-18s %s [%s]\n", "client settings", dxl_config_client_path(app->cfg),
           dxl_config_client_section(app->cfg));
    printf("  %-18s %s\n", "bindings", dxl_config_user_path(app->cfg)
                                       ? dxl_config_user_path(app->cfg) : "-");
    printf("  %-18s %d\n", "FirstRun", dxl_config_first_run(app->cfg));
    printf("  %-18s %.2f\n", "Brightness", dxl_config_brightness(app->cfg));
    printf("  %-18s %s\n", "Decals", dxl_config_decals(app->cfg) ? "on" : "off");
    const dxl_pad_preset *pp = dxl_app_current_layout(app);
    printf("  %-18s %s%s\n", "pad layout", pp ? pp->label : "custom",
           app->pad_layout_applied ? " (applied now; shipped bindings were in place)" : "");
    if (dxl_app_cpu_mode(app)) printf("  %-18s %s\n", "CPU mode", dxl_app_cpu_mode(app));
    printf("  %-18s %s\n", "Running.ini",
           dxl_sentinel_exists(&app->sentinel) ? "present (crash pending)" : "absent");

    printf("\nengine settings (%s%s)\n", dxl_es_path(app->es),
           !dxl_es_existed(app->es) ? ", not yet written"
           : dxl_es_was_corrupt(app->es) ? ", UNREADABLE -- will be replaced" : "");
    for (int f = 0; f < DXL_ES_FIELD_COUNT; f++) {
        const dxl_es_info *in = dxl_es_describe((dxl_es_field)f);
        printf("  %-12s %-24s ", in->section, in->key);
        switch (in->kind) {
        case DXL_ES_CHOICE: printf("%s\n", dxl_es_choice(app->es, (dxl_es_field)f)); break;
        case DXL_ES_BOOL:   printf("%s\n", dxl_es_bool(app->es, (dxl_es_field)f) ? "true" : "false"); break;
        case DXL_ES_NUMBER: printf("%g\n", dxl_es_number(app->es, (dxl_es_field)f)); break;
        }
    }

    printf("\nrenderers (%s)\n", app->gpu.probed ? "device probed"
                                 : app->gpu.note[0] ? app->gpu.note
                                 : "device not probed -- add --probe");
    int cur = dxl_app_current_renderer(app);
    for (size_t i = 0; i < app->renderers.count; i++) {
        const dxl_renderer *r = &app->renderers.items[i];
        printf("  %c %-10s %-11s %s\n", (int)i == cur ? '*' : ' ', r->label,
               r->selectable ? "selectable" : "unavailable", r->status);
    }

    printf("\nnothing was written.\n");
}
