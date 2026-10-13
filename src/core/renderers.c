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
    return DXL_API_NONE;
}

const char *dxl_gfx_api_name(dxl_gfx_api a) {
    switch (a) {
    case DXL_API_NONE:   return "none";
    case DXL_API_VULKAN: return "Vulkan";
    case DXL_API_GLES:   return "OpenGL ES";
    case DXL_API_GL:     return "OpenGL";
    }
    return "?";
}

void dxl_renderers_load(dxl_renderer_list *out, const char *path) {
    memset(out, 0, sizeof *out);

    dxl_ini *ini = dxl_ini_load(path, NULL);
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
        snprintf(out, n, "desktop OpenGL driver present");
        break;
    case DXL_API_NONE:
        snprintf(out, n, "runs on the CPU");
        break;
    }
}

static int device_has(dxl_gfx_api api, const dxl_gpu_probe *p) {
    switch (api) {
    case DXL_API_NONE:   return 1;
    case DXL_API_VULKAN: return p->vulkan;
    case DXL_API_GLES:   return p->gles;
    case DXL_API_GL:     return p->gl;
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
            snprintf(r->status, sizeof r->status, "Not available: no %s driver on this device",
                     dxl_gfx_api_name(r->requires));
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

int dxl_renderers_first_selectable(const dxl_renderer_list *l) {
    for (size_t i = 0; i < l->count; i++)
        if (l->items[i].selectable) return (int)i;
    return -1;
}
