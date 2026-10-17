#define _GNU_SOURCE
#include "screens/window.h"
#include "screens/shell.h"
#include "screens/uiscript.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/desktop.h"
#include "platform/process.h"
#include "ui/icon.h"
#include "ui/text.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* The window at first, and at its smallest, in dp. */
#define FIRST_W    1280
#define FIRST_H    720
#define SMALLEST_W 960
#define SMALLEST_H 600
/* A pad there as the window opens is no news. */
#define PADS_SETTLE_S 1.5
#define FRAME_NS 16666666ull

struct dxl_window {
    dxl_shell      shell;
    const char    *version;
    volatile int  *stop;
    dxl_run       *run;
    SDL_Window    *window;
    SDL_Renderer  *renderer;
    int            vsync;
    int            sdl, pads, kit;
    Uint32         wake_event, pick_event;
    double         opened;
    int            cursor_hidden;
    int            diags;          /* the kit's diagnostics last logged */
    dxl_uiscript  *script;         /* DXL_UI_SCRIPT's input, or NULL */
    char          *words_before;   /* the command line as it was before a run's own words */

    /* The thread that waits on a second start's line and on signals. */
    SDL_Thread    *listener;
    int            quit_pipe[2];
    SDL_Semaphore *drained;

    /* The GPU probe, dxl-cli --probe --ini in a worker. */
    SDL_Thread    *probe_thread;
    SDL_AtomicInt  probe_done;
    dxl_gpu_probe  probe_result;
    char          *cli;
    Uint32         job_event;

    /* The system's folder dialog's answer, from its thread. */
    SDL_Mutex     *pick_lock;
    int            pick_state;     /* 1 a folder; 2 none chosen; 3 no dialog to show */
    char          *picked;
    char           pick_error[256];
};

static int signal_pipe[2] = { -1, -1 };

void dxl_window_wake(void) {
    if (signal_pipe[1] >= 0) {
        char c = 's';
        ssize_t n = write(signal_pipe[1], &c, 1);
        (void)n;
    }
}

static double now_s(void) { return (double)SDL_GetTicksNS() / 1e9; }

/* ---- the listener ------------------------------------------------------ */

static int SDLCALL listen_main(void *p) {
    dxl_window *w = p;
    int fd = w->run ? dxl_instance_fd(w->run->instance) : -1;
    for (;;) {
        struct pollfd fds[3] = {
            { w->quit_pipe[0], POLLIN, 0 }, { signal_pipe[0], POLLIN, 0 }, { fd, POLLIN, 0 },
        };
        if (poll(fds, fd >= 0 ? 3 : 2, -1) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (fds[0].revents) break;
        if (fds[1].revents & POLLIN) {
            char buf[16];
            while (read(signal_pipe[0], buf, sizeof buf) > 0) {}
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&e);
        }
        if (fd >= 0 && (fds[2].revents & POLLIN)) {
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
    if (w->listener || pipe2(w->quit_pipe, O_CLOEXEC) != 0) return;
    w->drained = SDL_CreateSemaphore(0);
    w->listener = SDL_CreateThread(listen_main, "dxl-listen", w);
    if (!w->listener) dxl_log("no listener thread: %s", SDL_GetError());
}

static void listen_stop(dxl_window *w) {
    if (!w->listener) return;
    char c = 'q';
    ssize_t n = write(w->quit_pipe[1], &c, 1);
    (void)n;
    SDL_SignalSemaphore(w->drained);
    SDL_WaitThread(w->listener, NULL);
    w->listener = NULL;
    SDL_DestroySemaphore(w->drained);
    w->drained = NULL;
    close(w->quit_pipe[0]);
    close(w->quit_pipe[1]);
}

/* A second start's command line: the window raised. */
static void take_lines(dxl_window *w) {
    char line[DXL_HANDOFF_MAX];
    while (w->run && dxl_instance_poll(w->run->instance, line, sizeof line)) {
        dxl_log("a second start's command line: %s", line);
        SDL_RestoreWindow(w->window);
        SDL_RaiseWindow(w->window);
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
    SDL_ShowOpenFolderDialog(on_pick, w, w->window, dir ? dir : getenv("HOME"), false);
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
    w->cli = dxl_path_join(w->run->session.app_dir, "dxl-cli");
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

static void desktop(dxl_shell *s, int install) {
    const dxl_session *ss = &s->run->session;
    dxl_err e;
    if (write_entry(ss->xdg.data_home, ss->data_dir, install, &e) == 0) {
        dxl_log("app menu entry %s", install ? "added" : "removed");
        dxl_shell_toast(s, install ? "DXLauncher is on the app menu." : "DXLauncher is off the app menu.");
    } else {
        dxl_shell_message(s, install ? "Not on the app menu" : "Still on the app menu", "%s", dxl_err_msg(&e));
    }
    s->desktop_entry = dxl_desktop_installed(ss->xdg.data_home);
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

/* Its first size: the layout's 1280 x 720 dp, or what it had when it last
 * closed (launcher.ini's [Window]), within the display. */
static void size_window(dxl_window *w) {
    SDL_DisplayID d = SDL_GetDisplayForWindow(w->window);
    float scale = SDL_GetWindowDisplayScale(w->window), density = SDL_GetWindowPixelDensity(w->window);
    if (scale <= 0) scale = 1;
    if (density <= 0) density = 1;
    float k = scale / density;   /* window points per dp */
    SDL_Rect usable;
    if (!SDL_GetDisplayUsableBounds(d, &usable)) usable = (SDL_Rect){ 0, 0, 1920, 1080 };
    int maxw = (int)(usable.w * 0.94f), maxh = (int)(usable.h * 0.94f);
    int minw = (int)(SMALLEST_W * k), minh = (int)(SMALLEST_H * k);
    if (minw > usable.w) minw = usable.w;
    if (minh > usable.h) minh = usable.h;
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
    SDL_SetWindowMinimumSize(w->window, minw, minh);
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
        int max = (SDL_GetWindowFlags(w->window) & SDL_WINDOW_MAXIMIZED) != 0;
        dxl_ini_set_bool(ini, "Window", "Maximized", max);
        if (!max) {
            int ww = 0, wh = 0;
            SDL_GetWindowSize(w->window, &ww, &wh);
            dxl_ini_set_int(ini, "Window", "Width", ww);
            dxl_ini_set_int(ini, "Window", "Height", wh);
        }
        dxl_err e;
        if (dxl_session_save(&r->session, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    }
    dxl_art_free(&w->shell.art);
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
        w->wake_event = SDL_RegisterEvents(3);
        w->pick_event = w->wake_event + 1;
        w->job_event = w->wake_event + 2;
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
    size_window(w);
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
    const char *scale = dxl_ini_get(r->session.ini, "Launcher", "UIScale");
    float user_scale = scale ? (float)atof(scale) : 1;
    if (user_scale < 0.5f || user_scale > 3) user_scale = 1;
    int rc;
    if (w->kit) {
        s->ui.user_scale = user_scale;
        rc = dxl_ui_attach(&s->ui, w->window, w->renderer);
    } else {
        rc = dxl_ui_init(&s->ui, w->window, w->renderer, fonts, user_scale);
    }
    s->ui.reduced_motion = dxl_ini_get_bool(r->session.ini, "Launcher", "ReducedMotion", 0);
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
    w->opened = now_s();
    s->window = w;
    s->pick_folder = pick_folder;
    s->desktop = desktop;
    s->start_probe = start_probe;
    s->desktop_entry = dxl_desktop_installed(r->session.xdg.data_home);
    return 0;
}

static void handle(dxl_window *w, const SDL_Event *e) {
    dxl_shell *s = &w->shell;
    dxl_ui_event(&s->ui, e);
    if (e->type == SDL_EVENT_GAMEPAD_ADDED) {
        if (now_s() - w->opened > PADS_SETTLE_S) {
            const char *name = SDL_GetGamepadNameForID(e->gdevice.which);
            dxl_shell_toast(s, "A controller is connected: %s.", name ? name : "a pad");
        }
    } else if (e->type == SDL_EVENT_GAMEPAD_REMOVED) {
        dxl_shell_toast(s, "A controller was disconnected.");
    } else if (e->type == w->wake_event) {
        take_lines(w);
    } else if (e->type == w->pick_event) {
        picked(w);
    } else if (e->type == w->job_event) {
        probe_collect(w);
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
    w->quit_pipe[0] = w->quit_pipe[1] = -1;
    dxl_shell_init(&w->shell, version);
    w->pick_lock = SDL_CreateMutex();
    w->script = dxl_uiscript_open();
    if (signal_pipe[0] < 0 && pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) != 0) signal_pipe[0] = signal_pipe[1] = -1;
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
    SDL_DestroyMutex(w->pick_lock);
    dxl_uiscript_free(w->script);
    free(w->words_before);
    free(w->picked);
    free(w);
}

void dxl_window_ui(dxl_window *w, dxl_run_ui *ui) {
    ui->ctx = w;
    ui->view = view;
    ui->before_play = before_play;
    ui->after_play = after_play;
}
