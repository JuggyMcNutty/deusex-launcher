/* The TrimUI Smart Pro under spruceOS, as measured (README.md).
 *
 * The CPU modes are spruceOS's own, applied by packaging/port-hooks.sh with
 * the helpers its Ports launcher uses; tests/test_target_port.c checks that
 * every mode named here is one that script handles.
 */
#include "platform/target.h"

static const char *const fonts[] = {
    "/usr/trimui/res/regular.ttf",
    "/usr/trimui/res/full.ttf",
    "/mnt/SDCARD/spruce/Font Files/Noto.ttf",
    NULL
};

static const dxl_cpu_mode_info cpu_modes[] = {
    { "Smart",
      "Smart: the CPU speeds up and down with load, as in the spruceOS menu. "
      "Saves battery; Deus Ex runs noticeably slower." },
    { "Performance",
      "Performance: all four cores held at 1.8 GHz while the game runs, "
      "as spruceOS does for its Ports. Put back as it was when you quit." },
    { "Overclock",
      "Overclock: all four cores at 2.0 GHz. The fastest, but the handheld "
      "runs warmer and the battery drains sooner." },
    { NULL, NULL }
};

static const dxl_target smartpro = {
    .id      = "trimui-smartpro",
    .about   = "Port Ex Machina: a Deus Ex launcher for spruceOS, running Surreal Engine.",
    .fonts   = fonts,
    .panel_w = 1280,
    .panel_h = 720,
    .cpu_modes        = cpu_modes,
    .cpu_mode_default = "Overclock",
    .pad_note = "The Smart Pro's built-in controls report themselves as an Xbox 360 controller.",
    .preview_gpu = {
        .probed         = 1,
        .vulkan         = 1,
        .vulkan_device  = "PowerVR Rogue GE8300",
        .vulkan_version = "1.3.225",
        .gles           = 1,
        .gles_version   = "OpenGL ES 3.2",
    },
};

const dxl_target *dxl_target_get(void) { return &smartpro; }
