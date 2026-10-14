/* The profile of a desktop Linux machine. Its fonts are the usual desktop
 * ones (ui/theme.c), it offers no CPU modes (a desktop's governor is its
 * owner's business), and its pad is whatever the player plugged in.
 */
#include "platform/target.h"

static const dxl_target generic = {
    .id      = "generic",
    .about   = "Port Ex Machina: a Deus Ex launcher, running Surreal Engine.",
    .panel_w = 1280,
    .panel_h = 720,
    .preview_gpu = {
        .probed         = 1,
        .vulkan         = 1,
        .vulkan_device  = "Desktop GPU (preview)",
        .vulkan_version = "1.3",
        .gl             = 1,
    },
};

const dxl_target *dxl_target_get(void) { return &generic; }
