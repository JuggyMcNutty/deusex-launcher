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
#include <stdlib.h>
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
            /* A GPU's device and a CPU implementation's (lavapipe) apart:
             * the first of each. */
            for (uint32_t i = 0; i < n; i++) {
                VkPhysicalDeviceProperties p;
                props(devs[i], &p);
                if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
                    if (out->vulkan_cpu) continue;
                    out->vulkan_cpu = 1;
                    snprintf(out->vulkan_cpu_device, sizeof out->vulkan_cpu_device, "%.127s", p.deviceName);
                } else if (!out->vulkan) {
                    out->vulkan = 1;
                    snprintf(out->vulkan_device, sizeof out->vulkan_device, "%.127s", p.deviceName);
                    snprintf(out->vulkan_version, sizeof out->vulkan_version, "%u.%u.%u",
                             VK_VERSION_MAJOR(p.apiVersion), VK_VERSION_MINOR(p.apiVersion),
                             VK_VERSION_PATCH(p.apiVersion));
                }
            }
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

/* EGL 1.5's, which a header of 1.4 lacks (EGL_KHR_create_context's). */
#ifndef EGL_CONTEXT_MAJOR_VERSION
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#endif
#ifndef EGL_CONTEXT_MINOR_VERSION
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#endif
#ifndef EGL_CONTEXT_OPENGL_PROFILE_MASK
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#endif
#ifndef EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001
#endif
#define DXL_EGL_OPENGL_ES3_BIT 0x40

/* Mesa's CPU renderers, as GL_RENDERER names them */
static int cpu_renderer(const char *renderer) {
    return renderer && (strstr(renderer, "llvmpipe") || strstr(renderer, "softpipe"));
}

typedef struct {
    PFNEGLBINDAPIPROC              bind_api;
    PFNEGLCHOOSECONFIGPROC         choose_config;
    PFNEGLCREATEPBUFFERSURFACEPROC create_pbuffer;
    PFNEGLCREATECONTEXTPROC        create_context;
    PFNEGLMAKECURRENTPROC          make_current;
    PFNEGLDESTROYCONTEXTPROC       destroy_context;
    PFNEGLDESTROYSURFACEPROC       destroy_surface;
    PFNEGLGETPROCADDRESSPROC       get_proc;
} egl_fns;

/* A throwaway pbuffer context of an API, of a config that renders it and
 * of the attributes asked for: its GL_VERSION and GL_RENDERER. 1 when one
 * was made current. Not every driver offers pbuffers, nor every version. */
static int context_strings(const egl_fns *f, EGLDisplay dpy, EGLenum api, EGLint renderable,
                           const EGLint *ctx_attr, const char *const *libs, char *version, size_t vsize,
                           char *renderer, size_t rsize) {
    if (!f->bind_api || !f->choose_config || !f->create_pbuffer || !f->create_context || !f->make_current ||
        !f->get_proc || !f->bind_api(api))
        return 0;
    const EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, renderable, EGL_NONE };
    EGLConfig cfg;
    EGLint ncfg = 0;
    if (!f->choose_config(dpy, cfg_attr, &cfg, 1, &ncfg) || ncfg < 1) return 0;
    const EGLint pb_attr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
    EGLSurface surf = f->create_pbuffer(dpy, cfg, pb_attr);
    if (surf == EGL_NO_SURFACE) return 0;
    int made = 0;
    EGLContext ctx = f->create_context(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (ctx != EGL_NO_CONTEXT && f->make_current(dpy, surf, surf, ctx)) {
        made = 1;
        /* EGL 1.4 only promises eglGetProcAddress for extensions, so fall
         * back to the API's library itself for a core entry point. */
        get_string_fn get = (get_string_fn)f->get_proc("glGetString");
        void *lib = NULL;
        if (!get && (lib = open_first(libs)) != NULL) get = (get_string_fn)dlsym(lib, "glGetString");
        if (get) {
            const char *v = (const char *)get(DXL_GL_VERSION);
            const char *r = (const char *)get(DXL_GL_RENDERER);
            if (v) snprintf(version, vsize, "%s", v);
            if (r) snprintf(renderer, rsize, "%s", r);
        }
        f->make_current(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (lib) dlclose(lib);
    }
    if (ctx != EGL_NO_CONTEXT && f->destroy_context) f->destroy_context(dpy, ctx);
    if (f->destroy_surface) f->destroy_surface(dpy, surf);
    return made;
}

/* OpenGL ES and desktop OpenGL as the engine's GL renderer asks SDL for
 * them, an ES 3.2 context and a 4.2 core one: each offered only with a
 * context made and its GL_VERSION at that, as Windows' probe asks for 4.2.
 * EGL_CLIENT_APIS alone names a driver of any version. */
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
    LOAD(PFNEGLTERMINATEPROC,      eglTerminate);
#undef LOAD
    if (!p_eglGetDisplay || !p_eglInitialize || !p_eglQueryString) { dlclose(lib); return; }
    egl_fns f = {
        (PFNEGLBINDAPIPROC)dlsym(lib, "eglBindAPI"),
        (PFNEGLCHOOSECONFIGPROC)dlsym(lib, "eglChooseConfig"),
        (PFNEGLCREATEPBUFFERSURFACEPROC)dlsym(lib, "eglCreatePbufferSurface"),
        (PFNEGLCREATECONTEXTPROC)dlsym(lib, "eglCreateContext"),
        (PFNEGLMAKECURRENTPROC)dlsym(lib, "eglMakeCurrent"),
        (PFNEGLDESTROYCONTEXTPROC)dlsym(lib, "eglDestroyContext"),
        (PFNEGLDESTROYSURFACEPROC)dlsym(lib, "eglDestroySurface"),
        (PFNEGLGETPROCADDRESSPROC)dlsym(lib, "eglGetProcAddress"),
    };

    EGLDisplay dpy = p_eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (dpy == EGL_NO_DISPLAY || !p_eglInitialize(dpy, &major, &minor)) { dlclose(lib); return; }

    const char *apis = p_eglQueryString(dpy, EGL_CLIENT_APIS);
    /* OpenGL ES: an ES 3 context, the highest 3.x the driver makes, else
     * an ES 2 one, for its GL_VERSION ("OpenGL ES 3.2 build ..."). */
    if (has_api_word(apis, "OpenGL_ES")) {
        static const char *const gles_libs[] = { "libGLESv2.so.2", "libGLESv2.so", NULL };
        static const EGLint es3[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        static const EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        snprintf(out->gles_version, sizeof out->gles_version, "EGL %d.%d", major, minor);
        if (!context_strings(&f, dpy, EGL_OPENGL_ES_API, DXL_EGL_OPENGL_ES3_BIT, es3, gles_libs, out->gles_version,
                             sizeof out->gles_version, out->gles_renderer, sizeof out->gles_renderer))
            context_strings(&f, dpy, EGL_OPENGL_ES_API, EGL_OPENGL_ES2_BIT, es2, gles_libs, out->gles_version,
                            sizeof out->gles_version, out->gles_renderer, sizeof out->gles_renderer);
        out->gles = dxl_gl_version_at_least(out->gles_version, 3, 2);
    }
    /* Desktop OpenGL: a 4.2 core context, or none. */
    if (has_api_word(apis, "OpenGL")) {
        static const char *const gl_libs[] = { "libOpenGL.so.0", "libGL.so.1", "libGL.so", NULL };
        static const EGLint core42[] = { EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 2,
                                         EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                         EGL_NONE };
        char renderer[128] = "";
        if (context_strings(&f, dpy, EGL_OPENGL_API, EGL_OPENGL_BIT, core42, gl_libs, out->gl_version,
                            sizeof out->gl_version, renderer, sizeof renderer))
            out->gl = dxl_gl_version_at_least(out->gl_version, 4, 2) && !cpu_renderer(renderer);
        if (!out->gl) out->gl_version[0] = '\0';
    }
    if (p_eglTerminate) p_eglTerminate(dpy);
    dlclose(lib);

    /* EGL's driver Mesa's CPU one, as where a GPU has no driver: no GPU's
     * GL, llvmpipe's. */
    if (cpu_renderer(out->gles_renderer)) {
        out->gl_cpu = 1;
        snprintf(out->gl_cpu_renderer, sizeof out->gl_cpu_renderer, "%s", out->gles_renderer);
        out->gles = out->gl = 0;
        out->gles_version[0] = out->gles_renderer[0] = out->gl_version[0] = '\0';
    }
}

/* Mesa's llvmpipe asked for (LIBGL_ALWAYS_SOFTWARE), where EGL's own driver
 * is a GPU's: in a child of its own, as the variable is read when the
 * driver loads. */
static void probe_egl_cpu(dxl_gpu_probe *out) {
    int fds[2];
    if (pipe(fds) != 0) return;
    pid_t pid = fork();
    if (pid < 0) { close(fds[0]); close(fds[1]); return; }
    if (pid == 0) {
        close(fds[0]);
        setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
        dxl_gpu_probe r;
        memset(&r, 0, sizeof r);
        probe_egl(&r);
        ssize_t w = write(fds[1], r.gl_cpu_renderer, sizeof r.gl_cpu_renderer);
        (void)w;
        _exit(0);
    }
    close(fds[1]);
    char renderer[sizeof out->gl_cpu_renderer];
    size_t have = 0;
    struct timespec t0;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long left = 3000 - ((now.tv_sec - t0.tv_sec) * 1000L + (now.tv_nsec - t0.tv_nsec) / 1000000L);
        if (left <= 0) break;
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        int pr = poll(&pfd, 1, (int)left);
        if (pr < 0 && errno == EINTR) continue;
        if (pr <= 0) break;
        ssize_t n = read(fds[0], renderer + have, sizeof renderer - have);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        have += (size_t)n;
        if (have == sizeof renderer) break;
    }
    close(fds[0]);
    if (have < sizeof renderer) kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    renderer[sizeof renderer - 1] = '\0';
    if (have == sizeof renderer && renderer[0]) {
        out->gl_cpu = 1;
        snprintf(out->gl_cpu_renderer, sizeof out->gl_cpu_renderer, "%s", renderer);
    }
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
        /* llvmpipe's first, its child forked before any driver is loaded
         * here: a driver's threads do not survive a fork */
        probe_egl_cpu(&r);
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
