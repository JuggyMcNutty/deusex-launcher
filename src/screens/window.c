#define _GNU_SOURCE
#include "screens/window.h"
#include "screens/shell.h"
#include "screens/uiscript.h"
#include "app/browser.h"
#include "core/argv.h"
#include "core/bigscreen.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/desktop.h"
#include "platform/fs.h"
#include "platform/os.h"
#include "platform/process.h"
#include "platform/wait.h"
#include "ui/icon.h"
#include "ui/text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The window at first, and at its smallest, in dp. */
#define FIRST_W    1280
#define FIRST_H    720
#define SMALLEST_W 960
#define SMALLEST_H 600
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

/* The import of a profile being made, in its worker (core/profile.h's
 * dxl_profile_import_watched): into a profile of its own, from copies of
 * the install's folder and the mod; its progress under its lock, an event
 * telling the window each time it has taken the last. The window waits on
 * it as it closes. */
typedef struct {
    SDL_Thread    *thread;
    SDL_Mutex     *lock;
    SDL_AtomicInt  done;
    atomic_int     stop;
    int            told;           /* an event on its way, not taken yet */
    Uint32         event;
    int            files, files_total;
    long long      bytes, bytes_total;
    dxl_profile    profile;
    char          *system_dir;
    dxl_mod        mod;
    int            has_mod;
    dxl_made       made;
    dxl_import     report;
    dxl_err        err;
    int            rc;
} make_job;

/* One folder of the browser's read, in its worker (dxl_browser_read): a
 * folder on a network mount that does not answer holds it, not the window,
 * which lets go of it for another or as it closes; freed by the last of the
 * two to let go (bugs.md LUI.17). */
typedef struct {
    SDL_Mutex     *lock;
    SDL_AtomicInt  refs;
    int            quiet, done;
    Uint32         event;
    char          *dir;
    int            hidden;
    char          *real;
    char         **names;
    int            count;
} list_job;

/* The engine's audit of a mod's natives, in its worker (Mods, bugs.md
 * LCORE.27): the engine killed once the window lets go of it (stop), and
 * the job freed by the last of the two to let go. */
typedef struct {
    SDL_Mutex      *lock;
    SDL_AtomicInt   refs;
    atomic_int      stop;
    int             quiet, done, ok;
    Uint32          event;
    char          **argv;
    dxl_mod_natives natives;
} audit_job;

struct dxl_window {
    dxl_shell      shell;
    const char    *version;
    atomic_int    *stop;
    dxl_run       *run;
    SDL_Window    *window;
    SDL_Renderer  *renderer;
    int            vsync;
    int            sdl, pads, kit;
    int            audio_up;       /* SDL's audio subsystem, up with the window */
    Uint32         wake_event, pick_event;
    double         opened;
    /* The window's smallest as last set (minimum): the points a dp, the
     * display's scale and the launcher's size's or less, so that it fits
     * the room the work area leaves past the frame; the kit's px a dp held
     * to that (0 none); set again for a display of another scale
     * (min_stale). */
    float          min_k, fit;
    int            room_w, room_h, min_stale;
    int            cursor_hidden;
    int            diags;          /* the kit's diagnostics last logged */
    float          min_scale;      /* the launcher's size (UIScale) its smallest is set for */
    int            big, big_decided;   /* the big screen, decided as the window first opens */
    dxl_uiscript  *script;         /* DXL_UI_SCRIPT's input, or NULL */
    char          *words_before;   /* the command line as it was before a run's own words */
    char           failed[300];    /* why the window last did not open, or "" */

    /* The thread that waits on a second start's line and on signals. */
    SDL_Thread    *listener;
    dxl_wake      *quit;
    SDL_Semaphore *drained;

    /* The GPU probe, dxl-cli --probe --ini in a worker, given up as the
     * window goes (probe_stop). */
    SDL_Thread    *probe_thread;
    SDL_AtomicInt  probe_done;
    atomic_int     probe_stop;
    dxl_gpu_probe  probe_result;
    char          *cli;
    Uint32         job_event;

    /* The finder's look, in its worker; NULL for none. The server
     * browser's. */
    find_job      *find;
    browse_job    *browse;
    /* Your profile's import, in its worker; NULL for none. The folder
     * browser's read. */
    make_job      *make;
    list_job      *list;
    /* Mods' audit of a mod's natives, in its worker; NULL for none. */
    audit_job     *audit;

    /* The system's folder dialog's answer, from its thread. */
    SDL_Mutex     *pick_lock;
    int            pick_state;     /* 1 a folder; 2 none chosen; 3 no dialog to show */
    char          *picked;
    char           pick_error[256];
};

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

/* Whether word is one of the line's. */
static int has_word(const char *line, const char *word) {
    size_t n = strlen(word);
    for (const char *p = line; (p = strstr(p, word)); p += n)
        if ((p == line || p[-1] == ' ') && (p[n] == ' ' || !p[n])) return 1;
    return 0;
}

/* A second start's command line: the window raised, on the big screen
 * when it asks for it (run.c passes --big-screen on). */
static void take_lines(dxl_window *w) {
    char line[DXL_HANDOFF_MAX + 1];
    while (w->run && dxl_instance_poll(w->run->instance, line, sizeof line)) {
        dxl_log("a second start's command line: %s", line);
        /* Brought back when minimized alone: SDL's restore un-maximizes a
         * maximized window too. */
        if (SDL_GetWindowFlags(w->window) & SDL_WINDOW_MINIMIZED) SDL_RestoreWindow(w->window);
        SDL_RaiseWindow(w->window);
        if (has_word(line, "--big-screen")) dxl_shell_big(&w->shell, 1);
        /* A link's server, the line's first word: Multiplayer asks. */
        char first[256], link[256];
        snprintf(first, sizeof first, "%.*s", (int)strcspn(line, " "), line);
        if (dxl_server_link(first, link, sizeof link) && w->shell.run && w->shell.run->session.install.ok)
            dxl_shell_join_link(&w->shell, link);
        dxl_shell_toast(&w->shell, "DXLauncher is open already: here it is.");
    }
    if (w->drained) SDL_SignalSemaphore(w->drained);
}

/* ---- the system's folder dialog ----------------------------------------- */

static void SDLCALL on_pick(void *ctx, const char *const *files, int filter) {
    (void)filter;
    dxl_window *w = ctx;
    SDL_LockMutex(w->pick_lock);
    free(w->picked);
    w->picked = NULL;
    if (!files) {
        w->pick_state = 3;
        snprintf(w->pick_error, sizeof w->pick_error, "%s", SDL_GetError());
    } else if (!files[0]) {
        w->pick_state = 2;
    } else {
        w->pick_state = 1;
        w->picked = dxl_xstrdup(files[0]);
    }
    SDL_UnlockMutex(w->pick_lock);
    SDL_Event e;
    SDL_zero(e);
    e.type = w->pick_event;
    SDL_PushEvent(&e);
}

static void pick_folder(dxl_shell *s) {
    dxl_window *w = s->window;
    char *dir = s->pick_for == DXL_BROWSE_ENGINE && s->run ? dxl_path_dirname(s->run->session.engine) : NULL;
    SDL_ShowOpenFolderDialog(on_pick, w, w->window, dir ? dir : dxl_fs_home(), false);
    free(dir);
}

static void picked(dxl_window *w) {
    dxl_shell *s = &w->shell;
    SDL_LockMutex(w->pick_lock);
    int state = w->pick_state;
    char *path = w->picked;
    w->picked = NULL;
    w->pick_state = 0;
    SDL_UnlockMutex(w->pick_lock);
    if (state == 1 && path) {
        dxl_shell_folder_picked(s, path);
    } else if (state == 3) {
        dxl_log("no system folder dialog: %s", w->pick_error);
        dxl_shell_message(s, "No folder dialog", "The system has none to show here (%s). Browse... finds the folder "
                          "as well.", w->pick_error);
    }
    free(path);
}

/* ---- the GPU probe ----------------------------------------------------- */

static int SDLCALL probe_main(void *p) {
    dxl_window *w = p;
    char *argv[] = { w->cli, "--probe", "--ini", NULL };
    char out[4096];
    int status = 0;
    dxl_err e;
    if (dxl_run_capture_until(argv, 10000, &w->probe_stop, out, sizeof out, NULL, 0, &status, &e) != 0 ||
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
    atomic_store(&w->probe_stop, 0);
    w->probe_thread = SDL_CreateThread(probe_main, "dxl-probe", w);
    if (!w->probe_thread) {
        /* An answer all the same, as a probe that gave none: nothing waits
         * on it for ever. */
        dxl_log("no probe thread: %s", SDL_GetError());
        memset(&w->probe_result, 0, sizeof w->probe_result);
        snprintf(w->probe_result.note, sizeof w->probe_result.note, "no probe thread: %s", SDL_GetError());
        dxl_shell_probed(s, &w->probe_result);
    }
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

/* ---- Your profile's import ---------------------------------------------- */

static int make_told(void *ctx, int files, int files_total, long long bytes, long long bytes_total) {
    make_job *j = ctx;
    SDL_LockMutex(j->lock);
    j->files = files;
    j->files_total = files_total;
    j->bytes = bytes;
    j->bytes_total = bytes_total;
    if (!j->told) {
        SDL_Event e;
        SDL_zero(e);
        e.type = j->event;
        SDL_PushEvent(&e);
        j->told = 1;
    }
    SDL_UnlockMutex(j->lock);
    return atomic_load(&j->stop);
}

static int SDLCALL make_main(void *p) {
    make_job *j = p;
    j->rc = dxl_profile_import_watched(&j->profile, j->system_dir, j->has_mod ? &j->mod : NULL, make_told, j,
                                       &j->made, &j->report, &j->err);
    SDL_SetAtomicInt(&j->done, 1);
    SDL_Event e;
    SDL_zero(e);
    e.type = j->event;
    SDL_PushEvent(&e);
    return 0;
}

static void make_free(make_job *j) {
    SDL_DestroyMutex(j->lock);
    free(j->system_dir);
    if (j->has_mod) dxl_mod_free(&j->mod);
    free(j);
}

static int start_make(dxl_shell *s) {
    dxl_window *w = s->window;
    dxl_session *ss = &s->run->session;
    if (w->make) return 0;
    make_job *j = dxl_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    atomic_init(&j->stop, 0);
    j->event = w->job_event;
    j->lock = SDL_CreateMutex();
    j->system_dir = dxl_xstrdup(s->chosen.system_dir);
    j->has_mod = s->making_mod.dir && dxl_mod_check(s->making_mod.dir, &j->mod, NULL) == 0;
    /* What the screens read of it each frame stays the session's, and the
     * window's alone: the worker's own of the same id is the session's once
     * the profile is made (dxl_shell_copied). */
    if ((s->making_mod.dir && !j->has_mod) || dxl_profile_open(&j->profile, &ss->xdg, ss->profile.id, NULL) != 0) {
        make_free(j);
        return -1;
    }
    j->thread = j->lock ? SDL_CreateThread(make_main, "dxl-make", j) : NULL;
    if (!j->thread) {
        dxl_log("no thread for the profile's copy (%s): it is made now", SDL_GetError());
        dxl_profile_free(&j->profile);
        make_free(j);
        return -1;
    }
    w->make = j;
    s->copying = 1;
    s->copy_files = 0;
    s->copy_bytes = 0;
    s->copy_files_total = s->plan.files;
    s->copy_bytes_total = s->plan.bytes;
    return 0;
}

static void cancel_make(dxl_shell *s) {
    dxl_window *w = s->window;
    if (w->make) atomic_store(&w->make->stop, 1);
}

/* The import's progress, and once it is done its end, to the page. */
static void make_collect(dxl_window *w) {
    make_job *j = w->make;
    if (!j) return;
    SDL_LockMutex(j->lock);
    w->shell.copy_files = j->files;
    w->shell.copy_bytes = j->bytes;
    if (j->files_total) {
        w->shell.copy_files_total = j->files_total;
        w->shell.copy_bytes_total = j->bytes_total;
    }
    j->told = 0;
    SDL_UnlockMutex(j->lock);
    if (!SDL_GetAtomicInt(&j->done)) return;
    SDL_WaitThread(j->thread, NULL);
    w->make = NULL;
    /* A Cancel that came after the last file was copied -- while the copy
     * was synced, or before this frame -- is a Cancel still: what the
     * import made is taken away, as the worker takes it at a file. */
    int cancelled = atomic_load(&j->stop) != 0;
    if (cancelled && j->rc == 0) {
        dxl_profile_unmake(&j->made);
        j->rc = -1;
        dxl_err_set(&j->err, "the copy was cancelled");
    }
    dxl_shell_copied(&w->shell, &j->profile, &j->made, &j->report, j->rc ? dxl_err_msg(&j->err) : NULL, cancelled);
    make_free(j);
}

/* The window going while the import copies: stopped, waited for, and what
 * it made, a profile with no profile.ini yet, taken away. */
static void make_stop(dxl_window *w) {
    make_job *j = w->make;
    if (!j) return;
    w->make = NULL;
    atomic_store(&j->stop, 1);
    SDL_WaitThread(j->thread, NULL);
    if (j->rc == 0) dxl_profile_unmake(&j->made);
    dxl_profile_free(&j->profile);
    make_free(j);
    w->shell.copying = 0;
    dxl_log("the profile's copy: given up, the window closing");
}

/* ---- the folder browser's read -------------------------------------------- */

static void list_unref(list_job *j) {
    if (SDL_AddAtomicInt(&j->refs, -1) != 1) return;
    for (int i = 0; i < j->count; i++) free(j->names[i]);
    free(j->names);
    free(j->real);
    free(j->dir);
    SDL_DestroyMutex(j->lock);
    free(j);
}

static int SDLCALL list_main(void *p) {
    list_job *j = p;
    char *real, **names;
    int count;
    dxl_browser_read(j->dir, j->hidden, &real, &names, &count);
    SDL_LockMutex(j->lock);
    j->real = real;
    j->names = names;
    j->count = count;
    j->done = 1;
    if (!j->quiet) {
        SDL_Event e;
        SDL_zero(e);
        e.type = j->event;
        SDL_PushEvent(&e);
    }
    SDL_UnlockMutex(j->lock);
    list_unref(j);
    return 0;
}

/* The window's read let go of: left to end, telling no one. */
static void list_drop(dxl_window *w) {
    list_job *j = w->list;
    if (!j) return;
    w->list = NULL;
    SDL_LockMutex(j->lock);
    j->quiet = 1;
    SDL_UnlockMutex(j->lock);
    list_unref(j);
}

static int start_list(dxl_shell *s, const char *dir, int hidden) {
    dxl_window *w = s->window;
    list_drop(w);
    list_job *j = dxl_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    j->lock = SDL_CreateMutex();
    j->event = w->job_event;
    j->dir = dxl_xstrdup(dir);
    j->hidden = hidden;
    SDL_SetAtomicInt(&j->refs, 2);
    SDL_Thread *t = j->lock ? SDL_CreateThread(list_main, "dxl-list", j) : NULL;
    if (!t) {
        dxl_log("no thread for the folder's list (%s): it is read now", SDL_GetError());
        SDL_SetAtomicInt(&j->refs, 1);
        list_unref(j);
        return -1;
    }
    SDL_DetachThread(t);
    w->list = j;
    return 0;
}

/* The folder read, to the browser. */
static void list_collect(dxl_window *w) {
    list_job *j = w->list;
    if (!j) return;
    SDL_LockMutex(j->lock);
    int done = j->done;
    char *real = j->real, **names = j->names;
    int count = j->count;
    if (done) {
        j->real = NULL;
        j->names = NULL;
        j->count = 0;
    }
    SDL_UnlockMutex(j->lock);
    if (!done) return;
    char *dir = dxl_xstrdup(j->dir);
    int hidden = j->hidden;
    list_drop(w);
    dxl_browser_listed(&w->shell, dir, hidden, real, names, count);
    free(dir);
}

/* ---- the audit of a mod's natives ----------------------------------------- */

static void audit_unref(audit_job *j) {
    if (SDL_AddAtomicInt(&j->refs, -1) != 1) return;
    dxl_argv_free(j->argv);
    SDL_DestroyMutex(j->lock);
    free(j);
}

static int SDLCALL audit_main(void *p) {
    audit_job *j = p;
    dxl_mod_natives n;
    int ok = dxl_session_audit_run(j->argv, &j->stop, &n) == 0;
    SDL_LockMutex(j->lock);
    j->natives = n;
    j->ok = ok;
    j->done = 1;
    if (!j->quiet) {
        SDL_Event e;
        SDL_zero(e);
        e.type = j->event;
        SDL_PushEvent(&e);
    }
    SDL_UnlockMutex(j->lock);
    audit_unref(j);
    return 0;
}

/* The window's audit let go of: the engine stopped, the answer told no one. */
static void audit_drop(dxl_window *w) {
    audit_job *j = w->audit;
    if (!j) return;
    w->audit = NULL;
    SDL_LockMutex(j->lock);
    j->quiet = 1;
    atomic_store(&j->stop, 1);
    SDL_UnlockMutex(j->lock);
    audit_unref(j);
}

static int start_audit(dxl_shell *s, char **argv) {
    dxl_window *w = s->window;
    audit_drop(w);
    audit_job *j = dxl_xmalloc(sizeof *j);
    memset(j, 0, sizeof *j);
    j->lock = SDL_CreateMutex();
    j->event = w->job_event;
    atomic_init(&j->stop, 0);
    size_t n = 0;
    while (argv[n]) n++;
    j->argv = dxl_xmalloc((n + 1) * sizeof *j->argv);
    for (size_t i = 0; i < n; i++) j->argv[i] = dxl_xstrdup(argv[i]);
    j->argv[n] = NULL;
    SDL_SetAtomicInt(&j->refs, 2);
    SDL_Thread *t = j->lock ? SDL_CreateThread(audit_main, "dxl-audit", j) : NULL;
    if (!t) {
        dxl_log("no thread for the mod's audit (%s): it is run now", SDL_GetError());
        SDL_SetAtomicInt(&j->refs, 1);
        audit_unref(j);
        return -1;
    }
    SDL_DetachThread(t);
    w->audit = j;
    return 0;
}

/* The audit's answer, to Mods. */
static void audit_collect(dxl_window *w) {
    audit_job *j = w->audit;
    if (!j) return;
    SDL_LockMutex(j->lock);
    int done = j->done, ok = j->ok;
    dxl_mod_natives n = j->natives;
    SDL_UnlockMutex(j->lock);
    if (!done) return;
    audit_drop(w);
    dxl_shell_audited(&w->shell, ok ? &n : NULL);
}

/* ---- the app menu's entry ------------------------------------------------- */

static int write_entry(const char *data_home, const char *data_dir, int install, dxl_err *err) {
    if (!install) return dxl_desktop_remove(data_home, err);
    for (int i = 0; i < dxl_desktop_icon_count; i++) {
        char *icon = dxl_desktop_icon_path(data_home, dxl_desktop_icon_sizes[i]);
        int rc = dxl_icon_save(icon, dxl_desktop_icon_sizes[i]);
        if (rc != 0) dxl_err_set(err, "cannot write %s: %s", icon, SDL_GetError());
        free(icon);
        if (rc != 0) return -1;
    }
    char *tmpl = dxl_path_join(data_dir, DXL_APP_ID ".desktop.in");
    char *self = dxl_path_self();
    int rc = dxl_desktop_install(data_home, tmpl, self ? self : "deusex-launcher", err);
    free(self);
    free(tmpl);
    return rc;
}

/* The app menu's entry: none, this program's, or another's (platform/desktop.h). */
static int entry_state(const char *data_home) {
    char *self = dxl_path_self();
    int state = dxl_desktop_installed(data_home, self ? self : "deusex-launcher");
    free(self);
    return state;
}

static void desktop(dxl_shell *s, int install) {
    const dxl_session *ss = &s->run->session;
    dxl_err e;
    if (write_entry(ss->xdg.data_home, ss->data_dir, install, &e) == 0) {
        dxl_log("the entry %s %s", install ? "added to" : "taken off", DXL_DESKTOP_MENU);
        dxl_shell_toast(s, install ? "DXLauncher is on " DXL_DESKTOP_MENU "." : "DXLauncher is off " DXL_DESKTOP_MENU ".");
    } else {
        dxl_shell_message(s, install ? "Not on " DXL_DESKTOP_MENU : "Still on " DXL_DESKTOP_MENU, "%s", dxl_err_msg(&e));
    }
    s->desktop_entry = entry_state(ss->xdg.data_home);
}

int dxl_window_desktop_entry(int install) {
    dxl_xdg x;
    dxl_err e;
    if (dxl_xdg_open(&x, &e) != 0) {
        fprintf(stderr, "%s\n", dxl_err_msg(&e));
        return 1;
    }
    char *self = dxl_path_self();
    char *app = self ? dxl_path_dirname(self) : dxl_xstrdup(".");
    char *data = dxl_path_join(app, "data");
    int rc = write_entry(x.data_home, data, install, &e);
    char *entry = dxl_desktop_entry_path(x.data_home);
    if (rc == 0) printf("%s %s\n", install ? "wrote" : "removed", entry);
    else fprintf(stderr, "%s\n", dxl_err_msg(&e));
    free(entry);
    free(data);
    free(app);
    free(self);
    dxl_xdg_free(&x);
    return rc == 0 ? 0 : 1;
}

/* ---- the window ---------------------------------------------------------- */

/* The window's points to a dp: the display's scale and the launcher's own
 * size, the kit's px per dp, as its pixels per point. */
static float points_per_dp(dxl_window *w, float user_scale) {
    float scale = SDL_GetWindowDisplayScale(w->window), density = SDL_GetWindowPixelDensity(w->window);
    if (scale <= 0) scale = 1;
    if (density <= 0) density = 1;
    return scale / density * (user_scale > 0 ? user_scale : 1);
}

/* Its smallest, 960 x 600 dp at the launcher's size, within the display's
 * work area and the window's frame round it: where that does not fit -- a
 * short screen, a large display scale or launcher size -- the dp made
 * smaller, the kit's scale held to it (bugs.md LUI.15). The frame is what
 * SDL knows of it: Windows' at once, X11's once the window is mapped,
 * none on Wayland, where the compositor places the window. */
static void minimum(dxl_window *w, float user_scale, int *minw, int *minh) {
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(w->window), &usable))
        usable = (SDL_Rect){ 0, 0, 1920, 1080 };
    int top = 0, left = 0, bottom = 0, right = 0;
    if (!SDL_GetWindowBordersSize(w->window, &top, &left, &bottom, &right)) top = left = bottom = right = 0;
    w->room_w = usable.w - left - right;
    w->room_h = usable.h - top - bottom;
    float k = points_per_dp(w, user_scale);
    float fit = dxl_ui_fit_scale(k, (float)w->room_w, (float)w->room_h, SMALLEST_W, SMALLEST_H);
    float density = SDL_GetWindowPixelDensity(w->window);
    w->fit = fit < k ? fit * (density > 0 ? density : 1) : 0;
    *minw = (int)(SMALLEST_W * fit);
    *minh = (int)(SMALLEST_H * fit);
    SDL_SetWindowMinimumSize(w->window, *minw, *minh);
    w->min_scale = user_scale;
    w->min_k = fit;
}

/* On a display of another scale or another work area: the smallest set
 * again, and a window not maximized nor full screen kept at its size in dp
 * where its points per dp changed -- Windows keeps its pixels, which at
 * 100% from 200% would be twice the size -- within its smallest and the
 * room (bugs.md LUI.16). */
static void refit(dxl_window *w, float user_scale) {
    float was = w->min_k;
    int minw, minh;
    minimum(w, user_scale, &minw, &minh);
    if (w->big || (SDL_GetWindowFlags(w->window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))) return;
    int ww = 0, wh = 0;
    SDL_GetWindowSize(w->window, &ww, &wh);
    int nw = ww, nh = wh;
    if (was > 0 && (w->min_k > was * 1.001f || w->min_k < was * 0.999f)) {
        nw = (int)((float)ww * w->min_k / was + 0.5f);
        nh = (int)((float)wh * w->min_k / was + 0.5f);
    }
    if (nw > w->room_w) nw = w->room_w;
    if (nh > w->room_h) nh = w->room_h;
    if (nw < minw) nw = minw;
    if (nh < minh) nh = minh;
    if (nw != ww || nh != wh) {
        dxl_log("window: %dx%d for this display, from %dx%d", nw, nh, ww, wh);
        /* About its centre, which keeps it on the display that holds most
         * of it: sized from its corner, a window astride two displays of
         * other scales could cross back and be sized again each frame. */
        int x = 0, y = 0;
        int placed = SDL_GetWindowPosition(w->window, &x, &y);
        SDL_SetWindowSize(w->window, nw, nh);
        if (placed) SDL_SetWindowPosition(w->window, x + (ww - nw) / 2, y + (wh - nh) / 2);
    }
}

/* Its first size: the layout's 1280 x 720 dp, or what it had when it last
 * closed (launcher.ini's [Window]), within the room the display's work area
 * leaves, never under its smallest, which fits it. */
static void size_window(dxl_window *w) {
    SDL_DisplayID d = SDL_GetDisplayForWindow(w->window);
    const char *ui_scale = dxl_ini_get(w->run->session.ini, "Launcher", "UIScale");
    float user_scale = ui_scale ? (float)dxl_strtod_c(ui_scale, NULL) : 1;
    if (user_scale < 0.5f || user_scale > 3) user_scale = 1;
    int minw, minh;
    minimum(w, user_scale, &minw, &minh);
    float k = w->min_k;
    int maxw = (int)((float)w->room_w * 0.94f), maxh = (int)((float)w->room_h * 0.94f);
    int ww = (int)(FIRST_W * k), wh = (int)(FIRST_H * k);
    const dxl_ini *ini = w->run->session.ini;
    int kw = dxl_ini_get_int(ini, "Window", "Width", 0), kh = dxl_ini_get_int(ini, "Window", "Height", 0);
    if (kw > 0 && kh > 0) {
        ww = kw;
        wh = kh;
    }
    if (ww > maxw) ww = maxw;
    if (wh > maxh) wh = maxh;
    if (ww < minw) ww = minw;
    if (wh < minh) wh = minh;
    SDL_SetWindowSize(w->window, ww, wh);
    SDL_SetWindowPosition(w->window, SDL_WINDOWPOS_CENTERED_DISPLAY(d), SDL_WINDOWPOS_CENTERED_DISPLAY(d));
    if (dxl_ini_get_bool(ini, "Window", "Maximized", 0)) SDL_MaximizeWindow(w->window);
}

/* The window's size kept in launcher.ini's [Window], once there is a
 * profile: a first run left before its end writes nothing. */
static void close_window(dxl_window *w, dxl_run *r) {
    if (!w->window) return;
    if (r && dxl_profile_exists(&r->session.profile)) {
        dxl_ini *ini = r->session.ini;
        /* A size full screen -- the big screen's, or the desktop's own -- is
         * the screen's, not the window's. */
        SDL_WindowFlags flags = SDL_GetWindowFlags(w->window);
        int max = (flags & SDL_WINDOW_MAXIMIZED) != 0, full = w->big || (flags & SDL_WINDOW_FULLSCREEN);
        if (!full) dxl_ini_set_bool(ini, "Window", "Maximized", max);
        if (!max && !full) {
            int ww = 0, wh = 0;
            SDL_GetWindowSize(w->window, &ww, &wh);
            dxl_ini_set_int(ini, "Window", "Width", ww);
            dxl_ini_set_int(ini, "Window", "Height", wh);
        }
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
    /* And the folder browser's read, left to end on its own. */
    list_drop(w);
    /* And a mod's audit, its engine stopped: asked again as Mods comes
     * again. */
    audit_drop(w);
    free(w->shell.auditing);
    w->shell.auditing = NULL;
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

/* The big screen on or off: the window full screen, the kit's scale the
 * screen's -- as the desktop made it, not as it was asked. */
static void set_big(dxl_shell *s, int on) {
    dxl_window *w = s->window;
    if (!SDL_SetWindowFullscreen(w->window, on != 0)) {
        dxl_log("full screen %s: %s", on ? "on" : "off", SDL_GetError());
        return;
    }
    w->big = s->big = full_screen(w);
    dxl_ui_set_big(&s->ui, w->big);
}

/* A window that does not open: why, on stderr and in launcher.log, and
 * kept for the run's stop (stopped). */
static int no_window(dxl_window *w, const char *what) {
    snprintf(w->failed, sizeof w->failed, "DXLauncher %s: %s", what, SDL_GetError());
    fprintf(stderr, "%s\n", w->failed);
    dxl_log("%s", w->failed);
    return -1;
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
        /* In a rootless container SDL's udev enumeration never hears of a
         * pad plugged in later: it watches /dev/input itself, as it does
         * in a Flatpak or a Snap. */
        if (dxl_os_host_root_unmapped()) SDL_SetHint("SDL_JOYSTICK_DISABLE_UDEV", "1");
        if (!SDL_Init(SDL_INIT_VIDEO)) return no_window(w, "has no window");
        if (!TTF_Init()) {
            no_window(w, "has no fonts");
            SDL_Quit();
            return -1;
        }
        w->wake_event = SDL_RegisterEvents(3);
        w->pick_event = w->wake_event + 1;
        w->job_event = w->wake_event + 2;
        w->sdl = 1;
    }
    /* With the controllers the mapping wizard made known. */
    if (!w->pads) w->pads = dxl_pad_mappings(r, 1);
    w->window = SDL_CreateWindow("DXLauncher", FIRST_W, FIRST_H,
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (!w->window) return no_window(w, "has no window");
    size_window(w);
    if (!w->big_decided) {
        const char *why = NULL;
        w->big = dxl_big_screen_wanted(dxl_big_setting_of(dxl_ini_get(r->session.ini, "Launcher", "BigScreen")),
                                       r->opt.big_screen, &why);
        w->big_decided = 1;
        if (w->big) dxl_log("the big screen, by %s", why);
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
        no_window(w, "has no renderer");
        SDL_DestroyWindow(w->window);
        w->window = NULL;
        return -1;
    }
    w->vsync = SDL_SetRenderVSync(w->renderer, 1);
    char fonts[1100];
    snprintf(fonts, sizeof fonts, "%s/fonts", r->session.data_dir);
    const char *scale = dxl_ini_get(r->session.ini, "Launcher", "UIScale");
    float user_scale = scale ? (float)dxl_strtod_c(scale, NULL) : 1;
    if (user_scale < 0.5f || user_scale > 3) user_scale = 1;
    int rc;
    if (w->kit) {
        s->ui.user_scale = user_scale;
        s->ui.big = w->big;
        rc = dxl_ui_attach(&s->ui, w->window, w->renderer);
    } else {
        rc = dxl_ui_init(&s->ui, w->window, w->renderer, fonts, user_scale);
        if (rc == 0) dxl_ui_set_big(&s->ui, w->big);
    }
    s->big = w->big;
    s->ui.reduced_motion = dxl_ini_get_bool(r->session.ini, "Launcher", "ReducedMotion", 0);
    if (rc != 0) {
        no_window(w, "cannot draw");
        dxl_ui_detach(&s->ui);
        SDL_DestroyRenderer(w->renderer);
        SDL_DestroyWindow(w->window);
        w->renderer = NULL;
        w->window = NULL;
        return -1;
    }
    w->kit = 1;
    w->failed[0] = '\0';
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
    s->pick_folder = pick_folder;
    s->start_list = start_list;
    s->start_audit = start_audit;
    s->desktop = desktop;
    s->start_probe = start_probe;
    s->start_find = start_find;
    s->start_browse = start_browse;
    s->start_make = start_make;
    s->cancel_make = cancel_make;
    s->set_big = set_big;
    s->desktop_entry = entry_state(r->session.xdg.data_home);
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
    } else if (e->type == SDL_EVENT_WINDOW_DISPLAY_CHANGED || e->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
        w->min_stale = 1;
    } else if (e->type == w->wake_event) {
        take_lines(w);
    } else if (e->type == w->pick_event) {
        picked(w);
    } else if (e->type == w->job_event) {
        probe_collect(w);
        find_collect(w);
        browse_collect(w);
        make_collect(w);
        list_collect(w);
        audit_collect(w);
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
    if (open_window(w, r) != 0) return DXL_VIEW_UNANSWERED;
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
        /* The launcher's size changed on Settings: its smallest with it;
         * the window on another display, its size too. */
        if (w->min_stale) {
            refit(w, s->ui.user_scale);
            w->min_stale = 0;
        } else if (s->ui.user_scale != w->min_scale) {
            int minw, minh;
            minimum(w, s->ui.user_scale, &minw, &minh);
        }
        s->ui.fit_scale = w->fit;
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
    make_stop(w);
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
    /* A signal's stop is no quit of the player's. */
    int stopped = !s->result && w->stop && *w->stop;
    dxl_log("%s: %s", view_name(which), s->result ? "play" : stopped ? "stopped" : "quit");
    /* A quit is the run's end: the window goes now, while the run is. */
    if (!s->result) close_window(w, r);
    w->run = NULL;
    return stopped ? DXL_VIEW_UNANSWERED : s->result;
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

dxl_window *dxl_window_new(const char *version, atomic_int *stop) {
    dxl_window *w = dxl_xmalloc(sizeof *w);
    memset(w, 0, sizeof *w);
    w->version = version;
    w->stop = stop;
    dxl_shell_init(&w->shell, version);
    w->pick_lock = SDL_CreateMutex();
    w->script = dxl_uiscript_open();
    atomic_init(&w->probe_stop, 0);
    if (!signal_wake) signal_wake = dxl_wake_new();
    return w;
}

void dxl_window_free(dxl_window *w) {
    if (!w) return;
    listen_stop(w);
    /* A probe still on is given up: no one is left to take its answer, and
     * a stop waits for it no more than a tenth of a second. */
    if (w->probe_thread) {
        atomic_store(&w->probe_stop, 1);
        SDL_WaitThread(w->probe_thread, NULL);
    }
    free(w->cli);
    close_window(w, NULL);
    if (w->kit) dxl_ui_free(&w->shell.ui);
    dxl_shell_free(&w->shell);
    if (w->sdl) {
        TTF_Quit();
        SDL_Quit();
    }
    SDL_DestroyMutex(w->pick_lock);
    dxl_uiscript_end(w->script);
    dxl_uiscript_free(w->script);
    free(w->words_before);
    free(w->picked);
    free(w);
}

/* The run stopped short: why, in a box of the system's, which needs
 * neither the fonts nor a renderer of the launcher's, for a player who
 * started it from the desktop and has no terminal to read; with the reason
 * the window did not open, where it did not. None for a UI script's run,
 * which has no one to answer it. */
static void stopped(void *ctx, const char *message) {
    dxl_window *w = ctx;
    if (w->script) return;
    char text[900];
    snprintf(text, sizeof text, "%s%s%s", message, w->failed[0] ? "\n\n" : "", w->failed);
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "DXLauncher", text, NULL))
        dxl_log("no message box: %s", SDL_GetError());
}

void dxl_window_ui(dxl_window *w, dxl_run_ui *ui) {
    ui->ctx = w;
    ui->view = view;
    ui->before_play = before_play;
    ui->after_play = after_play;
    ui->stopped = stopped;
}
