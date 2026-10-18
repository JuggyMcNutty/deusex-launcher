#define _GNU_SOURCE
#include "app/run.h"
#include "core/argv.h"
#include "core/cmdline.h"
#include "core/engine_args.h"
#include "core/engine_sections.h"
#include "core/padmap.h"
#include "core/log.h"
#include "core/servers.h"
#include "core/paths.h"
#include "platform/process.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef __GLIBC__
#include <gnu/libc-version.h>
#endif

#define HANDOFF_TIMEOUT_MS 30000   /* the original's SendMessageTimeout */
#define VERSION_TIMEOUT_MS 5000
#define PACKAGE DXL_GAME_PACKAGE

/* ---- the command line --------------------------------------------- */

static int take(const char *arg, const char *name, const char **value) {
    size_t n = strlen(name);
    if (strncmp(arg, name, n) != 0) return 0;
    if (arg[n] == '=') { *value = arg + n + 1; return 1; }
    return 0;
}

int dxl_run_parse(int argc, char **argv, dxl_run_options *opt, char **words, dxl_err *err) {
    memset(opt, 0, sizeof *opt);
    char **rest = dxl_xmalloc(((size_t)argc + 1) * sizeof *rest);
    int n = 0;
    rest[n++] = argc > 0 ? argv[0] : "deusex-launcher";
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        /* A link's server is the launcher's to join, not a word of the
         * original's. */
        char link[256];
        if (dxl_server_link(a, link, sizeof link)) { opt->join = a; continue; }
        if (strncmp(a, "--", 2) != 0) { rest[n++] = argv[i]; continue; }
        if (take(a, "--game", &opt->game) || take(a, "--profile", &opt->profile) || take(a, "--mod", &opt->mod))
            continue;
        if (strcmp(a, "--play") == 0) { opt->play = 1; continue; }
        if (strcmp(a, "--dry-run") == 0) { opt->dry = 1; continue; }
        if (strcmp(a, "--print-command") == 0) { opt->print_command = 1; continue; }
        if (strcmp(a, "--import") == 0) { opt->import_only = 1; continue; }
        if (strcmp(a, "--big-screen") == 0) { opt->big_screen = 1; continue; }
        dxl_err_set(err, "no option %s", a);
        free(rest);
        return -1;
    }
    rest[n] = NULL;
    *words = dxl_cmdline_join(n, rest);
    free(rest);
    return 0;
}

/* ---- the report of a stop ------------------------------------------ */

static void stop_with(dxl_run *r, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->message, sizeof r->message, fmt, ap);
    va_end(ap);
    dxl_log("%s", r->message);
}

/* What a screen with no window says instead: the last run's end on
 * Recovery, what it wants elsewhere. */
static void say_view(dxl_run *r, dxl_view v) {
    char line[600];
    switch (v) {
    case DXL_VIEW_RECOVERY:
        if (r->last.known) {
            dxl_lastrun_describe(&r->last, line, sizeof line);
            fprintf(stderr, "The last run of profile %s did not end cleanly: %s.\n",
                    r->session.profile.id, line);
        } else {
            fprintf(stderr, "Safe mode: the Recovery screen is the launcher's window's.\n");
        }
        char *log = dxl_path_join(r->session.profile.logs_dir, DXL_LASTRUN_LOG);
        char *tail = dxl_lastrun_tail(log, 8);
        if (tail) fprintf(stderr, "The end of its log, %s:\n%s\n", log, tail);
        free(tail);
        free(log);
        fprintf(stderr, "Its Running.ini is %s: remove it to play again without the Recovery screen.\n",
                dxl_sentinel_path(&r->sentinel));
        break;
    case DXL_VIEW_GRAPHICS:
        fprintf(stderr, "-changevideo: the Graphics screen is the launcher's window's.\n");
        break;
    case DXL_VIEW_FIRST_RUN:
        fprintf(stderr, "No profile yet: give the game's folder, --game=<dir>.\n");
        break;
    default:
        break;
    }
}

/* ---- the engine ------------------------------------------------------ */

/* The version a loader's error says the program needs of a library
 * ("version `GLIBC_2.44' not found"): its name and number into what. */
static int needs_version(const char *errs, char *what, size_t size) {
    const char *v = strstr(errs, "version `");
    if (!v) return 0;
    v += 9;
    size_t n = strcspn(v, "'\n");
    if (!n || v[n] != '\'') return 0;
    snprintf(what, size, "%.*s", (int)n, v);
    return 1;
}

/* The library a loader's error says the program cannot find. */
static int needs_library(const char *errs, char *what, size_t size) {
    const char *l = strstr(errs, "error while loading shared libraries: ");
    if (!l) return 0;
    l += strlen("error while loading shared libraries: ");
    size_t n = strcspn(l, ":\n");
    if (!n) return 0;
    snprintf(what, size, "%.*s", (int)n, l);
    return 1;
}

int dxl_engine_check(const char *engine, dxl_engine_version *v, char *brief, size_t brief_size, char *why,
                     size_t size) {
    memset(v, 0, sizeof *v);
    if (!engine || !dxl_path_exists(engine)) {
        snprintf(brief, brief_size, "not there");
        snprintf(why, size, "There is no engine at %s.", engine ? engine : "(none)");
        return -1;
    }
    char *argv[] = { (char *)engine, "--version", NULL };
    char out[1024], errs[1024];
    int status = 0;
    dxl_err e;
    int ran = dxl_run_capture(argv, VERSION_TIMEOUT_MS, out, sizeof out, errs, sizeof errs, &status, &e) == 0;
    if (!ran && status == 128 + SIGKILL && !errs[0]) {
        snprintf(brief, brief_size, "gives no answer");
        snprintf(why, size, "%s gave no answer to --version in %d seconds: refusing it.", engine,
                 VERSION_TIMEOUT_MS / 1000);
        return -1;
    }
    if (dxl_engine_version_parse(out, v) != 0) {
        char need[96], line[300];
        snprintf(line, sizeof line, "%.*s", (int)strcspn(errs, "\n"), errs);
        if (needs_version(errs, need, sizeof need) && !strncmp(need, "GLIBC_", 6)) {
#ifdef __GLIBC__
            const char *here = gnu_get_libc_version();
#else
            const char *here = "an older one";
#endif
            snprintf(brief, brief_size, "needs glibc %s, this system has %s", need + 6, here);
            snprintf(why, size, "%s does not start here: it needs glibc %s, and this system has %s. It was built on "
                     "a newer system than this one: choose an engine built for this one, or build it here.",
                     engine, need + 6, here);
        } else if (needs_version(errs, need, sizeof need)) {
            snprintf(brief, brief_size, "needs %s", need);
            snprintf(why, size, "%s does not start here: it needs %s of a library, which this system lacks (%s).",
                     engine, need, line);
        } else if (needs_library(errs, need, sizeof need)) {
            snprintf(brief, brief_size, "needs %s", need);
            snprintf(why, size, "%s does not start here: it needs %s, which this system lacks.", engine, need);
        } else if (status != 0 || line[0]) {
            snprintf(brief, brief_size, "does not start");
            snprintf(why, size, "%s does not start here (its exit %d)%s%s", engine, status, line[0] ? ": " : ".",
                     line);
        } else {
            snprintf(brief, brief_size, "not VibeEngine");
            snprintf(why, size, "%s does not answer --version as VibeEngine does: refusing it.", engine);
        }
        return -1;
    }
    if (!dxl_engine_has(v, DXL_ENGINE_NEEDS)) {
        snprintf(brief, brief_size, "%s, without --%s", v->version, DXL_ENGINE_NEEDS);
        snprintf(why, size, "%s (%s) lacks --%s: refusing it, so that nothing writes ~/.config/SurrealEngine.",
                 engine, v->version, DXL_ENGINE_NEEDS);
        return -1;
    }
    return 0;
}

int dxl_run_engine_check(const dxl_session *s, dxl_engine_version *v, char *brief, size_t brief_size, char *why,
                         size_t size) {
    return dxl_engine_check(s->engine, v, brief, brief_size, why, size);
}

char *dxl_engine_in(const char *dir) {
    static const char *const names[] = { "VibeEngine", "SurrealEngine" };
    for (size_t i = 0; dir && i < sizeof names / sizeof *names; i++) {
        char *p = dxl_path_join(dir, names[i]);
        if (dxl_path_exists(p) && !dxl_path_is_dir(p) && access(p, X_OK) == 0) return p;
        free(p);
    }
    return NULL;
}

int dxl_engine_pk3_found(const char *path) {
    char *dir = dxl_path_dirname(path);
    char *beside = dxl_path_join(dir, "SurrealEngine.pk3");
    int found = dxl_path_exists(beside);
    free(beside);
    free(dir);
    if (found || dxl_path_exists("/usr/share/surrealengine/SurrealEngine.pk3")) return 1;
    const char *home = getenv("HOME"), *data = getenv("XDG_DATA_HOME");
    char p[2048];
    if (data && *data) snprintf(p, sizeof p, "%s/surrealengine/SurrealEngine.pk3", data);
    else snprintf(p, sizeof p, "%s/.local/share/surrealengine/SurrealEngine.pk3", home ? home : "");
    if (dxl_path_exists(p)) return 1;
    snprintf(p, sizeof p, "%s/.surrealengine/SurrealEngine.pk3", home ? home : "");
    return dxl_path_exists(p);
}

/* The engine's --version, before the game: refused without the userdir
 * feature. */
static int check_engine(dxl_run *r, dxl_engine_version *v) {
    char why[700], brief[160];
    if (dxl_run_engine_check(&r->session, v, brief, sizeof brief, why, sizeof why) != 0) {
        stop_with(r, "%s", why);
        return -1;
    }
    dxl_log("engine: %s, %s; features: %s", r->session.engine, v->version, v->features);
    return 0;
}

/* The words and the environment the engine is started with: the
 * profile's extra words (profile.ini's EngineArgs), then
 * DXL_ENGINE_ARGS's. */
static char **engine_argv(const dxl_run *r) {
    const dxl_profile *p = &r->session.profile;
    const char *mine = dxl_profile_get(p, "EngineArgs"), *env = getenv("DXL_ENGINE_ARGS");
    char extra[2048];
    snprintf(extra, sizeof extra, "%s%s%s", mine ? mine : "", mine && *mine && env && *env ? " " : "",
             env ? env : "");
    dxl_engine_command c = {
        .engine = r->session.engine,
        .game = r->session.install.root,
        .ini = p->system_ini,
        .user_ini = p->user_ini,
        .user_dir = p->engine_dir,
        .url = r->url[0] ? r->url : NULL,
        .extra = extra,
        .cmdline = r->words,
    };
    return dxl_engine_argv(&c);
}

/* The launcher's pad mappings (core/padmap.h): the controllers the mapping
 * wizard made known, the engine's too. */
char *dxl_run_pad_db(const dxl_run *r) { return dxl_path_join(r->session.xdg.data, "gamecontrollerdb.txt"); }

/* SDL3 for the engine's window, the backend its build has, unless the
 * player named one (the fork's ENGINE.md, settings and environment); and
 * the pad mappings, Steam's and the launcher's, when the launcher keeps any. */
static char **engine_env(const dxl_run *r) {
    static char *env[3], *pads;
    int n = 0;
    if (!getenv("SURREALWIDGETS_DISPLAY_BACKEND")) env[n++] = "SURREALWIDGETS_DISPLAY_BACKEND=SDL3";
    char *db = dxl_run_pad_db(r);
    char *config = dxl_padmap_engine_config(getenv("SDL_GAMECONTROLLERCONFIG"), db);
    free(db);
    free(pads);
    pads = NULL;
    if (config) {
        size_t len = strlen(config) + sizeof "SDL_GAMECONTROLLERCONFIG=";
        pads = dxl_xmalloc(len);
        snprintf(pads, len, "SDL_GAMECONTROLLERCONFIG=%s", config);
        free(config);
        env[n++] = pads;
    }
    env[n] = NULL;
    return env;
}

static void print_command(const dxl_run *r) {
    char **argv = engine_argv(r);
    char *line = dxl_argv_quoted(argv);
    char **env = engine_env(r);
    char *cwd[] = { r->session.install.system_dir, NULL };
    char *dir = dxl_argv_quoted(cwd);
    /* Each variable NAME=value, its value quoted: a quoted name is no
     * assignment to a shell. */
    dxl_buf vars;
    dxl_buf_init(&vars);
    for (int i = 0; env[i]; i++) {
        const char *eq = strchr(env[i], '=');
        char *value[] = { (char *)(eq ? eq + 1 : ""), NULL };
        char *quoted = dxl_argv_quoted(value);
        dxl_buf_printf(&vars, "%.*s=%s ", (int)(eq ? eq - env[i] : 0), env[i], quoted);
        free(quoted);
    }
    printf("cd %s && %s%s\n", dir, vars.data ? vars.data : "", line);
    dxl_buf_free(&vars);
    printf("# and DXL_LAUNCHER_FD, the launcher's line; output into %s/%s\n",
           r->session.profile.logs_dir, DXL_LASTRUN_LOG);
    free(dir);
    free(line);
    dxl_argv_free(argv);
}

/* ---- the dry run ------------------------------------------------------ */

static const char *view_name(dxl_view v) {
    switch (v) {
    case DXL_VIEW_NONE: return "none: play";
    case DXL_VIEW_HOME: return "Home";
    case DXL_VIEW_FIRST_RUN: return "First run";
    case DXL_VIEW_INSTALL: return "the profile's install";
    case DXL_VIEW_RECOVERY: return "Recovery";
    case DXL_VIEW_GRAPHICS: return "Graphics";
    }
    return "?";
}

static void dry_run(dxl_run *r) {
    dxl_session *s = &r->session;
    dxl_profile *p = &s->profile;
    printf("launcher\n");
    printf("  app folder     %s\n", s->app_dir);
    printf("  engine         %s\n", s->engine);
    printf("  command line   \"%s\"\n", r->words);
    printf("  launcher.ini   %s%s\n", s->ini_path, dxl_path_exists(s->ini_path) ? "" : " (not yet written)");
    printf("\nprofile %s (%s)\n", p->id, dxl_profile_exists(p) ? "made" : "not made yet");
    printf("  inis           %s, %s\n", p->system_ini, p->user_ini);
    const char *mod = dxl_profile_get(p, "Mod");
    if (mod && *mod) printf("  mod            %s\n", mod);
    printf("  engine folder  %s\n", p->engine_dir);
    printf("  saves, shots   %s, %s\n", p->save_dir, p->shots_dir);
    printf("  runs           %s\n", p->state_dir);
    printf("\ninstall\n");
    if (s->install.root) {
        printf("  folder         %s\n", s->install.root);
        printf("  build          %s%s\n", s->install.build ? s->install.build->version : "unknown",
               s->install.build ? "" : " (the engine does not play it)");
        if (s->install.problem[0]) printf("  problem        %s\n", s->install.problem);
    } else {
        printf("  none yet%s\n", r->opt.game ? "" : ": --game=<dir> names one");
    }
    if (!dxl_profile_exists(p) && s->install.root) {
        dxl_import im;
        dxl_err e;
        if (dxl_profile_import(p, s->install.system_dir, 1, &im, &e) == 0) {
            printf("\nthe first run would make the profile from\n");
            printf("  DeusEx.ini     %s\n", im.system_from);
            printf("  User.ini       %s\n", im.user_from);
            if (im.engine_from[0]) printf("  engine's own   %s\n", im.engine_from);
            printf("  saves          %d from %s (never Current)\n", im.saves, im.saves_from);
            printf("  shots          %d\n", im.shots);
            if (im.master_replaced) printf("  master server  GameSpy's, closed: 333networks' in its place\n");
        }
    }
    printf("\ndecision\n");
    printf("  action         %s, %s\n", dxl_action_name(r->decision.action), dxl_screen_name(r->decision.screen));
    printf("  screen         %s\n", view_name(r->view));
    printf("  first run      %d (effective)\n", r->decision.effective_first_run);
    printf("  Running.ini    %s\n", dxl_sentinel_exists(&r->sentinel) ? "present: the last run did not end cleanly" : "absent");
    if (r->last.known) {
        char line[600];
        dxl_lastrun_describe(&r->last, line, sizeof line);
        printf("  last run       %s\n", line);
    }
    if (s->install.root && s->install.ok) {
        printf("\nthe engine's command\n  ");
        fflush(stdout);
        print_command(r);
    }
    printf("\nnothing was written.\n");
}

/* ---- the profile ---------------------------------------------------- */

int dxl_run_use_profile(dxl_run *r, const char *id, dxl_err *err) {
    if (dxl_session_use_profile(&r->session, id, err) != 0) return -1;
    dxl_config_free(r->config);
    r->config = NULL;
    if (r->sentinel.path) dxl_sentinel_free(&r->sentinel);
    dxl_sentinel_init(&r->sentinel, r->session.profile.state_dir);
    memset(&r->last, 0, sizeof r->last);
    dxl_lastrun_load(&r->last, r->session.profile.state_dir);
    if (dxl_profile_exists(&r->session.profile))
        r->config = dxl_config_open_files(r->session.install.system_dir ? r->session.install.system_dir : ".",
                                          PACKAGE, r->session.profile.system_ini, r->session.profile.user_ini);
    return 0;
}

/* ---- the run ---------------------------------------------------------- */

static void open_log(dxl_run *r) {
    if (r->opt.dry || r->opt.print_command) {
        dxl_log_set_to_file(0);
        dxl_log_set_echo(0);
        return;
    }
    dxl_path_mkdirs(r->session.xdg.state);
    char *path = dxl_path_join(r->session.xdg.state, "launcher.log");
    dxl_log_open_new(path);
    free(path);
}

/* The first run: a profile made by the first run's screens, where --game's
 * install is the first one offered; with no window, from --game's install.
 * 0 to go on; -1 when the player quit there; else the exit code to stop
 * with. */
static int first_run(dxl_run *r, const dxl_run_ui *ui) {
    if (ui && ui->view && !r->opt.import_only) {
        if (!ui->view(ui->ctx, r, DXL_VIEW_FIRST_RUN)) return -1;
        if (dxl_profile_exists(&r->session.profile)) return 0;
        stop_with(r, "the first run made no profile");
        return DXL_RUN_EXIT_NOGAME;
    }
    if (!r->opt.game) {
        say_view(r, DXL_VIEW_FIRST_RUN);
        dxl_log("no profile yet and no --game");
        return DXL_RUN_EXIT_SCREEN;
    }
    dxl_install in;
    if (dxl_install_check(r->opt.game, &in) != 0) {
        stop_with(r, "cannot make a profile from %s: %s", in.root, in.problem);
        dxl_install_free(&in);
        return DXL_RUN_EXIT_NOGAME;
    }
    if (in.problem[0]) dxl_log("warning: %s", in.problem);
    dxl_import report;
    dxl_err e;
    dxl_mod mod;
    if (r->opt.mod && dxl_mod_check(r->opt.mod, &mod, &e) != 0) {
        stop_with(r, "cannot make a mod's profile: %s", dxl_err_msg(&e));
        dxl_install_free(&in);
        return DXL_RUN_EXIT_NOGAME;
    }
    int rc = dxl_session_make_profile(&r->session, &in, r->opt.mod ? &mod : NULL, NULL, &report, &e);
    if (r->opt.mod) dxl_mod_free(&mod);
    dxl_install_free(&in);
    if (rc != 0) {
        stop_with(r, "cannot make the profile: %s", dxl_err_msg(&e));
        return DXL_RUN_EXIT_NOGAME;
    }
    return 0;
}

/* --game on a profile that has one: the profile's install is that one now
 * (the game moved, or another copy of it). */
static int repoint(dxl_run *r) {
    dxl_install in;
    if (dxl_install_check(r->opt.game, &in) != 0) {
        stop_with(r, "%s is not an install to play: %s", in.root, in.problem);
        dxl_install_free(&in);
        return DXL_RUN_EXIT_NOGAME;
    }
    char id[96];
    dxl_session_remember_install(&r->session, &in, id, sizeof id);
    const char *was = dxl_profile_get(&r->session.profile, "Install");
    if (!was || strcmp(was, id) != 0) {
        dxl_profile_set(&r->session.profile, "Install", id);
        dxl_log("profile %s: install %s (%s)", r->session.profile.id, id, in.root);
    }
    dxl_install_free(&in);
    return 0;
}

/* Home, unless --play or DXL_NO_HOME=1 asked for none. */
static int home_wanted(const dxl_run *r) {
    const char *no_home = getenv("DXL_NO_HOME");
    return !r->opt.play && !(no_home && strcmp(no_home, "1") == 0);
}

static dxl_view view_of(const dxl_run *r) {
    switch (r->decision.screen) {
    case DXL_SCREEN_MAIN_SAFE:
    case DXL_SCREEN_MAIN_RECOVERY:  return DXL_VIEW_RECOVERY;
    case DXL_SCREEN_RENDERER_VIDEO: return DXL_VIEW_GRAPHICS;
    /* The original's first-time renderer page: the first run here is the
     * launcher's own, done by now. */
    case DXL_SCREEN_RENDERER_FIRST:
    case DXL_SCREEN_NONE:           break;
    }
    if (r->decision.action != DXL_ACTION_LAUNCH && r->decision.action != DXL_ACTION_SCREEN)
        return DXL_VIEW_NONE;
    return home_wanted(r) ? DXL_VIEW_HOME : DXL_VIEW_NONE;
}

static void decide(dxl_run *r) {
    dxl_policy_input in = {
        .cmdline = r->words,
        .first_run = dxl_config_first_run(r->config),
        .other_instance = r->instance == NULL,
        .forward_tried = 1,
        .running_ini_exists = dxl_sentinel_exists(&r->sentinel),
        .is_client = !dxl_cmd_param(r->words, "server"),
    };
    dxl_policy_decide(&in, &r->decision);
    r->view = view_of(r);
    dxl_log("decision: %s, %s; FirstRun %d; Running.ini %d; screen %s",
            dxl_action_name(r->decision.action), dxl_screen_name(r->decision.screen),
            r->decision.effective_first_run, in.running_ini_exists, view_name(r->view));
}

/* Saved, FirstRun raised, the sentinel made: after the screens, before the
 * game, in the original's order (dx-reverse-info/launch-flow.md section
 * 5). */
static void commit(dxl_run *r) {
    dxl_err e;
    /* The engine's own sections whole before it runs, a profile made by an
     * earlier launcher's too (core/engine_sections.h). */
    if (dxl_engine_sections_seed(dxl_config_ini(r->config)) > 0) dxl_log("the engine's sections seeded");
    if (dxl_sentinel_create(&r->sentinel, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    dxl_config_clamp_first_run(r->config);
    if (dxl_config_save(r->config, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    if (dxl_session_save(&r->session, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
}

/* The game, and its end recorded. */
static int play(dxl_run *r, const dxl_run_ui *ui, volatile int *stop) {
    char *log = dxl_lastrun_rotate(r->session.profile.logs_dir);
    char **argv = engine_argv(r);
    char *line = dxl_argv_quoted(argv);
    /* A server's password not in the log: its characters as '*'. */
    for (char *pw = strstr(line, "?password="); pw; pw = strstr(pw, "?password=")) {
        pw += 10;
        for (; *pw && *pw != ' ' && *pw != '\'' && *pw != '"'; pw++) *pw = '*';
    }
    dxl_log("starting the game: %s", line);
    free(line);
    dxl_play_request req = {
        .argv = argv,
        .env = engine_env(r),
        .workdir = r->session.install.system_dir,
        .log_path = log,
        .instance = r->instance,
        .stop = stop,
    };
    if (ui && ui->before_play) ui->before_play(ui->ctx, r);
    dxl_play_result_free(&r->played);
    dxl_play(&req, &r->played);
    if (ui && ui->after_play) ui->after_play(ui->ctx, r);
    dxl_argv_free(argv);

    dxl_lastrun *last = &r->last;
    memset(last, 0, sizeof *last);
    last->known = 1;
    last->ended = (long)time(NULL);
    last->status = r->played.started ? r->played.status : 127;
    last->signal = r->played.signal;
    last->ready = r->played.ready;
    last->clean = r->played.clean;
    /* Where it started, a server's password left out. */
    const char *pw = strstr(r->url, "?password=");
    snprintf(last->start, sizeof last->start, "%.*s", pw ? (int)(pw - r->url) : (int)strlen(r->url), r->url);
    if (!r->played.started)
        snprintf(last->error, sizeof last->error, "%s", r->played.error);
    else
        dxl_lastrun_error_line(log, last->error, sizeof last->error);
    dxl_err e;
    if (dxl_lastrun_save(last, r->session.profile.state_dir, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    free(log);

    if (r->played.clean) dxl_sentinel_remove(&r->sentinel);
    else dxl_log("not a clean end: Running.ini stays for the next start");
    return last->status;
}

int dxl_run_main(int argc, char **argv, const dxl_run_ui *ui, volatile int *stop) {
    dxl_run *r = dxl_xmalloc(sizeof *r);
    memset(r, 0, sizeof *r);
    int rc = 0;
    dxl_err e;
    if (dxl_run_parse(argc, argv, &r->opt, &r->words, &e) != 0) {
        fprintf(stderr, "%s\n", dxl_err_msg(&e));
        free(r);
        return DXL_RUN_EXIT_USAGE;
    }
    if (r->opt.mod && !r->opt.import_only) {
        fprintf(stderr, "--mod makes a mod's profile, with --import\n");
        free(r->words);
        free(r);
        return DXL_RUN_EXIT_USAGE;
    }
    if (dxl_session_open(&r->session, r->opt.profile, &e) != 0) {
        fprintf(stderr, "%s\n", dxl_err_msg(&e));
        rc = DXL_RUN_EXIT_NOGAME;
        goto done;
    }
    char *id = dxl_session_instance_id(&r->session);

    /* 1. Forwarding, before anything is written. */
    dxl_policy_input pre = { .cmdline = r->words, .forward_tried = 1 };
    dxl_decision first;
    dxl_policy_decide(&pre, &first);
    int busy = dxl_instance_other_running(id);
    /* The line, and what of the launcher's own the one running takes too:
     * a link's server first, as a URL is the original's first word; its
     * window on the big screen last. */
    size_t handed_size = strlen(r->words) + (r->opt.join ? strlen(r->opt.join) : 0) + 20;
    char *handed = dxl_xmalloc(handed_size);
    snprintf(handed, handed_size, "%s%s%s%s%s", r->opt.join ? r->opt.join : "", r->opt.join && r->words[0] ? " " : "",
             r->words, r->opt.big_screen && (r->words[0] || r->opt.join) ? " " : "",
             r->opt.big_screen ? "--big-screen" : "");
    int forwarded = !r->opt.dry && !r->opt.print_command && !r->opt.import_only && !first.skip_handoff && busy &&
                    dxl_instance_forward(id, handed, HANDOFF_TIMEOUT_MS, &e) == 0;
    free(handed);
    if (forwarded) {
        fprintf(stderr, "the launcher already running took this command line\n");
        free(id);
        goto done;
    }
    open_log(r);
    dxl_log("Init: command line: %s", r->words);
    if (!r->opt.dry && !r->opt.print_command) r->instance = dxl_instance_acquire(id);
    free(id);

    /* 2. The profile, and its install. */
    if (r->opt.mod && dxl_profile_exists(&r->session.profile)) {
        stop_with(r, "profile %s is made already: --mod makes a new one, --profile=<id> naming it",
                  r->session.profile.id);
        rc = DXL_RUN_EXIT_USAGE;
        goto done;
    }
    if (!dxl_profile_exists(&r->session.profile)) {
        if (r->opt.dry || r->opt.print_command) {
            if (r->opt.game) dxl_install_check(r->opt.game, &r->session.install);
        } else if ((rc = first_run(r, ui)) != 0) {
            if (rc < 0) rc = 0;   /* the player quit */
            goto done;
        }
    } else if (r->opt.game && !r->opt.dry && !r->opt.print_command) {
        if ((rc = repoint(r)) != 0) goto done;
    }
    if (r->opt.import_only) {
        if (dxl_session_save(&r->session, &e) != 0) stop_with(r, "%s", dxl_err_msg(&e));
        printf("profile %s made from %s\n", r->session.profile.id,
               r->session.install.root ? r->session.install.root : r->opt.game);
        goto done;
    }
    if (dxl_profile_exists(&r->session.profile) && dxl_session_check_install(&r->session) != 0 && !r->opt.dry) {
        if (ui && ui->view && !r->opt.print_command) {
            dxl_log("profile %s: its install is not there to play (%s)", r->session.profile.id,
                    r->session.install.problem);
            if (!ui->view(ui->ctx, r, DXL_VIEW_INSTALL)) goto done;
        }
        if (dxl_session_check_install(&r->session) != 0) {
            const dxl_install *in = &r->session.install;
            stop_with(r, "profile %s has no install to play%s%s%s%s", r->session.profile.id,
                      in->root ? " at " : "", in->root ? in->root : "", in->problem[0] ? ": " : "", in->problem);
            rc = DXL_RUN_EXIT_NOGAME;
            goto done;
        }
    }
    dxl_sentinel_init(&r->sentinel, r->session.profile.state_dir);
    dxl_lastrun_load(&r->last, r->session.profile.state_dir);
    if (dxl_profile_exists(&r->session.profile))
        r->config = dxl_config_open_files(r->session.install.system_dir ? r->session.install.system_dir : ".",
                                          PACKAGE, r->session.profile.system_ini, r->session.profile.user_ini);

    if (!dxl_profile_exists(&r->session.profile)) r->view = DXL_VIEW_FIRST_RUN;
    /* A screen the player has just answered with Play: none again for this
     * game, though its Running.ini is still there. */
    int answered = 0, back_home = 0;
    for (;;) {
        /* 3. The decision. */
        if (r->config) decide(r);
        if (answered) r->view = DXL_VIEW_NONE;
        if (back_home) r->view = DXL_VIEW_HOME;
        answered = back_home = 0;
        if (r->opt.dry) {
            dry_run(r);
            goto done;
        }
        if (r->opt.print_command) {
            if (!r->session.install.ok) {
                fprintf(stderr, "no install to play: %s\n", r->session.install.problem);
                rc = DXL_RUN_EXIT_NOGAME;
            } else {
                print_command(r);
            }
            goto done;
        }
        if (r->decision.action == DXL_ACTION_CONSOLE_CMD) {
            dxl_log("Executing console command %s", r->decision.console_command);
            goto done;
        }
        if (r->decision.action == DXL_ACTION_TEST_RENDEV) {
            fprintf(stderr, "-testrendev: the renderers this machine runs are dxl-cli --probe's\n");
            goto done;
        }
        if (r->view != DXL_VIEW_NONE) {
            if (ui && ui->view) {
                if (!ui->view(ui->ctx, r, r->view)) {
                    if (dxl_session_save(&r->session, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
                    goto done;
                }
            } else if (r->view != DXL_VIEW_HOME) {
                say_view(r, r->view);
                rc = DXL_RUN_EXIT_SCREEN;
                goto done;
            }
        }

        /* A link's server, when no screen asked about it first nor chose
         * another start: joined. */
        char link[256];
        if (r->opt.join && !r->url[0] && dxl_server_link(r->opt.join, link, sizeof link)) {
            snprintf(r->url, sizeof r->url, "%s", link);
            dxl_log("play: joining %s, a link's", link);
        }
        r->opt.join = NULL;

        /* A mod's profile plays only while its mod is there: with a window,
         * Home again, which says so. */
        const char *mod_dir = dxl_profile_get(&r->session.profile, "Mod");
        dxl_mod mod;
        if (mod_dir && *mod_dir) {
            if (dxl_mod_check(mod_dir, &mod, &e) != 0) {
                dxl_log("profile %s: its mod is not there to play: %s", r->session.profile.id, dxl_err_msg(&e));
                if (ui && ui->view) {
                    /* No game: its start and its run's own words as after one. */
                    r->url[0] = '\0';
                    if (ui->after_play) ui->after_play(ui->ctx, r);
                    back_home = 1;
                    continue;
                }
                stop_with(r, "profile %s's mod is not there to play: %s", r->session.profile.id, dxl_err_msg(&e));
                rc = DXL_RUN_EXIT_NOGAME;
                goto done;
            }
            dxl_mod_free(&mod);
        }

        /* 4 and 5. */
        dxl_engine_version v;
        if (check_engine(r, &v) != 0) {
            rc = DXL_RUN_EXIT_NOGAME;
            goto done;
        }
        /* A save to start on only with an engine that starts on one: to an
         * older one a bare ?loadgame is a map it cannot load. */
        if (!strncmp(r->url, "?loadgame=", 10) && !dxl_engine_has(&v, "loadgame-url")) {
            dxl_log("engine: no loadgame-url feature: the game's own start, not %s", r->url);
            r->url[0] = '\0';
        }
        commit(r);
        rc = play(r, ui, stop);
        r->url[0] = '\0';
        /* The inis as the engine left them: it writes them whole at a clean
         * end, the game's own changes in them. */
        if (r->config) dxl_config_reload(r->config);

        /* 6. The end. */
        if (!r->played.clean) {
            char line[600];
            dxl_lastrun_describe(&r->last, line, sizeof line);
            fprintf(stderr, "The game did not end cleanly: %s.\n", line);
            if (ui && ui->view) {
                r->view = DXL_VIEW_RECOVERY;
                if (ui->view(ui->ctx, r, DXL_VIEW_RECOVERY)) {
                    answered = 1;
                    continue;
                }
            } else {
                char *log = dxl_path_join(r->session.profile.logs_dir, DXL_LASTRUN_LOG);
                fprintf(stderr, "Its log: %s\n", log);
                free(log);
            }
            if (rc == 0) rc = 1;
            goto done;
        }
        if (r->played.relaunch) {
            /* The original's RELAUNCH starts DeusEx.exe again with the line:
             * a dedicated server (-server), the renderer page
             * (-changevideo); decided again here, in this process. */
            dxl_log("relaunch: %s", r->played.relaunch);
            free(r->words);
            r->words = dxl_xstrdup(r->played.relaunch);
            continue;
        }
        /* Back to Home after a clean end, when the player asked for it:
         * Home whatever the line asks for (-safe, -changevideo), where Home
         * is shown at all. */
        if (ui && ui->view && home_wanted(r) && dxl_ini_get_bool(r->session.ini, "Launcher", "ReturnAfterPlaying", 0)) {
            dxl_log("back to Home after playing");
            back_home = 1;
            continue;
        }
        break;
    }

done:
    if (r->message[0] && !r->opt.dry) fprintf(stderr, "%s\n", r->message);
    dxl_log("Exit (%d)", rc);
    dxl_log_close();
    if (r->instance) dxl_instance_release(r->instance);
    dxl_config_free(r->config);
    if (r->sentinel.path) dxl_sentinel_free(&r->sentinel);
    dxl_play_result_free(&r->played);
    dxl_session_close(&r->session);
    free(r->words);
    free(r);
    return rc;
}
