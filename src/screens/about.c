/* About: the launcher, the engine it starts, where it keeps everything,
 * the fonts and libraries it is made with; the app menu's entry. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>

void dxl_page_about(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_xdg *x = &s->run->session.xdg;
    dxl_shell_ask_engine(s);

    float w = r.w < 900 ? r.w : 900;
    char title[200], buf[1024];
    snprintf(title, sizeof title, "DXLauncher %s", s->version ? s->version : "");
    static const char intro[] = "The desktop's launcher for Deus Ex on VibeEngine, from Port Ex Machina.";
    static const char credits[] =
        "Made with SDL3 and SDL3_ttf (the zlib licence), libxmp-lite (the MIT licence) and FreeType (its own "
        "licence); its type is "
        "Barlow Semi Condensed and Share Tech Mono (the SIL Open Font License 1.1). Deus Ex is its "
        "makers'; DXLauncher plays your own copy of it.";
    const float nw = 150, lh = 28;
    float content = dxl_text_size(ui, DXL_FONT_BODY, intro, w - 44).y + 10 + (s->engine_ok ? 6 : 5) * lh + 12 +
                    dxl_text_size(ui, DXL_FONT_SMALL, credits, w - 44).y;
    float ph = content + 133;
    dxl_rect panel = { r.x, r.y + 6, w, ph < r.h - 6 ? ph : r.h - 6 };
    dxl_rect in = dxl_window(ui, panel, title);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 14);

    float h = dxl_paragraph(ui, in, DXL_FONT_BODY, intro, t->text);
    dxl_cut_top(&in, h + 10);
    dxl_field(ui, &in, lh, nw, "Engine", s->engine_ok ? s->engine_version : s->engine_problem,
              s->engine_ok ? t->text : t->bad, DXL_W_MAY_CUT);
    if (s->engine_ok) dxl_field(ui, &in, lh, nw, "Its features", s->engine_features, t->text, DXL_W_MAY_CUT);
    dxl_field(ui, &in, lh, nw, "Settings", dxl_shell_tilde(x->config, buf, sizeof buf), t->text,
              DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_field(ui, &in, lh, nw, "Saves, shots", dxl_shell_tilde(x->data, buf, sizeof buf), t->text,
              DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_field(ui, &in, lh, nw, "Runs, logs", dxl_shell_tilde(x->state, buf, sizeof buf), t->text,
              DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_field(ui, &in, lh, nw, "Downloads", dxl_shell_tilde(x->cache, buf, sizeof buf), t->text,
              DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_cut_top(&in, 12);
    dxl_paragraph(ui, in, DXL_FONT_SMALL, credits, t->help);

    int entry = dxl_button(ui, "about entry", dxl_cut_left(&buttons, 250),
                           s->desktop_entry ? "Take it off the app menu" : "Add it to the app menu", 0);
    dxl_cut_left(&buttons, 12);
    int open = dxl_button(ui, "about open", dxl_cut_left(&buttons, 230), "Open the settings folder", 0);
    int back = dxl_button(ui, "about back", dxl_cut_right(&buttons, 150), "Back", 0);

    if (entry) {
        if (s->desktop) s->desktop(s, !s->desktop_entry);
    } else if (open) {
        dxl_shell_open(s, x->config);
    } else if (back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_HOME);
    }
}
