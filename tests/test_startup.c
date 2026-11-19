/* What a start asks of SDL's video driver (platform/startup.h): X11 first
 * on a Wayland session whose compositor draws no window's frame, where
 * libdecor cannot be opened (bugs.md LPORT.18). */
#include "test.h"
#include "platform/os.h"
#include "platform/startup.h"

#define NO_LIBDECOR "libdxl-no-such-library.so.0"
#define A_LIBRARY   "libc.so.6"

static int asked;
static int frames(void) { asked++; return 1; }      /* KDE's, Sway's */
static int no_frames(void) { asked++; return 0; }   /* GNOME's */
static int no_answer(void) { asked++; return -1; }

static void session(const char *wayland, const char *x11, const char *driver, const char *driver2) {
    dxl_env_set("WAYLAND_DISPLAY", wayland);
    dxl_env_set("DISPLAY", x11);
    dxl_env_set("SDL_VIDEO_DRIVER", driver);
    dxl_env_set("SDL_VIDEODRIVER", driver2);
    asked = 0;
}

static void test_no_frame_without_libdecor_takes_x11(void) {
    session("wayland-0", ":0", NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 1);
}

/* A compositor that draws the frame needs no libdecor: Wayland kept. */
static void test_compositor_frames_keeps_wayland(void) {
    session("wayland-0", ":0", NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, frames), 0);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_answer), 0);
}

/* libdecor there: SDL draws the frame with it, and no compositor is asked. */
static void test_libdecor_there_keeps_wayland(void) {
    session("wayland-0", ":0", NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(A_LIBRARY, no_frames), 0);
    CHECK_INT(asked, 0);
}

static void test_no_x11_keeps_wayland(void) {
    session("wayland-0", NULL, NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 0);
    session("wayland-0", "", NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 0);
    CHECK_INT(asked, 0);
}

static void test_x11_session_untouched(void) {
    session(NULL, ":0", NULL, NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 0);
    CHECK_INT(asked, 0);
}

/* The player's own choice, by SDL3's name for it or SDL2's, is kept. */
static void test_a_chosen_driver_kept(void) {
    session("wayland-0", ":0", "wayland", NULL);
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 0);
    session("wayland-0", ":0", NULL, "wayland");
    CHECK_INT(dxl_startup_x11_first(NO_LIBDECOR, no_frames), 0);
    CHECK_INT(asked, 0);
}

TEST_MAIN_BEGIN
    RUN(test_no_frame_without_libdecor_takes_x11);
    RUN(test_compositor_frames_keeps_wayland);
    RUN(test_libdecor_there_keeps_wayland);
    RUN(test_no_x11_keeps_wayland);
    RUN(test_x11_session_untouched);
    RUN(test_a_chosen_driver_kept);
TEST_MAIN_END
