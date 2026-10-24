/* Which renderers exist, and which of them can actually run here.
 *
 * Two independent facts decide that, and the screen shows both:
 *
 *   - Does this *engine build* have a backend for it? Declared per renderer
 *     in renderers.ini as EngineType -- the value Settings.json
 *     RenderDevice.Type takes. Empty means the engine has no such backend
 *     (DXEngine's GL device runs on OpenGL ES too, as GLES; its software
 *     renderer needs nothing of the device).
 *   - Does *this device* have the graphics API it needs? Measured by the
 *     GPU probe (platform/gpu_probe.c) when a page needs it, never assumed.
 *
 * The original tested Direct3D by running itself again, once, with
 * testrendev=D3DDrv.D3DRenderDevice, so a crashing driver only killed a
 * child (dx-reverse-info/wizard.md, Renderer page). The probe keeps that idea
 * -- detection runs in a forked child -- without the re-exec.
 *
 * The choice is written to Settings.json's Type, which DXEngine takes over
 * [Engine.Engine] GameRenderDevice: that key chooses only where no Type is
 * named (SoftDrv.SoftwareRenderDevice the software device, any other the
 * Vulkan one), and -nohard goes over both.
 */
#ifndef DXL_RENDERERS_H
#define DXL_RENDERERS_H

#include "core/common.h"

typedef enum {
    DXL_API_NONE,      /* needs nothing from the system (a CPU renderer) */
    DXL_API_VULKAN,
    DXL_API_GLES,
    DXL_API_GL,        /* desktop OpenGL */
    DXL_API_VULKAN_CPU,/* Mesa's lavapipe */
    DXL_API_GL_CPU     /* Mesa's llvmpipe */
} dxl_gfx_api;

/* What the GPU probe found. Filled by the platform layer; plain data so the
 * resolution logic below stays testable without a GPU. A GPU's drivers and
 * Mesa's CPU drivers apart: lavapipe is no Vulkan GPU, llvmpipe as EGL's
 * driver no GL one. */
typedef struct {
    int  probed;              /* 0: no probe ran, so nothing is known */
    int  vulkan;              /* a GPU's Vulkan device */
    char vulkan_device[128];  /* "PowerVR Rogue GE8300" */
    char vulkan_version[32];  /* "1.3.225" */
    int  gles;                /* a GPU's OpenGL ES */
    char gles_version[128];   /* GL_VERSION when a context could be made, else EGL's */
    char gles_renderer[128];
    int  gl;                  /* a GPU's desktop GL through EGL */
    int  vulkan_cpu;          /* Mesa's lavapipe */
    char vulkan_cpu_device[128];
    int  gl_cpu;              /* Mesa's llvmpipe (or softpipe), through EGL */
    char gl_cpu_renderer[128];
    char note[192];           /* why a probe failed or timed out */
} dxl_gpu_probe;

/* The probe as text and back: an ini of one section, [Probe], which
 * dxl-cli --probe --ini prints for a launcher that runs it as a program of
 * its own (a process with threads must not fork to load drivers). The
 * text is malloc'd; caller frees. Parse returns 0 when it read a probe. */
char *dxl_gpu_probe_to_ini(const dxl_gpu_probe *p);
int   dxl_gpu_probe_from_ini(const char *text, dxl_gpu_probe *out);

/* The PowerVR GE8300's multisample resolve turns partially covered pixels to
 * speckle (found on the TrimUI Smart Pro; its README). Keyed on
 * the GPU, not the device, so any PowerVR is treated the same until shown
 * otherwise. False when no probe ran: nothing is known. */
int dxl_gpu_msaa_broken(const dxl_gpu_probe *g);

typedef struct {
    char *id;             /* section name in renderers.ini */
    char *label;          /* "Vulkan", "OpenGL ES", "Software" */
    char *engine_type;    /* Settings.json RenderDevice.Type; "" if unsupported */
    char *description;    /* one or two sentences for the help pane */
    dxl_gfx_api requires;
    int  cpu_driver;      /* Settings.json RenderDevice.CPUDriver: Mesa's CPU driver asked for */

    /* Filled by dxl_renderers_resolve. */
    int  selectable;
    char status[320];     /* the device it runs on, or why it cannot be used */
} dxl_renderer;

typedef struct {
    dxl_renderer *items;
    size_t        count;
} dxl_renderer_list;

/* Reads path. A missing or empty file yields an empty list -- the caller
 * decides what to do about it rather than being handed a fabricated one. */
void dxl_renderers_load(dxl_renderer_list *out, const char *path);
void dxl_renderers_free(dxl_renderer_list *l);

/* Marks each entry selectable or not, and writes its status line. Pure: the
 * probe result is the only input. When no probe ran, an entry the engine
 * supports stays selectable with an "unverified" status rather than being
 * refused on no evidence. */
void dxl_renderers_resolve(dxl_renderer_list *l, const dxl_gpu_probe *probe);

/* Index of the entry whose EngineType is engine_type (case-insensitive), or
 * -1. */
int dxl_renderers_find(const dxl_renderer_list *l, const char *engine_type);

/* A renderer as a choice's value, its Type and whether it asks for Mesa's
 * CPU driver: "Vulkan", "Vulkan+CPU". The Graphics page's renderer row
 * keeps it in Settings.json as Type and CPUDriver. */
#define DXL_RENDERER_CPU_SUFFIX "+CPU"
void dxl_renderer_value(const dxl_renderer *r, char *out, size_t size);
/* Index of the entry of that value, or -1. */
int  dxl_renderers_find_value(const dxl_renderer_list *l, const char *value);

/* First selectable entry, or -1. */
int dxl_renderers_first_selectable(const dxl_renderer_list *l);

const char *dxl_gfx_api_name(dxl_gfx_api a);

#endif
