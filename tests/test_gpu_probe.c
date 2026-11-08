/* The GPU probe on this machine's own drivers (platform/gpu_probe.h), in
 * its child: the GL renderer's two APIs offered only at the versions it
 * asks for, an OpenGL 4.2 core context and an OpenGL ES 3.2 one, each made
 * and its version read. Held to what Mesa's drivers are made to offer, not
 * to the probe's own answers (bugs.md LAPP.13): at 4.2 and ES 3.2
 * (MESA_GL_VERSION_OVERRIDE, MESA_GLES_VERSION_OVERRIDE) both offered, a
 * context of each made at that version; lowered below them, 4.1 and ES 3.1,
 * as an older GPU's are, neither. Skipped where an OpenGL ES context this
 * test makes itself is no GPU's of Mesa's -- no GPU, no EGL, or another
 * maker's driver, which Mesa's variables do not move; a probe that does not
 * report back, a driver having crashed or hung it, fails. */
#include "test.h"
#include "platform/gpu_probe.h"

#include <EGL/egl.h>
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define GL_VERSION_STRING  0x1F02
#define GL_RENDERER_STRING 0x1F01

/* In a child, so that no driver is loaded here before a probe's fork: an
 * OpenGL ES 2 context of this machine's EGL, its GL_VERSION Mesa's and its
 * renderer no CPU's. */
static int mesa_gpu_gles(void) {
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        alarm(10);
        void *lib = dlopen("libEGL.so.1", RTLD_NOW);
        if (!lib) _exit(1);
        PFNEGLGETDISPLAYPROC get_display = (PFNEGLGETDISPLAYPROC)dlsym(lib, "eglGetDisplay");
        PFNEGLINITIALIZEPROC initialize = (PFNEGLINITIALIZEPROC)dlsym(lib, "eglInitialize");
        PFNEGLBINDAPIPROC bind_api = (PFNEGLBINDAPIPROC)dlsym(lib, "eglBindAPI");
        PFNEGLCHOOSECONFIGPROC choose_config = (PFNEGLCHOOSECONFIGPROC)dlsym(lib, "eglChooseConfig");
        PFNEGLCREATEPBUFFERSURFACEPROC pbuffer = (PFNEGLCREATEPBUFFERSURFACEPROC)dlsym(lib, "eglCreatePbufferSurface");
        PFNEGLCREATECONTEXTPROC create_context = (PFNEGLCREATECONTEXTPROC)dlsym(lib, "eglCreateContext");
        PFNEGLMAKECURRENTPROC make_current = (PFNEGLMAKECURRENTPROC)dlsym(lib, "eglMakeCurrent");
        PFNEGLGETPROCADDRESSPROC get_proc = (PFNEGLGETPROCADDRESSPROC)dlsym(lib, "eglGetProcAddress");
        if (!get_display || !initialize || !bind_api || !choose_config || !pbuffer || !create_context ||
            !make_current || !get_proc)
            _exit(1);
        EGLDisplay dpy = get_display(EGL_DEFAULT_DISPLAY);
        EGLint major, minor, n = 0;
        if (dpy == EGL_NO_DISPLAY || !initialize(dpy, &major, &minor) || !bind_api(EGL_OPENGL_ES_API)) _exit(1);
        static const EGLint want[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                       EGL_NONE };
        static const EGLint size[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
        static const EGLint es2[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        EGLConfig cfg;
        if (!choose_config(dpy, want, &cfg, 1, &n) || n < 1) _exit(1);
        EGLSurface surf = pbuffer(dpy, cfg, size);
        EGLContext ctx = create_context(dpy, cfg, EGL_NO_CONTEXT, es2);
        if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !make_current(dpy, surf, surf, ctx)) _exit(1);
        typedef const char *(*get_string_fn)(unsigned);
        get_string_fn get = (get_string_fn)get_proc("glGetString");
        void *gles = get ? NULL : dlopen("libGLESv2.so.2", RTLD_NOW);
        if (gles) get = (get_string_fn)dlsym(gles, "glGetString");
        const char *v = get ? get(GL_VERSION_STRING) : NULL, *r = get ? get(GL_RENDERER_STRING) : NULL;
        int mesa = v && strstr(v, "Mesa") && r && !strstr(r, "llvmpipe") && !strstr(r, "softpipe");
        _exit(mesa ? 0 : 1);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0)
        if (errno != EINTR) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* The probe with Mesa's versions at gl and gles. */
static int probe_at(dxl_gpu_probe *p, const char *gl, const char *gles) {
    setenv("MESA_GL_VERSION_OVERRIDE", gl, 1);
    setenv("MESA_GLES_VERSION_OVERRIDE", gles, 1);
    int rc = dxl_gpu_probe_run(p, 10000);
    if (rc != 0) fprintf(stderr, "  the probe at %s and ES %s did not report back: %s\n", gl, gles, p->note);
    return rc;
}

TEST_MAIN_BEGIN
    dxl_gpu_probe p;
    int rc = dxl_gpu_probe_run(&p, 10000);
    if (rc != 0) fprintf(stderr, "  the probe did not report back: %s\n", p.note);
    CHECK_INT(rc, 0);
    if (rc != 0) return 1;
    if (!mesa_gpu_gles()) {
        printf("skipped: no GPU's OpenGL ES of Mesa's here\n");
        return 77;
    }
    /* At the versions the GL renderer asks for: both offered. */
    dxl_gpu_probe at;
    CHECK_INT(probe_at(&at, "4.2", "3.2"), 0);
    if (!at.gles || !at.gl) fprintf(stderr, "  at 4.2 and ES 3.2: \"%s\", \"%s\"\n", at.gles_version, at.gl_version);
    CHECK_INT(at.gles, 1);
    CHECK(strstr(at.gles_version, "OpenGL ES 3.2") != NULL);
    CHECK_INT(at.gl, 1);
    CHECK(strncmp(at.gl_version, "4.2", 3) == 0);
    /* Below them: neither, the ES context made at its own version. */
    dxl_gpu_probe low;
    CHECK_INT(probe_at(&low, "4.1", "3.1"), 0);
    CHECK(strstr(low.gles_version, "OpenGL ES 3.1") != NULL);
    CHECK_INT(low.gles, 0);
    CHECK_INT(low.gl, 0);
    CHECK_INT(low.vulkan, p.vulkan);
TEST_MAIN_END
