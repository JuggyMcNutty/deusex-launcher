/* The GPU probe on this machine's own drivers (platform/gpu_probe.h), in
 * its child: the GL renderer's two APIs offered only at the versions it
 * asks for, an OpenGL 4.2 core context and an OpenGL ES 3.2 one, each made
 * and its version read. Mesa's drivers are then lowered below both
 * (MESA_GL_VERSION_OVERRIDE 4.1, MESA_GLES_VERSION_OVERRIDE 3.1), as an
 * older GPU's are, and neither is offered. Skipped with no GPU's OpenGL ES
 * of Mesa's here -- no GPU, no EGL, or another maker's driver, which
 * Mesa's variables do not lower -- as the probe finds on any machine; a
 * probe that does not report back, a driver having crashed or hung it,
 * fails. */
#include "test.h"
#include "platform/gpu_probe.h"

TEST_MAIN_BEGIN
    dxl_gpu_probe p;
    int rc = dxl_gpu_probe_run(&p, 10000);
    if (rc != 0) fprintf(stderr, "  the probe did not report back: %s\n", p.note);
    CHECK_INT(rc, 0);
    if (rc != 0) return 1;
    if (!strstr(p.gles_version, "Mesa")) {
        printf("skipped: no GPU's OpenGL ES of Mesa's here\n");
        return 77;
    }
    CHECK_INT(p.gles, dxl_gl_version_at_least(p.gles_version, 3, 2));
    CHECK(!p.gl || dxl_gl_version_at_least(p.gl_version, 4, 2));
    setenv("MESA_GL_VERSION_OVERRIDE", "4.1", 1);
    setenv("MESA_GLES_VERSION_OVERRIDE", "3.1", 1);
    dxl_gpu_probe low;
    CHECK_INT(dxl_gpu_probe_run(&low, 10000), 0);
    CHECK(strstr(low.gles_version, "OpenGL ES 3.1") != NULL);
    CHECK_INT(low.gles, 0);
    CHECK_INT(low.gl, 0);
    CHECK_INT(low.vulkan, p.vulkan);
TEST_MAIN_END
