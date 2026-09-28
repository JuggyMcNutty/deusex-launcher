#include "gui/msgbox.h"

#define WRAP     264    /* the text's width in the box's template: 176 dialog units */
#define ICON_X   12     /* the icon's left, and the margin right of the text */
#define ICON     32
#define FRAME_W  6      /* the window's borders, which the buttons are centred with */

static void on_command(dxl_dialog *d, int id, dxl_notify what, void *ctx) {
    (void)ctx;
    if (what == DXL_CLICKED && (id == DXL_IDOK || id == DXL_IDCANCEL)) dxl_dialog_end(d, id);
}

dxl_dialog *dxl_msgbox_build(dxl_gui *g, const dxl_msgbox_spec *s, int offscreen, dxl_err *err) {
    TTF_Font *f = g->font_msg;
    int line = dxl_font_height(f);

    /* The buttons: twice the widest label, or four lines if wider, and two
     * lines high; a third of a button apart. */
    const char *labels[2] = { "OK", "Cancel" };
    int n = s->ok_cancel ? 2 : 1, bw = 0;
    for (int i = 0; i < n; i++) {
        int w = dxl_text_width(g, f, labels[i]);
        if (w > bw) bw = w;
    }
    if (bw < 2 * line) bw = 2 * line;
    bw *= 2;
    int bh = 2 * line, space = bw / 3;

    /* The text beside the icon, as wide as its widest line but never
     * narrower than the buttons need; both centred in a band 16 taller than
     * the taller of the two. */
    int text_w = 0;
    SDL_Rect measure = { 0, 0, WRAP, 0 };
    int text_h = dxl_text_wrapped(NULL, g, f, measure, s->text, dxl_colors.text, &text_w);
    int icon = s->error_icon ? ICON : 0;
    int text_x = icon ? ICON_X + ICON_X + icon : ICON_X;
    int need = (bw + space) * n + space - text_x;
    if (text_w < need) text_w = need;
    int band = 16 + (icon > text_h ? icon : text_h);
    int w = text_x + text_w + ICON_X;
    int h = 8 + band + bh;

    dxl_dialog *d = dxl_dialog_open(g, s->title, w, h, offscreen, err);
    if (!d) return NULL;
    dxl_dialog_set_font(d, f);
    d->on_command = on_command;
    if (icon) {
        SDL_Rect ir = { ICON_X, (band - icon) / 2, icon, icon };
        dxl_dialog_add(d, DXL_CTL_ICON, 0, ir, "", 0);
    }
    SDL_Rect tr = { text_x, (band - text_h) / 2, text_w, text_h };
    /* A message box's text shows & as it is. */
    dxl_buf esc;
    dxl_buf_init(&esc);
    dxl_buf_add(&esc, "", 0);
    for (const char *p = s->text; *p; p++) {
        if (*p == '&') dxl_buf_add(&esc, "&", 1);
        dxl_buf_add(&esc, p, 1);
    }
    dxl_dialog_add(d, DXL_CTL_TEXT, -1, tr, esc.data, 0);
    dxl_buf_free(&esc);
    int bx = (w + FRAME_W - (bw + space) * n + space) / 2;
    for (int i = 0; i < n; i++) {
        SDL_Rect br = { bx + i * (bw + space), band, bw, bh };
        dxl_ctl *c = dxl_dialog_add(d, DXL_CTL_PUSH, i == 0 ? DXL_IDOK : DXL_IDCANCEL, br,
                                    labels[i], 0);
        c->tabstop = 1;
        c->is_default = i == 0;
    }
    dxl_dialog_focus_first(d);
    return d;
}

int dxl_msgbox(dxl_gui *g, const dxl_msgbox_spec *s) {
    dxl_err e;
    dxl_dialog *d = dxl_msgbox_build(g, s, 0, &e);
    if (!d) return 0;
    if (d->win) SDL_SetWindowAlwaysOnTop(d->win, SDL_TRUE);   /* MB_TOPMOST */
    while (!dxl_dialog_step(d, 50)) {}
    int ok = d->result == DXL_IDOK;
    dxl_dialog_close(d);
    return ok;
}
