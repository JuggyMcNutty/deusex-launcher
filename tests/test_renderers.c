#include "test.h"
#include "core/renderers.h"
#include "platform/os.h"

#include <unistd.h>

/* A list where Vulkan works and the engine build lacks the rest: no GLES
 * backend, no software renderer. */
static const char *device_ini =
    "[Renderers]\n"
    "Renderer=Vulkan\n"
    "Renderer=GLES\n"
    "Renderer=Software\n"
    "\n"
    "[Vulkan]\n"
    "Label=Vulkan\n"
    "EngineType=Vulkan\n"
    "Requires=vulkan\n"
    "Description=The engine's main renderer.\n"
    "\n"
    "[GLES]\n"
    "Label=OpenGL ES\n"
    "EngineType=\n"
    "Requires=gles\n"
    "\n"
    "[Software]\n"
    "Label=Software\n"
    "Requires=none\n";

static dxl_renderer_list load(const char *text) {
    char path[256];
    snprintf(path, sizeof path, "%s/dxl-renderers-%ld.ini", dxl_test_tmp(), dxl_os_pid());
    FILE *f = fopen(path, "wb");
    fputs(text, f);
    fclose(f);
    dxl_renderer_list l;
    dxl_renderers_load(&l, path);
    remove(path);
    return l;
}

static dxl_gpu_probe smart_pro(void) {
    dxl_gpu_probe p;
    memset(&p, 0, sizeof p);
    p.probed = 1;
    p.vulkan = 1;
    snprintf(p.vulkan_device, sizeof p.vulkan_device, "PowerVR Rogue GE8300");
    snprintf(p.vulkan_version, sizeof p.vulkan_version, "1.3.225");
    p.gles = 1;
    snprintf(p.gles_version, sizeof p.gles_version, "OpenGL ES 3.2 build 1.17");
    return p;
}

static void test_loads_in_order(void) {
    dxl_renderer_list l = load(device_ini);
    CHECK_INT(l.count, 3);
    CHECK_STR(l.items[0].label, "Vulkan");
    CHECK_STR(l.items[1].label, "OpenGL ES");
    CHECK_STR(l.items[2].engine_type, "");
    CHECK_INT(l.items[0].requires, DXL_API_VULKAN);
    CHECK_INT(l.items[2].requires, DXL_API_NONE);
    dxl_renderers_free(&l);
}

/* Both facts shown: the device has GLES, the engine does not. */
static void test_resolve_on_the_handheld(void) {
    dxl_renderer_list l = load(device_ini);
    dxl_gpu_probe p = smart_pro();
    dxl_renderers_resolve(&l, &p);

    CHECK_INT(l.items[0].selectable, 1);
    CHECK(strstr(l.items[0].status, "PowerVR Rogue GE8300") != NULL);
    CHECK(strstr(l.items[0].status, "1.3.225") != NULL);

    CHECK_INT(l.items[1].selectable, 0);
    CHECK(strstr(l.items[1].status, "Not in this engine build") != NULL);
    CHECK(strstr(l.items[1].status, "OpenGL ES 3.2") != NULL);

    CHECK_INT(l.items[2].selectable, 0);
    CHECK_STR(l.items[2].status, "Not in this engine build");

    CHECK_INT(dxl_renderers_first_selectable(&l), 0);
    dxl_renderers_free(&l);
}

static void test_engine_has_it_device_does_not(void) {
    dxl_renderer_list l = load(device_ini);
    dxl_gpu_probe p = smart_pro();
    p.vulkan = 0;
    dxl_renderers_resolve(&l, &p);
    CHECK_INT(l.items[0].selectable, 0);
    CHECK(strstr(l.items[0].status, "no Vulkan driver") != NULL);
    CHECK_INT(dxl_renderers_first_selectable(&l), -1);
    dxl_renderers_free(&l);
}

/* Without a probe nothing is known to be missing: do not refuse on no
 * evidence, but do not claim a device either. */
static void test_unprobed_is_permissive_but_honest(void) {
    dxl_renderer_list l = load(device_ini);
    dxl_renderers_resolve(&l, NULL);
    CHECK_INT(l.items[0].selectable, 1);
    CHECK(strstr(l.items[0].status, "Not checked") != NULL);
    CHECK_INT(l.items[1].selectable, 0);     /* the engine still lacks it */
    dxl_renderers_free(&l);
}

/* Phase 4 flips one line of packaging: EngineType=GLES. */
static void test_gles_selectable_once_engine_has_it(void) {
    char *ini = strdup(device_ini);
    char *at = strstr(ini, "EngineType=\n");
    CHECK(at != NULL);
    dxl_renderer_list l;
    {
        char buf[2048];
        snprintf(buf, sizeof buf, "%.*sEngineType=GLES\n%s",
                 (int)(at - ini), ini, at + strlen("EngineType=\n"));
        l = load(buf);
    }
    dxl_gpu_probe p = smart_pro();
    dxl_renderers_resolve(&l, &p);
    CHECK_INT(l.items[1].selectable, 1);
    CHECK_INT(dxl_renderers_find(&l, "gles"), 1);
    dxl_renderers_free(&l);
    free(ini);
}

static void test_find_is_case_insensitive_and_skips_empty(void) {
    dxl_renderer_list l = load(device_ini);
    CHECK_INT(dxl_renderers_find(&l, "vulkan"), 0);
    CHECK_INT(dxl_renderers_find(&l, ""), -1);
    CHECK_INT(dxl_renderers_find(&l, NULL), -1);
    CHECK_INT(dxl_renderers_find(&l, "D3D11"), -1);
    dxl_renderers_free(&l);
}

static void test_missing_file_is_empty(void) {
    dxl_renderer_list l;
    dxl_renderers_load(&l, "/nonexistent/renderers.ini");
    CHECK_INT(l.count, 0);
    dxl_renderers_resolve(&l, NULL);
    CHECK_INT(dxl_renderers_first_selectable(&l), -1);
    dxl_renderers_free(&l);
}

/* The shipped list on a machine whose GPU has no driver: the software
 * renderer alone, which needs nothing of it, offered first. */
static void test_software_where_no_gpu(void) {
    dxl_renderer_list l;
    dxl_renderers_load(&l, DXL_RENDERERS);
    int soft = dxl_renderers_find(&l, "Software");
    CHECK(soft >= 0);
    CHECK_INT(l.items[soft].requires, DXL_API_NONE);
    dxl_gpu_probe p;
    memset(&p, 0, sizeof p);
    p.probed = 1;
    dxl_renderers_resolve(&l, &p);
    CHECK_INT(l.items[soft].selectable, 1);
    CHECK_STR(l.items[soft].status, "runs on the CPU");
    CHECK_INT(dxl_renderers_first_selectable(&l), soft);
    for (size_t i = 0; i < l.count; i++)
        if ((int)i != soft) CHECK_INT(l.items[i].selectable, 0);
    dxl_renderers_free(&l);
}

/* A machine with Mesa's CPU drivers alone: the GPU rows say so, the CPU
 * drivers' rows and the software renderer offered, Software first. */
static void test_cpu_drivers_alone(void) {
    dxl_renderer_list l;
    dxl_renderers_load(&l, DXL_RENDERERS);
    dxl_gpu_probe p;
    memset(&p, 0, sizeof p);
    p.probed = 1;
    p.vulkan_cpu = 1;
    snprintf(p.vulkan_cpu_device, sizeof p.vulkan_cpu_device, "llvmpipe (LLVM 23.1.1, 256 bits)");
    p.gl_cpu = 1;
    snprintf(p.gl_cpu_renderer, sizeof p.gl_cpu_renderer, "llvmpipe (LLVM 23.1.1, 256 bits)");
    dxl_renderers_resolve(&l, &p);
    int vk = dxl_renderers_find_value(&l, "Vulkan"), vkcpu = dxl_renderers_find_value(&l, "Vulkan+CPU");
    int gl = dxl_renderers_find_value(&l, "OpenGL"), glcpu = dxl_renderers_find_value(&l, "OpenGL+CPU");
    int soft = dxl_renderers_find_value(&l, "Software");
    CHECK(vk >= 0 && vkcpu >= 0 && gl >= 0 && glcpu >= 0 && soft >= 0);
    CHECK_INT(l.items[vk].selectable, 0);
    CHECK(strstr(l.items[vk].status, "lavapipe") != NULL);
    CHECK_INT(l.items[gl].selectable, 0);
    CHECK(strstr(l.items[gl].status, "llvmpipe") != NULL);
    CHECK_INT(l.items[vkcpu].selectable, 1);
    CHECK_INT(l.items[vkcpu].cpu_driver, 1);
    CHECK(strstr(l.items[vkcpu].status, "on the CPU") != NULL);
    CHECK_INT(l.items[glcpu].selectable, 1);
    CHECK_INT(dxl_renderers_first_selectable(&l), soft);
    char v[64];
    dxl_renderer_value(&l.items[glcpu], v, sizeof v);
    CHECK_STR(v, "OpenGL+CPU");
    CHECK_INT(dxl_renderers_find_value(&l, "vulkan+cpu"), vkcpu);
    CHECK_INT(dxl_renderers_find_value(&l, "D3D11"), -1);
    dxl_renderers_free(&l);
}

/* The GL renderer's versions, as the probe reads them off its contexts:
 * desktop GL's 4.2 and ES's 3.1, a driver below either refused. */
static void test_gl_versions(void) {
    CHECK_INT(dxl_gl_version_at_least("4.6 (Core Profile) Mesa 26.2.4", 4, 2), 1);
    CHECK_INT(dxl_gl_version_at_least("4.2.0 NVIDIA 470.256.02", 4, 2), 1);
    CHECK_INT(dxl_gl_version_at_least("4.1 (Core Profile) Mesa 21.0", 4, 2), 0);
    CHECK_INT(dxl_gl_version_at_least("3.3 (Core Profile) Mesa 22.3 (crocus)", 4, 2), 0);
    CHECK_INT(dxl_gl_version_at_least("OpenGL ES 3.2 build 1.17", 3, 1), 1);
    CHECK_INT(dxl_gl_version_at_least("OpenGL ES 3.1 Mesa 23.2 (V3D 4.2)", 3, 1), 1);
    CHECK_INT(dxl_gl_version_at_least("OpenGL ES 3.0 Mesa 20.3 (VC4 V3D 2.1)", 3, 1), 0);
    CHECK_INT(dxl_gl_version_at_least("OpenGL ES 2.0 Mesa", 3, 1), 0);
    CHECK_INT(dxl_gl_version_at_least("OpenGL ES-CM 1.1", 1, 0), 0);
    CHECK_INT(dxl_gl_version_at_least("EGL 1.5", 1, 0), 0);
    CHECK_INT(dxl_gl_version_at_least("", 1, 0), 0);
    CHECK_INT(dxl_gl_version_at_least(NULL, 1, 0), 0);
    /* Neither there: each row says the version it needs. */
    static const char *ini = "[Renderers]\nRenderer=GL\nRenderer=ES\n"
                             "[GL]\nLabel=OpenGL\nEngineType=OpenGL\nRequires=gl\n"
                             "[ES]\nLabel=OpenGL ES\nEngineType=GLES\nRequires=gles\n";
    dxl_renderer_list l = load(ini);
    dxl_gpu_probe p;
    memset(&p, 0, sizeof p);
    p.probed = 1;
    snprintf(p.gles_version, sizeof p.gles_version, "OpenGL ES 3.0 Mesa 20.3 (VC4 V3D 2.1)");
    dxl_renderers_resolve(&l, &p);
    CHECK_INT(l.items[0].selectable, 0);
    CHECK(strstr(l.items[0].status, "no OpenGL 4.2 driver") != NULL);
    CHECK_INT(l.items[1].selectable, 0);
    CHECK(strstr(l.items[1].status, "no OpenGL ES 3.1 driver") != NULL);
    p.gl = 1;
    snprintf(p.gl_version, sizeof p.gl_version, "4.6 (Core Profile) Mesa 26.2.4");
    dxl_renderers_resolve(&l, &p);
    CHECK_INT(l.items[0].selectable, 1);
    CHECK_STR(l.items[0].status, "OpenGL 4.6 (Core Profile) Mesa 26.2.4");
    dxl_renderers_free(&l);
}

/* The MSAA lock follows the GPU, not the device or the driver, and needs evidence. */
static void test_msaa_broken_on_powervr_only(void) {
    dxl_gpu_probe p = smart_pro();
    CHECK_INT(dxl_gpu_msaa_broken(&p), 1);
    p.probed = 0;
    CHECK_INT(dxl_gpu_msaa_broken(&p), 0);
    memset(&p, 0, sizeof p);
    p.probed = 1;
    p.gles = 1;
    snprintf(p.gles_renderer, sizeof p.gles_renderer, "PowerVR Rogue GE8300");   /* a board without Vulkan */
    CHECK_INT(dxl_gpu_msaa_broken(&p), 1);
    memset(&p, 0, sizeof p);
    p.probed = 1;
    p.vulkan = 1;
    snprintf(p.vulkan_device, sizeof p.vulkan_device, "AMD Radeon RX 6800");
    CHECK_INT(dxl_gpu_msaa_broken(&p), 0);
}

/* The probe through dxl-cli --probe --ini and back, as the window reads it. */
static void test_probe_ini(void) {
    dxl_gpu_probe p = smart_pro(), q;
    snprintf(p.note, sizeof p.note, "a note = with an equals sign");
    snprintf(p.gles_renderer, sizeof p.gles_renderer, "PowerVR Rogue GE8300");
    p.vulkan_cpu = 1;
    snprintf(p.vulkan_cpu_device, sizeof p.vulkan_cpu_device, "llvmpipe (LLVM 23.1.1, 256 bits)");
    p.gl_cpu = 1;
    snprintf(p.gl_cpu_renderer, sizeof p.gl_cpu_renderer, "softpipe");
    p.gl = 1;
    snprintf(p.gl_version, sizeof p.gl_version, "4.6 (Core Profile) Mesa 26.2.4");
    char *text = dxl_gpu_probe_to_ini(&p);
    CHECK(strstr(text, "[Probe]") != NULL);
    CHECK_INT(dxl_gpu_probe_from_ini(text, &q), 0);
    CHECK_INT(q.probed, 1);
    CHECK_INT(q.vulkan, p.vulkan);
    CHECK_STR(q.vulkan_device, p.vulkan_device);
    CHECK_STR(q.vulkan_version, "1.3.225");
    CHECK_INT(q.gles, p.gles);
    CHECK_STR(q.gles_version, p.gles_version);
    CHECK_STR(q.gles_renderer, "PowerVR Rogue GE8300");
    CHECK_INT(q.gl, 1);
    CHECK_STR(q.gl_version, p.gl_version);
    CHECK_INT(q.vulkan_cpu, 1);
    CHECK_STR(q.vulkan_cpu_device, p.vulkan_cpu_device);
    CHECK_INT(q.gl_cpu, 1);
    CHECK_STR(q.gl_cpu_renderer, "softpipe");
    CHECK_STR(q.note, p.note);
    free(text);
    CHECK_INT(dxl_gpu_probe_from_ini("nothing here", &q), -1);
    CHECK_INT(q.probed, 0);
    CHECK_INT(dxl_gpu_probe_from_ini(NULL, &q), -1);
}

TEST_MAIN_BEGIN
    RUN(test_probe_ini);
    RUN(test_loads_in_order);
    RUN(test_resolve_on_the_handheld);
    RUN(test_engine_has_it_device_does_not);
    RUN(test_unprobed_is_permissive_but_honest);
    RUN(test_gles_selectable_once_engine_has_it);
    RUN(test_find_is_case_insensitive_and_skips_empty);
    RUN(test_missing_file_is_empty);
    RUN(test_software_where_no_gpu);
    RUN(test_cpu_drivers_alone);
    RUN(test_gl_versions);
    RUN(test_msaa_broken_on_powervr_only);
TEST_MAIN_END
