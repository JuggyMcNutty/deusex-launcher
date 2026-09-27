/* Per-button remapping: a list of the pad's buttons with what each does, and
 * for the chosen button a picker of every action the game can bind.
 *
 * The actions are the game's own key-binding menu (core/bindings.c), grouped
 * the way that menu is. What changes is User.ini's Joy line for that button,
 * the same line the in-game key menu would write for a key -- so a remapped
 * layout is an ordinary User.ini, not a launcher-private format.
 */
#include "screens_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The panel both lists share: most of the screen, above the footer. */
static SDL_Rect panel_rect(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int footer = dxl_ui_line_height(s->ui, DXL_TXT_FOOTER) + m->pad;
    return (SDL_Rect){ m->pad, m->pad / 2, dxl_ui_width(s->ui) - 2 * m->pad,
                       dxl_ui_height(s->ui) - m->pad / 2 - footer };
}

/* Rows fit between the title and a two-line note at the bottom. */
static int rows_top(dxl_session *s, SDL_Rect p) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    return p.y + m->pad / 4 + dxl_ui_line_height(s->ui, DXL_TXT_ITEM) + m->pad / 4;
}
static int note_top(dxl_session *s, SDL_Rect p) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    return p.y + p.h - m->pad / 4 - 2 * dxl_ui_line_height(s->ui, DXL_TXT_BODY);
}
static int rows_per_page(dxl_session *s, SDL_Rect p) {
    int per = (note_top(s, p) - rows_top(s, p) - dxl_ui_metrics(s->ui)->pad / 4) /
              dxl_ui_metrics(s->ui)->item_height;
    return per > 1 ? per : 1;
}

static void keep_visible(int cursor, int *scroll, int per, int n) {
    if (cursor < *scroll) *scroll = cursor;
    if (cursor >= *scroll + per) *scroll = cursor - per + 1;
    if (*scroll > n - per) *scroll = n - per > 0 ? n - per : 0;
    if (*scroll < 0) *scroll = 0;
}

/* One row inside the panel. A header is a dim group name with no bar. */
static void panel_row(dxl_session *s, SDL_Rect p, int y, const char *label,
                      const char *value, int selected, int header, int mark) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int ih = m->item_height, lh = dxl_ui_line_height(s->ui, DXL_TXT_ITEM);
    int x = p.x + m->pad / 2;
    if (header) {
        dxl_ui_text(s->ui, x, y + ih - dxl_ui_line_height(s->ui, DXL_TXT_FOOTER) - 4,
                    label, DXL_TXT_FOOTER, DXL_PAL.accent_dim);
        return;
    }
    if (selected) {
        dxl_ui_fill(s->ui, (SDL_Rect){ p.x + 8, y, p.w - 16, ih }, DXL_PAL.panel_sel);
        dxl_ui_fill(s->ui, (SDL_Rect){ p.x + 8, y, 4, ih }, DXL_PAL.accent);
    }
    dxl_ui_text(s->ui, x + m->pad / 4, y + (ih - lh) / 2, label, DXL_TXT_ITEM,
                selected ? DXL_PAL.accent : DXL_PAL.text);
    if (value && *value) {
        int vw = dxl_ui_text_width(s->ui, value, DXL_TXT_ITEM);
        dxl_ui_text(s->ui, p.x + p.w - m->pad / 2 - vw, y + (ih - lh) / 2, value, DXL_TXT_ITEM,
                    mark ? DXL_PAL.ok : selected ? DXL_PAL.text : DXL_PAL.text_dim);
    }
}

static void panel_title(dxl_session *s, SDL_Rect p, const char *title) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    dxl_ui_overlay_panel(s->ui, p);
    dxl_ui_text(s->ui, p.x + m->pad / 2, p.y + m->pad / 4, title, DXL_TXT_ITEM, DXL_PAL.accent);
}

static void panel_note(dxl_session *s, SDL_Rect p, const char *text) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int y = note_top(s, p);
    dxl_ui_fill(s->ui, (SDL_Rect){ p.x + m->pad / 2, y - m->pad / 8, p.w - m->pad, 1 }, DXL_PAL.rule);
    if (s->notice_frames > 0) {
        dxl_ui_paragraph_n(s->ui, p.x + m->pad / 2, y, p.w - m->pad, s->notice, DXL_TXT_BODY,
                           s->notice_bad ? DXL_PAL.bad : DXL_PAL.ok, 2);
        return;
    }
    dxl_ui_paragraph_n(s->ui, p.x + m->pad / 2, y, p.w - m->pad, text, DXL_TXT_BODY,
                       DXL_PAL.text_dim, 2);
}

static const char *current_binding(dxl_session *s, const char *key) {
    dxl_ini *user = dxl_config_user_ini(s->app->cfg);
    const char *v = user ? dxl_ini_get(user, DXL_INPUT_SECTION, key) : NULL;
    return v ? v : "";
}

/* ---- the button list ---------------------------------------------------- */

void open_remap(dxl_session *s) {
    s->overlay = DXL_OVL_REMAP;
    s->ovl_cursor = s->ovl_scroll = 0;
}

void remap_draw(dxl_session *s) {
    SDL_Rect p = panel_rect(s);
    const dxl_pad_preset *now = dxl_app_current_layout(s->app);
    char title[128];
    snprintf(title, sizeof title, "Customize buttons -- %s", now ? now->label : "Custom");
    panel_title(s, p, title);

    int n = (int)dxl_pad_button_count(), per = rows_per_page(s, p);
    keep_visible(s->ovl_cursor, &s->ovl_scroll, per, n);
    int top = rows_top(s, p), ih = dxl_ui_metrics(s->ui)->item_height;
    for (int i = 0; i < per && s->ovl_scroll + i < n; i++) {
        const char *key = dxl_pad_button_at((size_t)(s->ovl_scroll + i));
        char what[96];
        dxl_binding_describe(current_binding(s, key), what, sizeof what);
        panel_row(s, p, top + i * ih, dxl_joy_key_label(key), what,
                  s->ovl_scroll + i == s->ovl_cursor, 0, 0);
    }

    const char *key = dxl_pad_button_at((size_t)s->ovl_cursor);
    const dxl_pad_preset *base = dxl_app_base_layout(s->app);
    char def[96], note[256];
    dxl_binding_describe(dxl_pad_preset_binding(base, key), def, sizeof def);
    snprintf(note, sizeof note, "X puts back %s's default: %s. Sticks: the Layout setting. "
             "In the game's menus, B, Y, SELECT and START go back.", base->label, def);
    panel_note(s, p, note);
}

static void open_actions(dxl_session *s);

void remap_act(dxl_session *s, dxl_act a) {
    int n = (int)dxl_pad_button_count();
    const char *key = dxl_pad_button_at((size_t)s->ovl_cursor);
    switch (a) {
    case DXL_ACT_UP:   s->ovl_cursor = (s->ovl_cursor + n - 1) % n; s->notice_frames = 0; break;
    case DXL_ACT_DOWN: s->ovl_cursor = (s->ovl_cursor + 1) % n;     s->notice_frames = 0; break;
    case DXL_ACT_CONFIRM:
        s->remap_button = s->ovl_cursor;
        s->remap_cursor = s->ovl_cursor;
        s->remap_scroll = s->ovl_scroll;
        open_actions(s);
        break;
    case DXL_ACT_ALT: {
        const dxl_pad_preset *base = dxl_app_base_layout(s->app);
        char what[96];
        dxl_app_bind(s->app, key, dxl_pad_preset_binding(base, key));
        dxl_binding_describe(dxl_pad_preset_binding(base, key), what, sizeof what);
        notice(s, 0, "%s: %s again.", dxl_joy_key_label(key), what);
        break;
    }
    case DXL_ACT_BACK:
    case DXL_ACT_QUIT:
        s->overlay = DXL_OVL_NONE;
        s->ovl_cursor = s->ovl_scroll = 0;
        break;
    default:
        break;
    }
}

/* ---- the action picker -------------------------------------------------- */

/* Rows: action indexes, with a header row (-1) before each new group. */
static void open_actions(dxl_session *s) {
    free(s->action_rows);
    size_t na = dxl_pad_action_count();
    s->action_rows = dxl_xmalloc(2 * na * sizeof *s->action_rows);
    s->action_nrows = 0;
    const char *group = NULL;
    for (size_t i = 0; i < na; i++) {
        const dxl_pad_action *act = dxl_pad_action_at(i);
        if (act->group[0] && (!group || strcmp(group, act->group) != 0)) {
            s->action_rows[s->action_nrows++] = -1 - (int)i;   /* header for this group */
            group = act->group;
        }
        s->action_rows[s->action_nrows++] = (int)i;
    }

    /* Start on what the button does now. */
    const char *key = dxl_pad_button_at((size_t)s->remap_button);
    int cur = dxl_pad_action_find(current_binding(s, key));
    s->ovl_cursor = 0;
    for (int r = 0; r < s->action_nrows; r++)
        if (s->action_rows[r] == (cur >= 0 ? cur : 0)) { s->ovl_cursor = r; break; }
    s->ovl_scroll = 0;
    s->overlay = DXL_OVL_ACTIONS;
}

static void back_to_buttons(dxl_session *s) {
    free(s->action_rows);
    s->action_rows = NULL;
    s->action_nrows = 0;
    s->overlay = DXL_OVL_REMAP;
    s->ovl_cursor = s->remap_cursor;
    s->ovl_scroll = s->remap_scroll;
}

void actions_draw(dxl_session *s) {
    SDL_Rect p = panel_rect(s);
    const char *key = dxl_pad_button_at((size_t)s->remap_button);
    char title[128];
    snprintf(title, sizeof title, "%s does...", dxl_joy_key_label(key));
    panel_title(s, p, title);

    int per = rows_per_page(s, p);
    keep_visible(s->ovl_cursor, &s->ovl_scroll, per, s->action_nrows);
    /* Keep the group header above the first visible action in view. */
    if (s->ovl_scroll > 0 && s->ovl_cursor == s->ovl_scroll &&
        s->action_rows[s->ovl_scroll - 1] < 0)
        s->ovl_scroll--;

    int cur = dxl_pad_action_find(current_binding(s, key));
    int top = rows_top(s, p), ih = dxl_ui_metrics(s->ui)->item_height;
    for (int i = 0; i < per && s->ovl_scroll + i < s->action_nrows; i++) {
        int r = s->action_rows[s->ovl_scroll + i];
        if (r < 0) {
            panel_row(s, p, top + i * ih, dxl_pad_action_at((size_t)(-1 - r))->group,
                      NULL, 0, 1, 0);
            continue;
        }
        panel_row(s, p, top + i * ih, dxl_pad_action_at((size_t)r)->label,
                  r == cur ? "Current" : NULL, s->ovl_scroll + i == s->ovl_cursor, 0, r == cur);
    }
    panel_note(s, p, "Every action the game's own key menu offers, plus the pause menu, "
               "belt slots and augmentation hotkeys.");
}

/* Moves the cursor by dir, stepping over group headers, wrapping. */
static void actions_move(dxl_session *s, int dir) {
    int n = s->action_nrows;
    for (int k = 0; k < n; k++) {
        s->ovl_cursor = (s->ovl_cursor + dir + n) % n;
        if (s->action_rows[s->ovl_cursor] >= 0) return;
    }
}

void actions_act(dxl_session *s, dxl_act a) {
    SDL_Rect p = panel_rect(s);
    switch (a) {
    case DXL_ACT_UP:   actions_move(s, -1); break;
    case DXL_ACT_DOWN: actions_move(s, +1); break;
    case DXL_ACT_LEFT:
    case DXL_ACT_TAB_PREV:
        for (int i = rows_per_page(s, p); i > 0; i--) actions_move(s, -1);
        break;
    case DXL_ACT_RIGHT:
    case DXL_ACT_TAB_NEXT:
        for (int i = rows_per_page(s, p); i > 0; i--) actions_move(s, +1);
        break;
    case DXL_ACT_CONFIRM: {
        int r = s->action_rows[s->ovl_cursor];
        if (r < 0) break;
        const char *key = dxl_pad_button_at((size_t)s->remap_button);
        const dxl_pad_action *act = dxl_pad_action_at((size_t)r);
        dxl_app_bind(s->app, key, act->command);
        back_to_buttons(s);
        notice(s, 0, "%s: %s.", dxl_joy_key_label(key), act->label);
        break;
    }
    case DXL_ACT_BACK:
    case DXL_ACT_QUIT:
        back_to_buttons(s);
        break;
    default:
        break;
    }
}

void remap_free(dxl_session *s) {
    free(s->action_rows);
    s->action_rows = NULL;
    s->action_nrows = 0;
}
