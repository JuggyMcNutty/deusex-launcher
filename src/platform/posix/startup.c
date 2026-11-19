#define _GNU_SOURCE
#include "platform/startup.h"

#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static atomic_int *stop_flag;
static void (*wake_up)(void);

static void on_signal(int sig) {
    (void)sig;
    if (stop_flag) *stop_flag = 1;
    if (wake_up) wake_up();
}

/* The compositor's globals, listed for the one that draws a window's frame
 * (xdg-decoration). */
static void on_global(void *data, void *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)registry;
    (void)name;
    (void)version;
    if (!strcmp(interface, "zxdg_decoration_manager_v1")) *(int *)data = 1;
}

static void on_global_remove(void *data, void *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

/* Whether the session's Wayland compositor draws a window's frame itself:
 * 1, 0, or -1 where it cannot be asked (no libwayland-client, which SDL
 * opens by this name too, or no compositor answering). One round trip. */
static int compositor_decorates(void) {
    void *lib = dlopen("libwayland-client.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (!lib) return -1;
    void *(*connect_to)(const char *) = (void *(*)(const char *))dlsym(lib, "wl_display_connect");
    void (*disconnect)(void *) = (void (*)(void *))dlsym(lib, "wl_display_disconnect");
    int (*roundtrip)(void *) = (int (*)(void *))dlsym(lib, "wl_display_roundtrip");
    void *(*constructor)(void *, uint32_t, const void *, ...) =
        (void *(*)(void *, uint32_t, const void *, ...))dlsym(lib, "wl_proxy_marshal_constructor");
    int (*add_listener)(void *, void (**)(void), void *) =
        (int (*)(void *, void (**)(void), void *))dlsym(lib, "wl_proxy_add_listener");
    void (*destroy)(void *) = (void (*)(void *))dlsym(lib, "wl_proxy_destroy");
    const void *registry_interface = dlsym(lib, "wl_registry_interface");
    int answer = -1;
    void *display = connect_to && disconnect && roundtrip && constructor && add_listener && destroy &&
                    registry_interface ? connect_to(NULL) : NULL;
    if (display) {
        /* wl_display's request 1, get_registry, and the registry's two
         * events, global and global_remove (the Wayland protocol). */
        static void (*listener[2])(void);
        listener[0] = (void (*)(void))on_global;
        listener[1] = (void (*)(void))on_global_remove;
        int found = 0;
        void *registry = constructor(display, 1, registry_interface, (void *)NULL);
        if (registry) {
            if (add_listener(registry, listener, &found) == 0 && roundtrip(display) >= 0) answer = found;
            destroy(registry);
        }
        disconnect(display);
    }
    dlclose(lib);
    return answer;
}

int dxl_startup_x11_first(const char *libdecor, int (*decorates)(void)) {
    const char *chosen = getenv("SDL_VIDEO_DRIVER"), *chosen2 = getenv("SDL_VIDEODRIVER"),
               *wayland = getenv("WAYLAND_DISPLAY"), *x11 = getenv("DISPLAY");
    /* SDL3 reads SDL2's name for the hint too. */
    if ((chosen && *chosen) || (chosen2 && *chosen2) || !wayland || !*wayland || !x11 || !*x11) return 0;
    void *h = dlopen(libdecor, RTLD_LAZY | RTLD_LOCAL);
    if (h) {
        dlclose(h);
        return 0;
    }
    return decorates() == 0;
}

void dxl_startup(atomic_int *stop, void (*wake)(void)) {
    stop_flag = stop;
    wake_up = wake;
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_signal;   /* no SA_RESTART: the wait ends at once */
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);
    /* A parent that ignores SIGCHLD passes that on across exec, and the
     * system then reaps the game itself: its end would never be seen. */
    signal(SIGCHLD, SIG_DFL);
    /* Before any thread, setenv being no thread's to race: the launcher's
     * SDL reads it as its hint, and the engine's SDL3 window inherits it. */
    if (dxl_startup_x11_first("libdecor-0.so.0", compositor_decorates))
        setenv("SDL_VIDEO_DRIVER", "x11,wayland", 0);
}

void dxl_startup_ended(void) {}
