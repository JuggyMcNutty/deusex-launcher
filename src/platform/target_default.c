/* The profile for a port that does not supply its own ports/<id>/target.c:
 * a desktop Linux machine. Nothing to add to the generic font list, no CPU
 * modes to offer (a desktop's governor is its owner's business), and a pad
 * that is whatever the player plugged in.
 */
#include "platform/target.h"

static const dxl_target generic = {
    .id      = "generic",
    .about   = "Port Ex Machina: a Deus Ex launcher, running Surreal Engine.",
    .fonts   = NULL,
    .panel_w = 1280,
    .panel_h = 720,
    .cpu_modes = NULL,
    .pad_note  = NULL,
    .preview_gpu = {
        .probed         = 1,
        .vulkan         = 1,
        .vulkan_device  = "Desktop GPU (preview)",
        .vulkan_version = "1.3",
        .gl             = 1,
    },
};

const dxl_target *dxl_target_get(void) { return &generic; }
