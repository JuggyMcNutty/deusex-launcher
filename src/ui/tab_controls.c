/* Controls: the pad in the game.
 *
 * The layout is User.ini's Joy* bindings (core/bindings.c); the feel -- dead
 * zone, look speed, invert, menu cursor speed -- is Settings.json's Gamepad
 * block, read by the fork's controller support (engine patch 0003).
 */
#include "screens_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void pad_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const char *name = dxl_ui_pad_name(s->ui);
    snprintf(out, n, "%s", name ? name : "None detected");
}

static void pad_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const char *note = dxl_target_get()->pad_note;
    if (dxl_ui_pad_name(s->ui))
        snprintf(out, n, "The controller SDL sees.%s%s", note ? " " : "", note ? note : "");
    else
        snprintf(out, n, "No controller is connected. The launcher also works from a "
                 "keyboard: arrows, Enter, Backspace, R to reset, P to play.");
}

/* ---- layout ------------------------------------------------------------ */

static void layout_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const dxl_pad_preset *p = dxl_app_current_layout(s->app);
    snprintf(out, n, "%s", p ? p->label : "Custom");
}

static void layout_describe(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    const dxl_pad_preset *p = dxl_app_current_layout(s->app);
    if (!dxl_config_user_ini(s->app->cfg))
        snprintf(out, n, "There is no User.ini or DefUser.ini to hold bindings.");
    else if (p)
        snprintf(out, n, "%s", p->description);
    else
        snprintf(out, n, "The Joy bindings in %s match no preset: they were edited by "
                 "hand or in the game. Choosing a preset replaces them.",
                 dxl_config_user_path(s->app->cfg));
}

static int layout_enabled(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (dxl_config_user_ini(s->app->cfg)) return 1;
    snprintf(why, n, "There is no User.ini or DefUser.ini in the game's System folder "
             "to hold the bindings.");
    return 0;
}

static void apply_pending_preset(dxl_session *s) {
    const dxl_pad_preset *p = dxl_pad_preset_at((size_t)s->pending_preset);
    if (!p) return;
    dxl_app_choose_layout(s->app, p);
    notice(s, 0, "Layout: %s.", p->label);
}

static void layout_step(dxl_session *s, const dxl_row *r, int dir) {
    const dxl_pad_preset *cur = dxl_app_current_layout(s->app);
    const dxl_pad_preset *base = cur ? cur : dxl_app_base_layout(s->app);
    size_t count = dxl_pad_preset_count(), at = 0;
    for (size_t i = 0; i < count; i++)
        if (dxl_pad_preset_at(i) == base) at = i;
    size_t next = (at + count + (size_t)(dir > 0 ? 1 : count - 1)) % count;
    s->pending_preset = (int)next;
    if (cur) { apply_pending_preset(s); return; }

    /* Customized buttons would be lost: say so first. */
    char body[256];
    snprintf(body, sizeof body, "Your buttons have been customized. Switching to %s replaces "
             "all of them with its layout.", dxl_pad_preset_at(next)->label);
    open_confirm(s, "Replace customized buttons?", body, "Switch layout", apply_pending_preset);
}

static void layout_reset(dxl_session *s, const dxl_row *r) {
    dxl_app_choose_layout(s->app, dxl_pad_preset_default());
}

static void customize_activate(dxl_session *s, const dxl_row *r) { open_remap(s); }

static int pad_on(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (dxl_es_bool(s->app->es, DXL_ES_PAD_ENABLED)) return 1;
    snprintf(why, n, "Controller in game is off.");
    return 0;
}

static const dxl_row rows[] = {
    { .label = "Controller", .value = pad_value, .describe = pad_describe },
    { .label = "Controller in game",
      .help = "The pad plays the game: sticks move and look, buttons follow the layout "
              "below, and in menus a stick moves the pointer. Off leaves the engine's "
              "basic mapping (A is Enter, d-pad is the arrow keys).",
      .value = es_value, .step = es_step, .reset = es_reset, .arg = DXL_ES_PAD_ENABLED },
    { .label = "Layout", .enabled = layout_enabled, .value = layout_value,
      .step = layout_step, .reset = layout_reset, .describe = layout_describe },
    { .label = "Customize buttons",
      .help = "Every button and what it does, and any of them set to another action: "
              "the game's key-menu actions, the pause menu, belt slots or "
              "augmentation hotkeys.",
      .enabled = layout_enabled, .activate = customize_activate },
    { .label = "Look speed, horizontal",
      .help = "How fast the look stick turns you left and right.",
      .enabled = pad_on, .value = es_value, .slider = es_slider, .step = es_step,
      .reset = es_reset, .arg = DXL_ES_PAD_LOOK_X },
    { .label = "Look speed, vertical",
      .help = "How fast the look stick tilts the view up and down.",
      .enabled = pad_on, .value = es_value, .slider = es_slider, .step = es_step,
      .reset = es_reset, .arg = DXL_ES_PAD_LOOK_Y },
    { .label = "Invert look",
      .help = "Pushing the look stick up looks down, as in a flight sim.",
      .enabled = pad_on, .value = es_value, .step = es_step, .reset = es_reset,
      .arg = DXL_ES_PAD_INVERT_Y },
    { .label = "Stick dead zone",
      .help = "How far a stick must move before it counts. Raise it if the view drifts "
              "while the stick is at rest.",
      .enabled = pad_on, .value = es_value, .slider = es_slider, .step = es_step,
      .reset = es_reset, .arg = DXL_ES_PAD_DEADZONE },
    { .label = "Menu pointer speed",
      .help = "How fast a stick moves the pointer in the game's menus, inventory and "
              "conversations.",
      .enabled = pad_on, .value = es_value, .slider = es_slider, .step = es_step,
      .reset = es_reset, .arg = DXL_ES_PAD_CURSOR_SPEED },
};

dxl_rows tab_controls_rows(void) {
    return (dxl_rows){ rows, (int)(sizeof rows / sizeof *rows) };
}
