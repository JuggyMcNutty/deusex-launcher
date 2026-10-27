#define _GNU_SOURCE
#include "screens/window.h"
#include "screens/shell.h"
#include "screens/uiscript.h"
#include "app/browser.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "platform/process.h"
#include "platform/wait.h"
#include "ui/icon.h"
#include "ui/text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The window as it is made, the panel's size, before full screen sizes it
 * to the display. */
#define FIRST_W    1280
#define FIRST_H    720
/* A pad there as the window opens is no news. */
#define PADS_SETTLE_S 1.5
#define FRAME_NS 16666666ull
/* The title music's fade as Play closes the window. */
#define FADE_S 0.35f

/* One look for the game, in its worker (core/finder.h): what it finds goes
 * through its lock to the window's thread, an event telling it as each
 * comes. The window's and the worker's, freed by the last to let go: a
 * window closed, or another look begun, leaves a worker held up on a drive
 * that does not answer to end on its own, telling no one. */
typedef struct {
    SDL_Mutex     *lock;
    SDL_AtomicInt  refs;
    atomic_int     stop;
    int            quiet;          /* the window let go: no events */
    int            done;
    Uint32         event;
    dxl_found_list found;          /* found, not taken yet */
    char          *given;
    char         **known;
} find_job;

/* One look for servers, in its worker (app/browser.h), as the finder's:
 * what comes waits under its lock for the window's thread, each told by an
 * event; freed by the last of the two to let go. */
typedef struct {
    char  address[64];
    int   port, ping, lan;
    char *status;       /* NULL: listed, not answered */
} browse_got;

typedef struct {
    SDL_Mutex     *lock;
    SDL_AtomicInt  refs;
    atomic_int     stop;
    int            quiet, done;
    Uint32         event;
    char          *master;
    char         **also;
    dxl_browse_lan lan;
    browse_got    *got;
    int            got_count, got_cap;
    char           error[300];
    int            failed;
} browse_job;

struct dxl_window {
    dxl_shell      shell;
    const char    *version;
    volatile int  *stop;
    dxl_run       *run;
    SDL_Window    *window;
    SDL_Renderer  *renderer;
    int            vsync;
    int            sdl, pads, kit;
    int            audio_up;       /* SDL's audio subsystem, up with the window */
    Uint32         wake_event;
    double         opened;
    int            cursor_hidden;
    int            diags;          /* the kit's diagnostics last logged */
    int            big, big_decided;   /* the big screen, decided as the window first opens */
    dxl_uiscript  *script;         /* DXL_UI_SCRIPT's input, or NULL */
    char          *words_before;   /* the command line as it was before a run's own words */

    /* The thread that waits on a second start's line and on signals. */
    SDL_Thread    *listener;
    dxl_wake      *quit;
    SDL_Semaphore *drained;

    /* The GPU probe, dxl-cli --probe --ini in a worker. */
    SDL_Thread    *probe_thread;
    SDL_AtomicInt  probe_done;
    dxl_gpu_probe  probe_result;
    char          *cli;
    Uint32         job_event;

    /* The finder's look, in its worker; NULL for none. The server
     * browser's. */
    find_job      *find;
    browse_job    *browse;
};

/* The launcher's id, its window's to SDL. */
#define DXL_APP_ID "io.github.juggymcnutty.DXLauncher"

/* A signal's: set by its handler. */
static dxl_wake *signal_wake;

void dxl_window_wake(void) { dxl_wake_set(signal_wake); }

static double now_s(void) { return (double)SDL_GetTicksNS() / 1e9; }

/* ---- the listener ------------------------------------------------------ */

static int SDLCALL listen_main(void *p) {
    dxl_window *w = p;
    dxl_instance *inst = w->run ? w->run->instance : NULL;
    dxl_wake *wakes[2] = { w->quit, signal_wake };
    for (;;) {
        int got = dxl_wait(wakes, 2, inst, -1);
        if (got < 0 || (got & 1)) break;
        if (got & 2) {
            dxl_wake_clear(signal_wake);
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
        }
        if (got & DXL_WAIT_LINE) {
            SDL_Event e;
            SDL_zero(e);
            e.type = w->wake_event;
            SDL_PushEvent(&e);
            /* Readable until the window has taken the line. */
            SDL_WaitSemaphore(w->drained);
        }
    }
    return 0;
}

static void listen_start(dxl_window *w) {
    if (w->listener || !(w->quit = dxl_wake_new())) return;
    w->drained = SDL_CreateSemaphore(0);
    w->listener = SDL_CreateThread(listen_main, "dxl-listen", w);
    if (!w->listener) {
        dxl_log("no listener thread: %s", SDL_GetError());
        dxl_wake_free(w->quit);
        w->quit = NULL;
    }
}

static void listen_stop(dxl_window *w) {
    if (!w->listener) return;
    dxl_wake_set(w->quit);
    SDL_SignalSemaphore(w->drained);
    SDL_WaitThread(w->listener, NULL);
    w->listener = NULL;
    SDL_DestroySemaphore(w->drained);
    w->drained = NULL;
    dxl_wake_free(w->quit);
    w->quit = NULL;
}

/* A second start's command line: the window raised. */
static void take_lines(dxl_window *w) {
    char line[DXL_HANDOFF_MAX];
    while (w->run && dxl_instance_poll(w->run->instance, line, sizeof line)) {
        dxl_log("a second start's command line: %s", line);
        /* Brought back when minimized alone: SDL's restore un-maximizes a
         * maximized window too. */
        if (SDL_GetWindowFlags(w->window) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(w->window);
        SDL_RaiseWindow(w->window);
        /* A link's server, the line's first word: Multiplayer asks. */
        char first[256], link[256];
        snprintf(first, sizeof first, "%.*s", (int)strcspn(line, " "), line);
        if (dxl_server_link(first, link, sizeof link) && w->shell.run && w->shell.run->session.install.ok)
            dxl_shell_join_link(&w->shell, link);
        dxl_shell_toast(&w->shell, "DXLauncher is open already: here it is.");
    }
    if (w->drained) SDL_SignalSemaphore(w->drained);
}

/* ---- the GPU probe ----------------------------------------------------- */

static int SDLCALL probe_main(void *p) {
    dxl_window *w = p;
    char *argv[] = { w->cli, "--probe", "--ini", NULL };
    char out[4096];
    int status = 0;
    dxl_err e;
    if (dxl_run_capture(argv, 10000, out, sizeof out, NULL, 0, &status, &e) != 0 ||
        dxl_gpu_probe_from_ini(out, &w->probe_result) != 0) {
        memset(&w->probe_result, 0, sizeof w->probe_result);
        snprintf(w->probe_result.note, sizeof w->probe_result.note, "%s --probe gave no answer", w->cli);
    }
    SDL_SetAtomicInt(&w->probe_done, 1);
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = w->job_event;
    SDL_PushEvent(&ev);
    return 0;
}

static void start_probe(dxl_shell *s) {
    dxl_window *w = s->window;
    if (w->probe_thread || !w->run) return;
    free(w->cli);
    w->cli = dxl_path_join(w->run->session.app_dir, "dxl-cli" DXL_EXE_SUFFIX);
    SDL_SetAtomicInt(&w->probe_done, 0);
    w->probe_thread = SDL_CreateThread(probe_main, "dxl-probe", w);
    if (!w->probe_thread) dxl_log("no probe thread: %s", SDL_GetError());
}

/* The probe's answer, once it is in. */
static void probe_collect(dxl_window *w) {
    if (!w->probe_thread || !SDL_GetAtomicInt(&w->probe_done)) return;
    SDL_WaitThread(w->probe_thread, NULL);
    w->probe_thread = NULL;
    dxl_shell_probed(&w->shell, &w->probe_result);
}

/* ---- the finder -------------------------------------------------------- */

static void find_unref(find_job *j) {
    if (SDL_AddAtomicInt(&j->refs, -1) != 1) return;
    dxl_found_list_free(&j->found);
    SDL_DestroyMutex(j->lock);
    free(j->given);
    for (size_t i = 0; j->known && j->known[i]; i++) free(j->known[i]);
    free(j->known);
    free(j);
}

/* Under the lock: the window told, while it listens. */
static void find_tell(find_job *j) {
    if (j->quiet) return;
    SDL_Event e;
    SDL_zero(e);
    e.type = j->event;
    SDL_PushEvent(&e);
}

static void find_one(void *ctx, const dxl_found *f) {
    find_job *j = ctx;
    SDL_LockMutex(j->lock);
    dxl_found_list_add(&j->found, f);
    find_tell(j);
    SDL_UnlockMutex(j->lock);
}

static int SDLCALL find_main(void *p) {
    find_job *j = p;
    dxl_finder_where where;
    memset(&where, 0, sizeof where);
    where.given = j->given;
    where.known = (const char *const *)j->known;
    dxl_finder_run(&where, find_one, j, &j->stop);
    SDL_LockMutex(j->lock);
    j->done = 1;
    find_tell(j);
    SDL_UnlockMutex(j->lock);
    find_unref(j);
    return 0;
}

/* The window's look let go of: told to stop, left to end. */
static void find_drop(dxl_window *w) {
    find_job *j = w->find;
    if (!j) return;
    w->find = NULL;
    SDL_LockMutex(j->lock);
    atomic_store(&j->stop, 1);
    j->quiet = 1;
    SDL_UnlockMutex(j->lock);
    find_unref(j);
}

static void start_find(dxl_shell *s, const dxl_finder_where *where) {
    dxl_window *w = s->window;
    find_drop(w);
    find_job *j = dxl_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    atomic_init(&j->stop, 0);
    j->lock = SDL_CreateMutex();
    j->event = w->job_event;
    j->given = where->given ? dxl_xstrdup(where->given) : NULL;
    size_t n = 0;
    while (where->known && where->known[n]) n++;
    j->known = dxl_xmalloc((n + 1) * sizeof *j->known);
    for (size_t i = 0; i < n; i++) j->known[i] = dxl_xstrdup(where->known[i]);
    j->known[n] = NULL;
    SDL_SetAtomicInt(&j->refs, 2);
    SDL_Thread *t = j->lock ? SDL_CreateThread(find_main, "dxl-find", j) : NULL;
    if (!t) {
        /* No worker: the look now, the window waiting on it. */
        dxl_log("no finder thread (%s): it looks now", SDL_GetError());
        SDL_SetAtomicInt(&j->refs, 1);
        find_unref(j);
        dxl_finder_find(&s->found, where);
        dxl_shell_found_all(s);
        return;
    }
    SDL_DetachThread(t);
    w->find = j;
}

/* What the look has found since the last time, into the page's list; and
 * its end. */
static void find_collect(dxl_window *w) {
    find_job *j = w->find;
    if (!j) return;
    SDL_LockMutex(j->lock);
    for (int i = 0; i < j->found.count; i++) dxl_shell_found(&w->shell, &j->found.items[i]);
    dxl_found_list_free(&j->found);
    int done = j->done;
    SDL_UnlockMutex(j->lock);
    if (!done) return;
    find_drop(w);
    dxl_shell_found_all(&w->shell);
}

/* ---- the server browser ------------------------------------------------- */

static void browse_unref(browse_job *j) {
    if (SDL_AddAtomicInt(&j->refs, -1) != 1) return;
    for (int i = 0; i < j->got_count; i++) free(j->got[i].status);
    free(j->got);
    SDL_DestroyMutex(j->lock);
    free(j->master);
    for (size_t i = 0; j->also && j->also[i]; i++) free(j->also[i]);
    free(j->also);
    free(j);
}

static void browse_tell(browse_job *j) {
    if (j->quiet) return;
    SDL_Event e;
    SDL_zero(e);
    e.type = j->event;
    SDL_PushEvent(&e);
}

static void browse_put(browse_job *j, const char *address, int port, int lan, int ping, const char *status) {
    SDL_LockMutex(j->lock);
    if (j->got_count == j->got_cap) {
        j->got_cap = j->got_cap ? j->got_cap * 2 : 64;
        j->got = dxl_xrealloc(j->got, sizeof *j->got * (size_t)j->got_cap);
    }
    browse_got *g = &j->got[j->got_count++];
    snprintf(g->address, sizeof g->address, "%s", address);
    g->port = port;
    g->lan = lan;
    g->ping = ping;
    g->status = status ? dxl_xstrdup(status) : NULL;
    browse_tell(j);
    SDL_UnlockMutex(j->lock);
}

static void browse_listed(void *ctx, const char *address, int port, int lan) {
    browse_put(ctx, address, port, lan, -1, NULL);
}

static void browse_answered(void *ctx, const char *address, int port, int ping, const char *status) {
    browse_put(ctx, address, port, 0, ping, status);
}

static int SDLCALL browse_main(void *p) {
    browse_job *j = p;
    dxl_browse_fns fns = { browse_listed, browse_answered };
    dxl_err e;
    e.msg[0] = '\0';
    int rc = dxl_browse(j->master, (const char *const *)j->also, &j->lan, &fns, j, &j->stop, &e);
    SDL_LockMutex(j->lock);
    j->done = 1;
    j->failed = rc != 0;
    snprintf(j->error, sizeof j->error, "%s", e.msg);
    browse_tell(j);
    SDL_UnlockMutex(j->lock);
    browse_unref(j);
    return 0;
}

static void browse_drop(dxl_window *w) {
    browse_job *j = w->browse;
    if (!j) return;
    w->browse = NULL;
    SDL_LockMutex(j->lock);
    atomic_store(&j->stop, 1);
    j->quiet = 1;
    SDL_UnlockMutex(j->lock);
    browse_unref(j);
}

static void start_browse(dxl_shell *s, const char *master, const char *const *also, const dxl_browse_lan *lan) {
    dxl_window *w = s->window;
    browse_drop(w);
    browse_job *j = dxl_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    atomic_init(&j->stop, 0);
    j->lock = SDL_CreateMutex();
    j->event = w->job_event;
    j->master = dxl_xstrdup(master ? master : "");
    if (lan) j->lan = *lan;
    else j->lan.none = 1;
    size_t n = 0;
    while (also && also[n]) n++;
    j->also = dxl_xmalloc((n + 1) * sizeof *j->also);
    for (size_t i = 0; i < n; i++) j->also[i] = dxl_xstrdup(also[i]);
    j->also[n] = NULL;
    SDL_SetAtomicInt(&j->refs, 2);
    SDL_Thread *t = j->lock ? SDL_CreateThread(browse_main, "dxl-browse", j) : NULL;
    if (!t) {
        dxl_log("no server browser thread: %s", SDL_GetError());
        SDL_SetAtomicInt(&j->refs, 1);
        browse_unref(j);
        dxl_shell_browsed(s, "the launcher could not start its look for servers");
        return;
    }
    SDL_DetachThread(t);
    w->browse = j;
}

/* What the look has found since the last time, into the page's list; and
 * its end. */
static void browse_collect(dxl_window *w) {
    browse_job *j = w->browse;
    if (!j) return;
    SDL_LockMutex(j->lock);
    for (int i = 0; i < j->got_count; i++) {
        browse_got *g = &j->got[i];
        if (g->status) dxl_shell_server_answered(&w->shell, g->address, g->port, g->ping, g->status);
        else dxl_shell_server_listed(&w->shell, g->address, g->port, g->lan);
        free(g->status);
    }
    j->got_count = 0;
    int done = j->done, failed = j->failed;
    char error[300];
    snprintf(error, sizeof error, "%s", j->error);
    SDL_UnlockMutex(j->lock);
    if (!done) return;
    browse_drop(w);
    dxl_shell_browsed(&w->shell, failed ? error : NULL);
}

/* ---- the window ---------------------------------------------------------- */

/* launcher.ini kept as the session left it -- the profile and the installs
 * it met --, once there is a profile: a first run left before its end
 * writes nothing. The window is full screen, its size the display's, and
 * none of it is kept. */
static void close_window(dxl_window *w, dxl_run *r) {
    if (!w->window) return;
    if (r && dxl_profile_exists(&r->session.profile)) {
        dxl_err e;
        if (dxl_session_save(&r->session, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    }
    dxl_shell_release(&w->shell);
    /* The look for the game, if it is still on, is for a page gone: one cut
     * short is made again from the start, if the page comes again. */
    find_drop(w);
    if (w->shell.finding) w->shell.looked = 0;
    w->shell.finding = 0;
    /* And the look for servers: asked again as the page comes again. */
    browse_drop(w);
    if (w->shell.browsing) w->shell.browsed = 0;
    w->shell.browsing = 0;
    /* The sound goes with the window: the game opens its own. */
    dxl_audio_close(w->shell.audio);
    w->shell.audio = NULL;
    if (w->audio_up) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    w->audio_up = 0;
    dxl_ui_detach(&w->shell.ui);
    SDL_DestroyRenderer(w->renderer);
    SDL_DestroyWindow(w->window);
    w->renderer = NULL;
    w->window = NULL;
    if (w->pads) SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    w->pads = 0;
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    w->cursor_hidden = 0;
}

/* Whether the window is full screen, once the desktop has done what it
 * was asked. */
static int full_screen(dxl_window *w) {
    SDL_SyncWindow(w->window);
    return (SDL_GetWindowFlags(w->window) & SDL_WINDOW_FULLSCREEN) != 0;
}

static int open_window(dxl_window *w, dxl_run *r) {
    dxl_shell *s = &w->shell;
    if (w->window) return 0;
    if (!w->sdl) {
        SDL_SetAppMetadata("DXLauncher", w->version, DXL_APP_ID);
        /* A launcher waiting on the player keeps no screensaver off; the
         * signals are the launcher's own (dxl_window_wake). */
        SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
        SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            fprintf(stderr, "DXLauncher has no window: %s\n", SDL_GetError());
            dxl_log("no window: %s", SDL_GetError());
            return -1;
        }
        if (!TTF_Init()) {
            fprintf(stderr, "DXLauncher has no fonts: %s\n", SDL_GetError());
            SDL_Quit();
            return -1;
        }
        w->wake_event = SDL_RegisterEvents(2);
        w->job_event = w->wake_event + 1;
        w->sdl = 1;
    }
    /* With the controllers the mapping wizard made known. */
    if (!w->pads) w->pads = dxl_pad_mappings(r, 1);
    w->window = SDL_CreateWindow("DXLauncher", FIRST_W, FIRST_H,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!w->window) {
        fprintf(stderr, "DXLauncher has no window: %s\n", SDL_GetError());
        return -1;
    }
    /* The Smart Pro's screen is the big screen alone: the panel, no desktop. */
    if (!w->big_decided) {
        w->big = 1;
        w->big_decided = 1;
        dxl_log("the big screen, the handheld's");
    }
    if (w->big && !SDL_SetWindowFullscreen(w->window, true)) dxl_log("no full screen: %s", SDL_GetError());
    SDL_Surface *icon = dxl_icon(64);
    if (icon) {
        SDL_SetWindowIcon(w->window, icon);
        SDL_DestroySurface(icon);
    }
    /* launcher.ini's RenderDriver, else SDL_RENDER_DRIVER's or SDL's own
     * choice; the software renderer when none starts. */
    const char *driver = dxl_ini_get(r->session.ini, "Launcher", "RenderDriver");
    w->renderer = SDL_CreateRenderer(w->window, driver && *driver ? driver : NULL);
    if (!w->renderer && driver && *driver) {
        dxl_log("no %s renderer (%s)", driver, SDL_GetError());
        w->renderer = SDL_CreateRenderer(w->window, NULL);
    }
    if (!w->renderer) {
        dxl_log("no renderer (%s): the software one", SDL_GetError());
        w->renderer = SDL_CreateRenderer(w->window, SDL_SOFTWARE_RENDERER);
    }
    if (!w->renderer) {
        fprintf(stderr, "DXLauncher has no renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(w->window);
        w->window = NULL;
        return -1;
    }
    w->vsync = SDL_SetRenderVSync(w->renderer, 1);
    char fonts[1100];
    snprintf(fonts, sizeof fonts, "%s/fonts", r->session.data_dir);
    int rc;
    if (w->kit) {
        s->ui.big = w->big;
        rc = dxl_ui_attach(&s->ui, w->window, w->renderer);
    } else {
        rc = dxl_ui_init(&s->ui, w->window, w->renderer, fonts, 1);
        if (rc == 0) dxl_ui_set_big(&s->ui, w->big);
    }
    s->big = w->big;
    s->ui.reduced_motion = dxl_ini_get_bool(r->session.ini, "Launcher", "ReducedMotion", 0);
    /* The handheld's own pad is the way in: its hints from the first frame. */
    s->ui.mode = DXL_INPUT_PAD;
    if (rc != 0) {
        fprintf(stderr, "DXLauncher cannot draw: %s\n", SDL_GetError());
        dxl_ui_detach(&s->ui);
        SDL_DestroyRenderer(w->renderer);
        SDL_DestroyWindow(w->window);
        w->renderer = NULL;
        w->window = NULL;
        return -1;
    }
    w->kit = 1;
    dxl_log("window: the %s renderer%s, %dx%d px at %.2f px a dp", SDL_GetRendererName(w->renderer),
            w->vsync ? ", VSync" : "", s->ui.w_px, s->ui.h_px, s->ui.scale);
    SDL_ShowWindow(w->window);
    if (w->big && !full_screen(w)) {
        dxl_log("the big screen: off, the desktop kept the window as it was");
        w->big = s->big = 0;
        dxl_ui_set_big(&s->ui, 0);
    }
    w->opened = now_s();
    /* Its sound: the title music and the menu's sounds (dxl_shell_sound). */
    w->audio_up = SDL_InitSubSystem(SDL_INIT_AUDIO);
    s->audio = w->audio_up ? dxl_audio_open() : NULL;
    s->audio_given = 0;
    s->window = w;
    s->start_probe = start_probe;
    s->start_find = start_find;
    s->start_browse = start_browse;
    return 0;
}

static void handle(dxl_window *w, const SDL_Event *e) {
    dxl_shell *s = &w->shell;
    dxl_ui_event(&s->ui, e);
    if (e->type == SDL_EVENT_GAMEPAD_ADDED) {
        if (now_s() - w->opened > PADS_SETTLE_S) {
            const char *name = SDL_GetGamepadNameForID(e->gdevice.which);
            dxl_shell_toast(s, "A controller is connected: %s.", name ? name : "a pad");
        } else if (w->big && s->ui.mode == DXL_INPUT_MOUSE) {
            /* On the big screen a pad there as it opens is the way in:
             * the hints its own from the first frame. */
            s->ui.mode = DXL_INPUT_PAD;
            s->ui.pad = e->gdevice.which;
            s->ui.pad_family = dxl_ui_pad_family(SDL_GetGamepadFromID(e->gdevice.which));
        }
    } else if (e->type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN && w->big &&
               !(SDL_GetWindowFlags(w->window) & SDL_WINDOW_FULLSCREEN)) {
        /* The desktop took the window out of full screen: the big screen
         * with it. */
        w->big = s->big = 0;
        dxl_ui_set_big(&s->ui, 0);
        dxl_log("the big screen: off, by the desktop");
    } else if (e->type == SDL_EVENT_GAMEPAD_REMOVED) {
        dxl_shell_toast(s, "A controller was disconnected.");
    } else if (e->type == w->wake_event) {
        take_lines(w);
    } else if (e->type == w->job_event) {
        probe_collect(w);
        find_collect(w);
        browse_collect(w);
    }
}

/* The pointer hides while a pad is in use, and comes back as it moves. */
static void cursor(dxl_window *w) {
    int pad = w->shell.ui.mode == DXL_INPUT_PAD;
    if (pad && !w->cursor_hidden) SDL_HideCursor();
    else if (!pad && w->cursor_hidden) SDL_ShowCursor();
    w->cursor_hidden = pad;
}

static const char *view_name(dxl_view v) {
    switch (v) {
    case DXL_VIEW_HOME: return "Home";
    case DXL_VIEW_FIRST_RUN: return "first run";
    case DXL_VIEW_INSTALL: return "the profile's install";
    case DXL_VIEW_RECOVERY: return "Recovery";
    case DXL_VIEW_GRAPHICS: return "Graphics";
    default: return "?";
    }
}

static int view(void *ctx, dxl_run *r, dxl_view which) {
    dxl_window *w = ctx;
    dxl_shell *s = &w->shell;
    w->run = r;
    if (open_window(w, r) != 0) return 0;
    dxl_shell_enter(s, r, which);
    probe_collect(w);
    listen_start(w);
    while (s->result < 0) {
        dxl_ui_input_begin(&s->ui);
        SDL_Event e;
        double idle = dxl_shell_idle_for(s), next = dxl_uiscript_step(w->script, s, now_s());
        if (next >= 0 && (idle < 0 || next < idle)) idle = next;
        if (s->ui.animating) {
            while (SDL_PollEvent(&e)) handle(w, &e);
        } else {
            if (SDL_WaitEventTimeout(&e, idle < 0 ? -1 : (Sint32)(idle * 1000) + 1)) {
                handle(w, &e);
                while (SDL_PollEvent(&e)) handle(w, &e);
            }
        }
        if (w->stop && *w->stop) s->ui.in.quit = 1;
        cursor(w);
        Uint64 t0 = SDL_GetTicksNS();
        SDL_SetRenderDrawColor(w->renderer, 0, 0, 0, 255);
        SDL_RenderClear(w->renderer);
        dxl_shell_frame(s, now_s());
        dxl_shell_cue(s, s->ui.cues);
        dxl_uiscript_shoot(w->script, w->renderer);
        SDL_RenderPresent(w->renderer);
        if (s->ui.diag_count != w->diags) {
            for (int i = 0; i < s->ui.diag_count && i < 8; i++) dxl_log("ui: %s", s->ui.diag[i]);
            w->diags = s->ui.diag_count;
        }
        if (!w->vsync && s->ui.animating) {
            Uint64 spent = SDL_GetTicksNS() - t0;
            if (spent < FRAME_NS) SDL_DelayNS(FRAME_NS - spent);
        }
    }
    listen_stop(w);
    dxl_shell_leave(s);
    /* Play: the music faded as the window goes. */
    if (s->result == 1 && s->audio) {
        dxl_audio_fade(s->audio, FADE_S);
        SDL_Delay((Uint32)(FADE_S * 1000) + 40);
    }
    if (s->result == 1 && s->next_run[0]) {
        /* The next run's own words, after the player's. */
        size_t n = strlen(r->words) + strlen(s->next_run) + 2;
        char *words = dxl_xmalloc(n);
        snprintf(words, n, "%s%s%s", r->words, r->words[0] ? " " : "", s->next_run);
        free(w->words_before);
        w->words_before = r->words;
        r->words = words;
        dxl_log("this run's words: %s", s->next_run);
        s->next_run[0] = '\0';
    }
    dxl_log("%s: %s", view_name(which), s->result ? "play" : "quit");
    /* A quit is the run's end: the window goes now, while the run is. */
    if (!s->result) close_window(w, r);
    w->run = NULL;
    return s->result;
}

static void before_play(void *ctx, dxl_run *r) {
    close_window(ctx, r);
}

static void after_play(void *ctx, dxl_run *r) {
    /* The window comes back with the next screen, if there is one; the
     * command line as it was before this run's own words. */
    dxl_window *w = ctx;
    if (w->words_before && r) {
        free(r->words);
        r->words = w->words_before;
        w->words_before = NULL;
    }
}

dxl_window *dxl_window_new(const char *version, volatile int *stop) {
    dxl_window *w = dxl_xmalloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->version = version;
    w->stop = stop;
    dxl_shell_init(&w->shell, version);
    w->script = dxl_uiscript_open();
    if (!signal_wake) signal_wake = dxl_wake_new();
    return w;
}

void dxl_window_free(dxl_window *w) {
    if (!w) return;
    listen_stop(w);
    if (w->probe_thread) SDL_WaitThread(w->probe_thread, NULL);
    free(w->cli);
    close_window(w, NULL);
    if (w->kit) dxl_ui_free(&w->shell.ui);
    dxl_shell_free(&w->shell);
    if (w->sdl) {
        TTF_Quit();
        SDL_Quit();
    }
    dxl_uiscript_free(w->script);
    free(w->words_before);
    free(w);
}

void dxl_window_ui(dxl_window *w, dxl_run_ui *ui) {
    ui->ctx = w;
    ui->view = view;
    ui->before_play = before_play;
    ui->after_play = after_play;
}
