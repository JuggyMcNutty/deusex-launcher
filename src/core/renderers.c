#include "core/renderers.h"
#include "core/ini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static dxl_gfx_api parse_api(const char *s) {
    if (!s || !*s || !dxl_stricmp(s, "none")) return DXL_API_NONE;
    if (!dxl_stricmp(s, "vulkan")) return DXL_API_VULKAN;
    if (!dxl_stricmp(s, "gles"))   return DXL_API_GLES;
    if (!dxl_stricmp(s, "gl"))     return DXL_API_GL;
    if (!dxl_stricmp(s, "vulkan-cpu")) return DXL_API_VULKAN_CPU;
    if (!dxl_stricmp(s, "gl-cpu"))     return DXL_API_GL_CPU;
    return DXL_API_NONE;
}

const char *dxl_gfx_api_name(dxl_gfx_api a) {
    switch (a) {
    case DXL_API_NONE:   return "none";
    case DXL_API_VULKAN: return "Vulkan";
    case DXL_API_GLES:   return "OpenGL ES";
    case DXL_API_GL:     return "OpenGL";
    case DXL_API_VULKAN_CPU: return "lavapipe";
    case DXL_API_GL_CPU:     return "llvmpipe";
    }
    return "?";
}

void dxl_renderers_load(dxl_renderer_list *out, const char *path) {
    memset(out, 0, sizeof *out);

    dxl_ini *ini = dxl_ini_load_utf8(path, NULL);
    if (!ini) return;

    size_t n = dxl_ini_get_all(ini, "Renderers", "Renderer", NULL, 0);
    if (n == 0) { dxl_ini_free(ini); return; }

    const char **ids = dxl_xmalloc(n * sizeof *ids);
    dxl_ini_get_all(ini, "Renderers", "Renderer", ids, n);

    out->items = dxl_xmalloc(n * sizeof *out->items);
    for (size_t i = 0; i < n; i++) {
        const char *id = ids[i];
        if (!id || !*id) continue;

        dxl_renderer *r = &out->items[out->count++];
        memset(r, 0, sizeof *r);
        r->id = dxl_xstrdup(id);

        const char *v = dxl_ini_get(ini, id, "Label");
        r->label = dxl_xstrdup(v && *v ? v : id);
        v = dxl_ini_get(ini, id, "EngineType");
        r->engine_type = dxl_xstrdup(v ? v : "");
        v = dxl_ini_get(ini, id, "Description");
        r->description = dxl_xstrdup(v ? v : "");
        r->requires = parse_api(dxl_ini_get(ini, id, "Requires"));
        r->cpu_driver = dxl_ini_get_bool(ini, id, "CPUDriver", 0);
    }

    free(ids);
    dxl_ini_free(ini);
}

void dxl_renderers_free(dxl_renderer_list *l) {
    if (!l) return;
    for (size_t i = 0; i < l->count; i++) {
        free(l->items[i].id);
        free(l->items[i].label);
        free(l->items[i].engine_type);
        free(l->items[i].description);
    }
    free(l->items);
    memset(l, 0, sizeof *l);
}

int dxl_gl_version_at_least(const char *v, int major, int minor) {
    int ma = 0, mi = 0;
    if (!v) return 0;
    if (!strncmp(v, "OpenGL ES ", 10)) v += 10;
    if (sscanf(v, "%d.%d", &ma, &mi) != 2) return 0;
    return ma > major || (ma == major && mi >= minor);
}

/* The one-line description of what the probe found for an API. */
static void found_line(char *out, size_t n, dxl_gfx_api api, const dxl_gpu_probe *p) {
    switch (api) {
    case DXL_API_VULKAN:
        if (p->vulkan_device[0] && p->vulkan_version[0])
            snprintf(out, n, "%s, Vulkan %s", p->vulkan_device, p->vulkan_version);
        else
            snprintf(out, n, "%s", p->vulkan_device[0] ? p->vulkan_device : "Vulkan driver present");
        break;
    case DXL_API_GLES:
        snprintf(out, n, "%s%s%s",
                 p->gles_renderer[0] ? p->gles_renderer : "",
                 p->gles_renderer[0] && p->gles_version[0] ? ", " : "",
                 p->gles_version[0] ? p->gles_version : (p->gles_renderer[0] ? "" : "OpenGL ES driver present"));
        break;
    case DXL_API_GL:
        if (p->gl_version[0]) snprintf(out, n, "OpenGL %s", p->gl_version);
        else snprintf(out, n, "desktop OpenGL driver present");
        break;
    case DXL_API_NONE:
        snprintf(out, n, "runs on the CPU");
        break;
    case DXL_API_VULKAN_CPU:
        snprintf(out, n, "%s, on the CPU", p->vulkan_cpu_device[0] ? p->vulkan_cpu_device : "lavapipe");
        break;
    case DXL_API_GL_CPU:
        snprintf(out, n, "%s, on the CPU", p->gl_cpu_renderer[0] ? p->gl_cpu_renderer : "llvmpipe");
        break;
    }
}

static int device_has(dxl_gfx_api api, const dxl_gpu_probe *p) {
    switch (api) {
    case DXL_API_NONE:   return 1;
    case DXL_API_VULKAN: return p->vulkan;
    case DXL_API_GLES:   return p->gles;
    case DXL_API_GL:     return p->gl;
    case DXL_API_VULKAN_CPU: return p->vulkan_cpu;
    case DXL_API_GL_CPU:     return p->gl_cpu;
    }
    return 0;
}

void dxl_renderers_resolve(dxl_renderer_list *l, const dxl_gpu_probe *probe) {
    static const dxl_gpu_probe none;
    const dxl_gpu_probe *p = probe ? probe : &none;

    for (size_t i = 0; i < l->count; i++) {
        dxl_renderer *r = &l->items[i];
        int engine_has = r->engine_type[0] != '\0';
        char found[272];
        found_line(found, sizeof found, r->requires, p);

        /* The device side is reported even when the engine side already rules
         * the entry out: "the GPU can do GLES, the engine cannot" is the
         * useful thing to know about a greyed-out row. */
        if (!engine_has) {
            r->selectable = 0;
            if (p->probed && r->requires != DXL_API_NONE && device_has(r->requires, p))
                snprintf(r->status, sizeof r->status,
                         "Not in this engine build (device has it: %s)", found);
            else if (p->probed && r->requires != DXL_API_NONE)
                snprintf(r->status, sizeof r->status,
                         "Not in this engine build, and not on this device");
            else
                snprintf(r->status, sizeof r->status, "Not in this engine build");
            continue;
        }
        if (!p->probed) {
            r->selectable = 1;
            snprintf(r->status, sizeof r->status, "Not checked -- device detection did not run");
            continue;
        }
        if (!device_has(r->requires, p)) {
            r->selectable = 0;
            /* A GPU's driver missing where Mesa's CPU one is there: the
             * engine would take it for no GPU, as this row's sibling says. */
            if (r->requires == DXL_API_VULKAN && p->vulkan_cpu)
                snprintf(r->status, sizeof r->status, "Not available: Vulkan here is only Mesa's lavapipe, on the CPU");
            else if ((r->requires == DXL_API_GL || r->requires == DXL_API_GLES) && p->gl_cpu)
                snprintf(r->status, sizeof r->status, "Not available: OpenGL here is only Mesa's llvmpipe, on the CPU");
            else
                snprintf(r->status, sizeof r->status, "Not available: no %s driver on this device",
                         r->requires == DXL_API_GL     ? "OpenGL 4.2"
                         : r->requires == DXL_API_GLES ? "OpenGL ES 3.1"
                                                       : dxl_gfx_api_name(r->requires));
            continue;
        }
        r->selectable = 1;
        snprintf(r->status, sizeof r->status, "%s", found);
    }
}

int dxl_renderers_find(const dxl_renderer_list *l, const char *engine_type) {
    if (!engine_type || !*engine_type) return -1;
    for (size_t i = 0; i < l->count; i++)
        if (dxl_stricmp(l->items[i].engine_type, engine_type) == 0) return (int)i;
    return -1;
}

void dxl_renderer_value(const dxl_renderer *r, char *out, size_t size) {
    snprintf(out, size, "%s%s", r->engine_type, r->cpu_driver ? DXL_RENDERER_CPU_SUFFIX : "");
}

int dxl_renderers_find_value(const dxl_renderer_list *l, const char *value) {
    if (!value || !*value) return -1;
    char v[128];
    for (size_t i = 0; i < l->count; i++) {
        if (!l->items[i].engine_type[0]) continue;
        dxl_renderer_value(&l->items[i], v, sizeof v);
        if (dxl_stricmp(v, value) == 0) return (int)i;
    }
    return -1;
}

int dxl_renderers_first_selectable(const dxl_renderer_list *l) {
    for (size_t i = 0; i < l->count; i++)
        if (l->items[i].selectable) return (int)i;
    return -1;
}

int dxl_gpu_msaa_broken(const dxl_gpu_probe *g) {
    return g->probed && (strstr(g->vulkan_device, "PowerVR") != NULL || strstr(g->gles_renderer, "PowerVR") != NULL);
}

#define PROBE "Probe"

char *dxl_gpu_probe_to_ini(const dxl_gpu_probe *p) {
    dxl_ini *ini = dxl_ini_new_utf8();
    dxl_ini_set_eol(ini, "\n");
    dxl_ini_set_bool(ini, PROBE, "Probed", p->probed);
    dxl_ini_set_bool(ini, PROBE, "Vulkan", p->vulkan);
    dxl_ini_set(ini, PROBE, "VulkanDevice", p->vulkan_device);
    dxl_ini_set(ini, PROBE, "VulkanVersion", p->vulkan_version);
    dxl_ini_set_bool(ini, PROBE, "GLES", p->gles);
    dxl_ini_set(ini, PROBE, "GLESVersion", p->gles_version);
    dxl_ini_set(ini, PROBE, "GLESRenderer", p->gles_renderer);
    dxl_ini_set_bool(ini, PROBE, "GL", p->gl);
    dxl_ini_set(ini, PROBE, "GLVersion", p->gl_version);
    dxl_ini_set_bool(ini, PROBE, "VulkanCPU", p->vulkan_cpu);
    dxl_ini_set(ini, PROBE, "VulkanCPUDevice", p->vulkan_cpu_device);
    dxl_ini_set_bool(ini, PROBE, "GLCPU", p->gl_cpu);
    dxl_ini_set(ini, PROBE, "GLCPURenderer", p->gl_cpu_renderer);
    dxl_ini_set(ini, PROBE, "Note", p->note);
    size_t len = 0;
    char *text = dxl_ini_render(ini, &len);
    dxl_ini_free(ini);
    return text;
}

static void copy_key(const dxl_ini *ini, const char *key, char *out, size_t size) {
    const char *v = dxl_ini_get(ini, PROBE, key);
    snprintf(out, size, "%s", v ? v : "");
}

int dxl_gpu_probe_from_ini(const char *text, dxl_gpu_probe *out) {
    memset(out, 0, sizeof *out);
    dxl_ini *ini = text ? dxl_ini_parse_utf8(text, strlen(text)) : NULL;
    if (!ini || !dxl_ini_has_section(ini, PROBE)) {
        dxl_ini_free(ini);
        snprintf(out->note, sizeof out->note, "the probe gave no answer");
        return -1;
    }
    out->probed = dxl_ini_get_bool(ini, PROBE, "Probed", 0);
    out->vulkan = dxl_ini_get_bool(ini, PROBE, "Vulkan", 0);
    copy_key(ini, "VulkanDevice", out->vulkan_device, sizeof out->vulkan_device);
    copy_key(ini, "VulkanVersion", out->vulkan_version, sizeof out->vulkan_version);
    out->gles = dxl_ini_get_bool(ini, PROBE, "GLES", 0);
    copy_key(ini, "GLESVersion", out->gles_version, sizeof out->gles_version);
    copy_key(ini, "GLESRenderer", out->gles_renderer, sizeof out->gles_renderer);
    out->gl = dxl_ini_get_bool(ini, PROBE, "GL", 0);
    copy_key(ini, "GLVersion", out->gl_version, sizeof out->gl_version);
    out->vulkan_cpu = dxl_ini_get_bool(ini, PROBE, "VulkanCPU", 0);
    copy_key(ini, "VulkanCPUDevice", out->vulkan_cpu_device, sizeof out->vulkan_cpu_device);
    out->gl_cpu = dxl_ini_get_bool(ini, PROBE, "GLCPU", 0);
    copy_key(ini, "GLCPURenderer", out->gl_cpu_renderer, sizeof out->gl_cpu_renderer);
    copy_key(ini, "Note", out->note, sizeof out->note);
    dxl_ini_free(ini);
    return 0;
}
