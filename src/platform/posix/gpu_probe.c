#define _GNU_SOURCE
#include "platform/gpu_probe.h"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <EGL/egl.h>

#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void *open_first(const char *const *names) {
    for (const char *const *n = names; *n; n++) {
        void *h = dlopen(*n, RTLD_NOW | RTLD_LOCAL);
        if (h) return h;
    }
    return NULL;
}

/* ---- Vulkan ---------------------------------------------------------- */

static void probe_vulkan(dxl_gpu_probe *out) {
    static const char *const names[] = { "libvulkan.so.1", "libvulkan.so", NULL };
    void *lib = open_first(names);
    if (!lib) return;

    PFN_vkGetInstanceProcAddr gipa =
        (PFN_vkGetInstanceProcAddr)dlsym(lib, "vkGetInstanceProcAddr");
    if (!gipa) { dlclose(lib); return; }
    PFN_vkCreateInstance create =
        (PFN_vkCreateInstance)gipa(VK_NULL_HANDLE, "vkCreateInstance");
    if (!create) { dlclose(lib); return; }

    VkApplicationInfo app = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "deusex-launcher probe",
        .apiVersion = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app,
    };
    VkInstance inst = VK_NULL_HANDLE;
    if (create(&ci, NULL, &inst) != VK_SUCCESS || !inst) { dlclose(lib); return; }

    PFN_vkEnumeratePhysicalDevices enumerate =
        (PFN_vkEnumeratePhysicalDevices)gipa(inst, "vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties props =
        (PFN_vkGetPhysicalDeviceProperties)gipa(inst, "vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroy =
        (PFN_vkDestroyInstance)gipa(inst, "vkDestroyInstance");

    if (enumerate && props) {
        VkPhysicalDevice devs[8];
        uint32_t n = 8;
        VkResult rc = enumerate(inst, &n, devs);
        if ((rc == VK_SUCCESS || rc == VK_INCOMPLETE) && n > 0) {
            /* Prefer a real GPU over a CPU implementation (lavapipe on a
             * desktop), but report the CPU one if that is all there is. */
            uint32_t pick = 0;
            for (uint32_t i = 0; i < n; i++) {
                VkPhysicalDeviceProperties p;
                props(devs[i], &p);
                if (p.deviceType != VK_PHYSICAL_DEVICE_TYPE_CPU) { pick = i; break; }
            }
            VkPhysicalDeviceProperties p;
            props(devs[pick], &p);
            out->vulkan = 1;
            snprintf(out->vulkan_device, sizeof out->vulkan_device, "%.127s", p.deviceName);
            snprintf(out->vulkan_version, sizeof out->vulkan_version, "%u.%u.%u",
                     VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion),
                     VK_VERSION_PATCH(p.apiVersion));
        }
    }
    if (destroy) destroy(inst, NULL);
    dlclose(lib);
}

/* ---- EGL / OpenGL ES --------------------------------------------------- */

/* "OpenGL" as a whole word in EGL_CLIENT_APIS, as opposed to "OpenGL_ES". */
static int has_api_word(const char *apis, const char *word) {
    size_t n = strlen(word);
    for (const char *p = apis; p && (p = strstr(p, word)) != NULL; p += n) {
        int start = (p == apis || p[-1] == ' ');
        int end = (p[n] == '\0' || p[n] == ' ');
        if (start && end) return 1;
    }
    return 0;
}

#define DXL_GL_RENDERER 0x1F01
#define DXL_GL_VERSION  0x1F02
typedef const unsigned char *(*get_string_fn)(unsigned int);

static void probe_egl(dxl_gpu_probe *out) {
    static const char *const names[] = { "libEGL.so.1", "libEGL.so", NULL };
    void *lib = open_first(names);
    if (!lib) return;

/* Local pointers get a p_ prefix: the EGL header already declares the
     * real names as functions. */
#define LOAD(type, name) type p_##name = (type)dlsym(lib, #name)
    LOAD(PFNEGLGETDISPLAYPROC,     eglGetDisplay);
    LOAD(PFNEGLINITIALIZEPROC,     eglInitialize);
    LOAD(PFNEGLQUERYSTRINGPROC,    eglQueryString);
    LOAD(PFNEGLBINDAPIPROC,        eglBindAPI);
    LOAD(PFNEGLCHOOSECONFIGPROC,   eglChooseConfig);
    LOAD(PFNEGLCREATEPBUFFERSURFACEPROC, eglCreatePbufferSurface);
    LOAD(PFNEGLCREATECONTEXTPROC,  eglCreateContext);
    LOAD(PFNEGLMAKECURRENTPROC,    eglMakeCurrent);
    LOAD(PFNEGLDESTROYCONTEXTPROC, eglDestroyContext);
    LOAD(PFNEGLDESTROYSURFACEPROC, eglDestroySurface);
    LOAD(PFNEGLTERMINATEPROC,      eglTerminate);
    LOAD(PFNEGLGETPROCADDRESSPROC, eglGetProcAddress);
#undef LOAD
    if (!p_eglGetDisplay || !p_eglInitialize || !p_eglQueryString) { dlclose(lib); return; }

    EGLDisplay dpy = p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (dpy == EGL_NO_DISPLAY || !p_eglInitialize(dpy, &major, &minor)) { dlclose(lib); return; }

    const char *apis = p_eglQueryString(dpy, EGL_CLIENT_APIS);
    out->gles = has_api_word(apis, "OpenGL_ES");
    out->gl   = has_api_word(apis, "OpenGL");
    if (out->gles)
        snprintf(out->gles_version, sizeof out->gles_version, "EGL %d.%d", major, minor);

    /* A throwaway pbuffer context gets the real GL_VERSION ("OpenGL ES 3.2
     * build ..."), which is what decides whether a GLES 3 backend can run.
     * Not every driver offers pbuffers; the EGL-level answer stands if not. */
    if (out->gles && p_eglBindAPI && p_eglChooseConfig && p_eglCreatePbufferSurface &&
        p_eglCreateContext && p_eglMakeCurrent && p_eglGetProcAddress &&
        p_eglBindAPI(EGL_OPENGL_ES_API)) {
        static const EGLint es3_bit = 0x40;   /* EGL_OPENGL_ES3_BIT */
        const EGLint versions[] = { 3, 2 };
        for (int vi = 0; vi < 2; vi++) {
            EGLint cfg_attr[] = {
                EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                EGL_RENDERABLE_TYPE, versions[vi] == 3 ? es3_bit : EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };
            EGLConfig cfg;
            EGLint ncfg = 0;
            if (!p_eglChooseConfig(dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) continue;
            const EGLint pb_attr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
            EGLSurface surf = p_eglCreatePbufferSurface(dpy, cfg, pb_attr);
            if (surf == EGL_NO_SURFACE) continue;
            const EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, versions[vi], EGL_NONE };
            EGLContext ctx = p_eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
            if (ctx != EGL_NO_CONTEXT && p_eglMakeCurrent(dpy, surf, surf, ctx)) {
                /* EGL 1.4 only promises eglGetProcAddress for extensions, so
                 * fall back to the GLES library itself for a core entry point. */
                static const char *const gles_names[] = { "libGLESv2.so.2", "libGLESv2.so", NULL };
                get_string_fn get = (get_string_fn)p_eglGetProcAddress("glGetString");
                void *gles = NULL;
                if (!get && (gles = open_first(gles_names)) != NULL)
                    get = (get_string_fn)dlsym(gles, "glGetString");
                if (get) {
                    const char *v = (const char *)get(DXL_GL_VERSION);
                    const char *r = (const char *)get(DXL_GL_RENDERER);
                    if (v) snprintf(out->gles_version, sizeof out->gles_version, "%s", v);
                    if (r) snprintf(out->gles_renderer, sizeof out->gles_renderer, "%s", r);
                }
                p_eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (gles) dlclose(gles);
            }
            if (ctx != EGL_NO_CONTEXT && p_eglDestroyContext) p_eglDestroyContext(dpy, ctx);
            if (p_eglDestroySurface) p_eglDestroySurface(dpy, surf);
            if (out->gles_renderer[0]) break;
        }
    }
    if (p_eglTerminate) p_eglTerminate(dpy);
    dlclose(lib);
}

/* ---- the child/parent split ------------------------------------------ */

static long elapsed_ms(const struct timespec *t0) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - t0->tv_sec) * 1000L + (now.tv_nsec - t0->tv_nsec) / 1000000L;
}

int dxl_gpu_probe_run(dxl_gpu_probe *out, int timeout_ms) {
    memset(out, 0, sizeof *out);

    int fds[2];
    if (pipe(fds) != 0) {
        snprintf(out->note, sizeof out->note, "pipe: %s", strerror(errno));
        return -1;
    }
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(out->note, sizeof out->note, "fork: %s", strerror(errno));
        close(fds[0]); close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        close(fds[0]);
        /* Drivers log to stdout/stderr freely; keep that out of the
         * launcher's output, which the dry run prints to a terminal. */
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        dxl_gpu_probe r;
        memset(&r, 0, sizeof r);
        probe_vulkan(&r);
        probe_egl(&r);
        r.probed = 1;
        ssize_t w = write(fds[1], &r, sizeof r);
        (void)w;
        /* _exit, not exit: the parent's atexit handlers and stdio buffers are
         * not ours to run, and driver destructors have been known to hang. */
        _exit(0);
    }
    close(fds[1]);

    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    dxl_gpu_probe got;
    size_t have = 0;
    int ok = 0;
    for (;;) {
        long left = timeout_ms - elapsed_ms(&t0);
        if (left <= 0) break;
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        int pr = poll(&pfd, 1, (int)left);
        if (pr < 0 && errno == EINTR) continue;
        if (pr <= 0) break;
        ssize_t n = read(fds[0], (char *)&got + have, sizeof got - have);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;                      /* child died mid-write */
        have += (size_t)n;
        if (have == sizeof got) { ok = 1; break; }
    }
    close(fds[0]);

    int status = 0;
    if (!ok) kill(pid, SIGKILL);
    waitpid(pid, &status, 0);

    if (ok) {
        *out = got;
        return 0;
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) != SIGKILL)
        snprintf(out->note, sizeof out->note,
                 "detection crashed (signal %d) -- a graphics driver failed to initialise",
                 WTERMSIG(status));
    else
        snprintf(out->note, sizeof out->note,
                 "detection timed out after %d ms", timeout_ms);
    return -1;
}
