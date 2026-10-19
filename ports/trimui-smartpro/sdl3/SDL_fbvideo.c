/* SPDX-License-Identifier: Zlib
 *
 * The Smart Pro's SDL3 video driver: /dev/fb0 and the PowerVR EGL, the
 * device's only display path (no DRM, no X11, no Wayland). Built into SDL3
 * as its private driver (sdl3.cmake), so SDL tries it first.
 *
 * One display, fb0's visible mode; every window is that size and full screen
 * (VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY). OpenGL ES draws through EGL on the
 * default display with a NULL native window: the only one the PowerVR EGL
 * takes, the swaps flipping fb0 between its first two pages
 * (tools/probes/probe-fbdev-egl.c, Port Ex Machina). The software renderer
 * draws through the framebuffer functions, copied into the page fb0 shows.
 *
 * There is no ShowWindow: SDL_ShowWindow then gives the window the keyboard
 * focus itself, without which SDL drops the pads' events. The pads are SDL's
 * own joystick driver's; this driver reads no input. The console's cursor is
 * hidden through /dev/tty0 while the video is up, and fb0's mode put back as
 * it was found when it goes. */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PRIVATE

#include "video/SDL_sysvideo.h"
#include "video/SDL_egl_c.h"
#include "SDL_properties_c.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define FB_SURFACE "SDL.internal.window.surface"

struct SDL_VideoData
{
    int fd;
    struct fb_var_screeninfo var0; // as found, put back at VideoQuit
    struct fb_fix_screeninfo fix;
    Uint8 *mem;
    size_t len;
};

struct SDL_WindowData
{
    EGLSurface egl_surface;
};

static void FB_Console(const char *seq)
{
    int tty = open("/dev/tty0", O_WRONLY | O_NOCTTY | O_CLOEXEC);
    if (tty >= 0) {
        ssize_t n = write(tty, seq, SDL_strlen(seq));
        (void)n;
        close(tty);
    }
}

static bool FB_VideoInit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    SDL_DisplayMode mode;

    data->fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (data->fd < 0) {
        return SDL_SetError("fbdev: /dev/fb0: %s", strerror(errno));
    }
    if (ioctl(data->fd, FBIOGET_VSCREENINFO, &data->var0) < 0 ||
        ioctl(data->fd, FBIOGET_FSCREENINFO, &data->fix) < 0) {
        close(data->fd);
        data->fd = -1;
        return SDL_SetError("fbdev: fb0's mode: %s", strerror(errno));
    }
    if (data->var0.bits_per_pixel != 32) {
        close(data->fd);
        data->fd = -1;
        return SDL_SetError("fbdev: fb0 is %u bits a pixel, not 32", data->var0.bits_per_pixel);
    }
    data->len = data->fix.smem_len;
    data->mem = mmap(NULL, data->len, PROT_READ | PROT_WRITE, MAP_SHARED, data->fd, 0);
    if (data->mem == MAP_FAILED) {
        data->mem = NULL; // EGL draws without it; the software renderer does not
    }

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.w = (int)data->var0.xres;
    mode.h = (int)data->var0.yres;
    mode.refresh_rate = 60.0f;
    if (SDL_AddBasicVideoDisplay(&mode) == 0) {
        return false;
    }
    FB_Console("\033[?25l");
    return true;
}

static void FB_VideoQuit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    if (data->fd < 0) {
        return;
    }
    // The virtual size and the page EGL left, back to what the menu had: a
    // mode put back does not move the page shown, a pan does.
    ioctl(data->fd, FBIOPUT_VSCREENINFO, &data->var0);
    ioctl(data->fd, FBIOPAN_DISPLAY, &data->var0);
    if (data->mem) {
        munmap(data->mem, data->len);
        data->mem = NULL;
    }
    close(data->fd);
    data->fd = -1;
    FB_Console("\033[?25h");
}

static bool FB_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_WindowData *wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(SDL_WindowData));
    if (!wdata) {
        return false;
    }
    window->internal = wdata;
    wdata->egl_surface = EGL_NO_SURFACE;

    // SDL has loaded EGL for an OpenGL window by now.
    if (window->flags & SDL_WINDOW_OPENGL) {
        wdata->egl_surface = SDL_EGL_CreateSurface(_this, window, (NativeWindowType)0);
        if (wdata->egl_surface == EGL_NO_SURFACE) {
            return SDL_SetError("fbdev: no EGL window surface");
        }
    }
    return true;
}

static void FB_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *wdata = window->internal;

    if (wdata) {
        if (wdata->egl_surface != EGL_NO_SURFACE) {
            SDL_EGL_DestroySurface(_this, wdata->egl_surface);
        }
        SDL_free(wdata);
    }
    window->internal = NULL;
}

// ---- the software renderer's path: a surface copied into fb0's shown page

static bool FB_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format,
                                       void **pixels, int *pitch)
{
    SDL_VideoData *data = _this->internal;
    SDL_Surface *surface;
    int w, h;

    if (!data->mem) {
        return SDL_SetError("fbdev: fb0 is not mapped");
    }
    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_XRGB8888);
    if (!surface) {
        return false;
    }
    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), FB_SURFACE, surface);
    *format = SDL_PIXELFORMAT_XRGB8888;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return true;
}

static bool FB_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_VideoData *data = _this->internal;
    SDL_Surface *surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), FB_SURFACE, NULL);
    struct fb_var_screeninfo var;
    int i, y;

    if (!surface || !data->mem) {
        return SDL_SetError("fbdev: no framebuffer for the window");
    }
    if (ioctl(data->fd, FBIOGET_VSCREENINFO, &var) < 0) {
        var = data->var0;
    }
    for (i = 0; i < numrects; i++) {
        SDL_Rect r;
        const SDL_Rect all = { 0, 0, surface->w, surface->h };
        if (!SDL_GetRectIntersection(&rects[i], &all, &r)) {
            continue;
        }
        if (r.x + r.w > (int)var.xres || r.y + r.h > (int)var.yres) {
            continue;
        }
        for (y = r.y; y < r.y + r.h; y++) {
            size_t off = (size_t)(var.yoffset + y) * data->fix.line_length + (size_t)(var.xoffset + r.x) * 4;
            if (off + (size_t)r.w * 4 > data->len) {
                break;
            }
            SDL_memcpy(data->mem + off, (Uint8 *)surface->pixels + (size_t)y * surface->pitch + (size_t)r.x * 4,
                       (size_t)r.w * 4);
        }
    }
    return true;
}

static void FB_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_ClearProperty(SDL_GetWindowProperties(window), FB_SURFACE);
}

// ---- OpenGL ES through EGL

static bool FB_GLES_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    return SDL_EGL_LoadLibrary(_this, path, EGL_DEFAULT_DISPLAY, 0);
}

static void FB_GLES_DefaultProfileConfig(SDL_VideoDevice *_this, int *mask, int *major, int *minor)
{
    *mask = SDL_GL_CONTEXT_PROFILE_ES;
    *major = 2;
    *minor = 0;
}

SDL_EGL_CreateContext_impl(FB)
SDL_EGL_SwapWindow_impl(FB)
SDL_EGL_MakeCurrent_impl(FB)

static void FB_PumpEvents(SDL_VideoDevice *_this)
{
}

static void FB_Destroy(SDL_VideoDevice *device)
{
    SDL_free(device->internal);
    SDL_free(device);
}

static SDL_VideoDevice *FB_Create(void)
{
    SDL_VideoDevice *device;
    SDL_VideoData *data;

    if (access("/dev/fb0", R_OK | W_OK) != 0) {
        return NULL;
    }
    device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    data = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!data) {
        SDL_free(device);
        return NULL;
    }
    data->fd = -1;
    device->internal = data;
    device->free = FB_Destroy;
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;

    device->VideoInit = FB_VideoInit;
    device->VideoQuit = FB_VideoQuit;
    device->CreateSDLWindow = FB_CreateWindow;
    device->DestroyWindow = FB_DestroyWindow;
    device->CreateWindowFramebuffer = FB_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = FB_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = FB_DestroyWindowFramebuffer;
    device->PumpEvents = FB_PumpEvents;

    device->GL_LoadLibrary = FB_GLES_LoadLibrary;
    device->GL_GetProcAddress = SDL_EGL_GetProcAddressInternal;
    device->GL_UnloadLibrary = SDL_EGL_UnloadLibrary;
    device->GL_CreateContext = FB_GLES_CreateContext;
    device->GL_MakeCurrent = FB_GLES_MakeCurrent;
    device->GL_SetSwapInterval = SDL_EGL_SetSwapInterval;
    device->GL_GetSwapInterval = SDL_EGL_GetSwapInterval;
    device->GL_SwapWindow = FB_GLES_SwapWindow;
    device->GL_DestroyContext = SDL_EGL_DestroyContext;
    device->GL_DefaultProfileConfig = FB_GLES_DefaultProfileConfig;

    return device;
}

VideoBootStrap PRIVATE_bootstrap = {
    "fbdev",
    "fb0 and the PowerVR EGL (the TrimUI Smart Pro's)",
    FB_Create,
    NULL, // no ShowMessageBox
    false
};

#endif // SDL_VIDEO_DRIVER_PRIVATE
