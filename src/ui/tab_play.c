/* The home tab: what will happen when you press Play, and anything that went
 * wrong last time.
 *
 * The crash banner replaces the original's RecoveryMode page. The original
 * could only say "it was not shut down properly"; the engine's log can say
 * why, so the banner quotes it.
 */
#include "screens_internal.h"

#include <stdio.h>
#include <string.h>

enum { PLAY_PLAY, PLAY_TROUBLESHOOT, PLAY_QUIT };

/* The rows shown, in order. Troubleshoot only appears after a crash. */
static int play_rows(dxl_session *s, int *out) {
    int n = 0;
    out[n++] = PLAY_PLAY;
    if (s->crashed) out[n++] = PLAY_TROUBLESHOOT;
    out[n++] = PLAY_QUIT;
    return n;
}

int tab_play_initial_cursor(dxl_session *s) {
    return s->crashed ? 1 : 0;
}

static const char *row_label(int id) {
    switch (id) {
    case PLAY_PLAY:         return "Play Deus Ex";
    case PLAY_TROUBLESHOOT: return "Troubleshoot";
    case PLAY_QUIT:         return "Quit";
    }
    return "";
}

/* "label   value" lines in the status block. */
static int status_line(dxl_session *s, int y, const char *label, const char *value,
                       SDL_Color vc) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int col = m->pad + dxl_ui_width(s->ui) / 6;
    dxl_ui_text(s->ui, m->pad, y, label, DXL_TXT_BODY, DXL_PAL.text_dim);
    return dxl_ui_paragraph_n(s->ui, col, y, dxl_ui_width(s->ui) - col - m->pad, value,
                              DXL_TXT_BODY, vc, 2);
}

/* A coloured bar down the left edge, then text: warnings and notes. */
static int banner(dxl_session *s, int y, SDL_Color c, const char *head, const char *body) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int w = dxl_ui_width(s->ui) - 2 * m->pad;
    int start = y;
    int x = m->pad + m->pad / 2;
    y = dxl_ui_paragraph_n(s->ui, x, y, w - m->pad / 2, head, DXL_TXT_BODY, c, 2);
    if (body && *body)
        y = dxl_ui_paragraph_n(s->ui, x, y, w - m->pad / 2, body, DXL_TXT_BODY, DXL_PAL.text, 3);
    dxl_ui_fill(s->ui, (SDL_Rect){ m->pad, start, 4, y - start }, c);
    return y + m->pad / 3;
}

void tab_play_draw(dxl_session *s, int top, int bottom) {
    dxl_app *app = s->app;
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int y = top;

    dxl_ui_text(s->ui, m->pad, y, "Deus Ex", DXL_TXT_TITLE, DXL_PAL.accent);
    y += dxl_ui_line_height(s->ui, DXL_TXT_TITLE) + m->pad / 3;

    char buf[512];
    if (s->crashed) {
        if (s->crash_detail[0])
            snprintf(buf, sizeof buf, "The engine reported: %s", s->crash_detail);
        else
            snprintf(buf, sizeof buf, "No error was recorded. If it keeps happening, "
                     "the System tab has the engine log and resets.");
        y = banner(s, y, DXL_PAL.bad, "The last session did not shut down properly.", buf);
    }
    switch (dxl_config_seeded(app->cfg)) {
    case 1:
        y = banner(s, y, DXL_PAL.warn, "Created DeusEx.ini from Default.ini.",
                   "The install had no game configuration yet; the engine needs one to "
                   "find its packages. It will be written when you play.");
        break;
    case 2:
        y = banner(s, y, DXL_PAL.warn, "Repaired DeusEx.ini.",
                   "It was missing the engine's package paths (an earlier launcher wrote "
                   "an incomplete one), so it was rebuilt from Default.ini, keeping its "
                   "own values. It will be written when you play.");
        break;
    default:
        break;
    }
    if (app->decision.effective_first_run < DXL_FIRSTRUN_WIZARD_BELOW && !s->crashed)
        y = banner(s, y, DXL_PAL.accent,
                   "First run on this install.",
                   "The defaults suit this device. Adjust them on the Video and Controls "
                   "tabs, or just press START.");
    if (app->pad_layout_update)
        y = banner(s, y, DXL_PAL.ok, "Controller layout updated.", app->pad_layout_update);
    if (app->pad_layout_applied)
        y = banner(s, y, DXL_PAL.ok, "Controller layout set to Modern.",
                   "The bindings Deus Ex ships with only use three buttons. Change it on "
                   "the Controls tab.");

    /* What will happen on Play. */
    int cur = dxl_app_current_renderer(app);
    if (cur >= 0) {
        const dxl_renderer *r = &app->renderers.items[cur];
        snprintf(buf, sizeof buf, "%s -- %s", r->label, r->status);
        y = status_line(s, y, "Renderer", buf, r->selectable ? DXL_PAL.text : DXL_PAL.warn);
    } else {
        snprintf(buf, sizeof buf, "%s (not a renderer this launcher knows)",
                 dxl_es_choice(app->es, DXL_ES_RENDER_TYPE));
        y = status_line(s, y, "Renderer", buf, DXL_PAL.warn);
    }
    const char *pad = dxl_ui_pad_name(s->ui);
    const dxl_pad_preset *layout = dxl_app_current_layout(app);
    snprintf(buf, sizeof buf, "%s -- layout: %s", pad ? pad : "none detected",
             layout ? layout->label : "custom");
    y = status_line(s, y, "Controller", buf, pad ? DXL_PAL.text : DXL_PAL.text_dim);
    status_line(s, y, "Game files", app->game_dir, DXL_PAL.text_dim);

    /* The choices, anchored to the bottom. */
    int ids[3];
    int n = play_rows(s, ids);
    int *c = &s->cursor[DXL_TAB_PLAY];
    if (*c >= n) *c = n - 1;
    int ry = bottom - n * m->item_height;
    for (int i = 0; i < n; i++)
        dxl_ui_row(s->ui, ry + i * m->item_height, row_label(ids[i]), NULL, *c == i, 0);

    if (s->notice_frames > 0)
        dxl_ui_paragraph_n(s->ui, m->pad, ry - dxl_ui_line_height(s->ui, DXL_TXT_BODY) - m->pad / 4,
                           dxl_ui_width(s->ui) - 2 * m->pad, s->notice, DXL_TXT_BODY,
                           s->notice_bad ? DXL_PAL.bad : DXL_PAL.ok, 1);
}

void tab_play_act(dxl_session *s, dxl_act a) {
    int ids[3];
    int n = play_rows(s, ids);
    int *c = &s->cursor[DXL_TAB_PLAY];
    switch (a) {
    case DXL_ACT_UP:   *c = (*c + n - 1) % n; break;
    case DXL_ACT_DOWN: *c = (*c + 1) % n;     break;
    case DXL_ACT_CONFIRM:
        switch (ids[*c]) {
        case PLAY_PLAY:         s->end = DXL_END_LAUNCH; break;
        case PLAY_TROUBLESHOOT: switch_tab(s, DXL_TAB_SYSTEM); break;
        case PLAY_QUIT:         s->end = DXL_END_QUIT; break;
        }
        break;
    case DXL_ACT_BACK:
        /* Home is the root: back leaves, the way handheld frontends' apps do. */
        s->end = DXL_END_QUIT;
        break;
    default:
        break;
    }
}
