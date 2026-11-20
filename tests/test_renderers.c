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
    CHECK_INT(q.gl, p.gl);
    CHECK_STR(q.note, p.note);
    free(text);
    CHECK_INT(dxl_gpu_probe_from_ini("nothing here", &q), -1);
    CHECK_INT(q.probed, 0);
    CHECK_INT(dxl_gpu_probe_from_ini(NULL, &q), -1);
}

/* Every GPU's Vulkan device by name, each once, through the ini and back:
 * the Graphics page's GPU row offers them (bugs.md REN.79). */
static void test_probe_gpus(void) {
    dxl_gpu_probe p, q;
    memset(&p, 0, sizeof p);
    p.probed = 1;
    dxl_gpu_probe_add_gpu(&p, "AMD Radeon 780M Graphics (RADV PHOENIX)");
    dxl_gpu_probe_add_gpu(&p, "NVIDIA GeForce RTX 4060 Laptop GPU");
    dxl_gpu_probe_add_gpu(&p, "NVIDIA GeForce RTX 4060 Laptop GPU");   /* two of one kind: one name */
    dxl_gpu_probe_add_gpu(&p, "");
    dxl_gpu_probe_add_gpu(&p, NULL);
    CHECK_INT(p.vulkan_gpus, 2);
    char long_name[200];
    memset(long_name, 'x', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    dxl_gpu_probe_add_gpu(&p, long_name);
    CHECK_INT(p.vulkan_gpus, 3);
    CHECK_INT((int)strlen(p.vulkan_gpu[2]), DXL_GPU_NAME_LEN - 1);
    for (int i = 0; i < 2 * DXL_GPU_NAMES; i++) {
        char name[32];
        snprintf(name, sizeof name, "GPU %d", i);
        dxl_gpu_probe_add_gpu(&p, name);
    }
    CHECK_INT(p.vulkan_gpus, DXL_GPU_NAMES);

    char *text = dxl_gpu_probe_to_ini(&p);
    CHECK_INT(dxl_gpu_probe_from_ini(text, &q), 0);
    CHECK_INT(q.vulkan_gpus, DXL_GPU_NAMES);
    CHECK_STR(q.vulkan_gpu[0], "AMD Radeon 780M Graphics (RADV PHOENIX)");
    CHECK_STR(q.vulkan_gpu[1], "NVIDIA GeForce RTX 4060 Laptop GPU");
    CHECK_STR(q.vulkan_gpu[2], p.vulkan_gpu[2]);
    CHECK_STR(q.vulkan_gpu[DXL_GPU_NAMES - 1], p.vulkan_gpu[DXL_GPU_NAMES - 1]);
    free(text);
    /* An older dxl-cli's answer: none named */
    CHECK_INT(dxl_gpu_probe_from_ini("[Probe]\nProbed=True\nVulkan=True\nVulkanDevice=X\n", &q), 0);
    CHECK_INT(q.vulkan_gpus, 0);
}

TEST_MAIN_BEGIN
    RUN(test_probe_ini);
    RUN(test_probe_gpus);
    RUN(test_loads_in_order);
    RUN(test_resolve_on_the_handheld);
    RUN(test_engine_has_it_device_does_not);
    RUN(test_unprobed_is_permissive_but_honest);
    RUN(test_gles_selectable_once_engine_has_it);
    RUN(test_find_is_case_insensitive_and_skips_empty);
    RUN(test_missing_file_is_empty);
    RUN(test_software_where_no_gpu);
    RUN(test_msaa_broken_on_powervr_only);
TEST_MAIN_END
