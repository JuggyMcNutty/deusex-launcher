#include "ui/osk.h"
#include "ui/draw.h"
#include "ui/glyphs.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <string.h>

#define COLS 10
#define KEY_W 54
#define KEY_H 44
#define GAP 6

static const char *const lower[5] = { "1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-", "_/:@!?#&()" };
static const char *const upper[5] = { "1234567890", "QWERTYUIOP", "ASDFGHJKL\"", "ZXCVBNM;+=", "_/:@!?#&()" };

void dxl_osk_open(dxl_osk_state *st, const char *buf) {
    snprintf(st->original, sizeof st->original, "%s", buf ? buf : "");
    st->shift = 0;
}

static void put(char *buf, size_t size, const char *s) {
    size_t len = strlen(buf), add = strlen(s);
    if (len + add < size) memcpy(buf + len, s, add + 1);
}

static void erase(char *buf) {
    size_t len = strlen(buf);
    if (!len) return;
    len--;
    while (len > 0 && ((unsigned char)buf[len] & 0xc0) == 0x80) len--;
    buf[len] = '\0';
}

int dxl_osk(dxl_ui *ui, dxl_osk_state *st, dxl_rect over, const char *title, char *buf, size_t size, int back) {
    const dxl_theme *t = &ui->theme;
    float w = COLS * KEY_W + (COLS - 1) * GAP + 44, h = 5 + 32 + 16 + 44 + 14 + 6 * KEY_H + 5 * GAP + 20;
    dxl_rect in = dxl_dialog_begin(ui, over, w, h, title);
    int ret = 0;

    /* The text, its end where the next key goes. */
    dxl_rect line = dxl_cut_top(&in, 44);
    dxl_well(ui, line);
    dxl_rect tr = dxl_inset(line, 10);
    char shown[600];
    dxl_text_fit_tail(ui, DXL_FONT_BODY, buf, tr.w - 6, shown, sizeof shown);
    float lh = dxl_text_line(ui, DXL_FONT_BODY), tw = *shown ? dxl_text_size(ui, DXL_FONT_BODY, shown, 0).x : 0;
    dxl_text_draw(ui, DXL_FONT_BODY, shown, tr.x, line.y + (line.h - lh) / 2, 0, t->text_focus, 1);
    dxl_fill(ui, (dxl_rect){ tr.x + tw + 1, line.y + (line.h - lh) / 2 + 2, 2, lh - 4 }, t->bracket);
    dxl_cut_top(&in, 14);

    float kw = (in.w - (COLS - 1) * GAP) / COLS;
    const char *const *rows = st->shift ? upper : lower;
    for (int r = 0; r < 5; r++) {
        dxl_rect rr = dxl_cut_top(&in, KEY_H);
        dxl_cut_top(&in, GAP);
        for (int c = 0; c < COLS && rows[r][c]; c++) {
            char key[2] = { rows[r][c], 0 }, id[16];
            snprintf(id, sizeof id, "osk %d %d", r, c);
            dxl_rect k = { rr.x + c * (kw + GAP), rr.y, kw, KEY_H };
            if (dxl_button(ui, id, k, key, 0)) put(buf, size, key);
        }
    }
    /* Shift, space, erase, done and cancel along the bottom. */
    dxl_rect last = dxl_cut_top(&in, KEY_H);
    float unit = (last.w - 4 * GAP) / 10;
    dxl_rect k = dxl_cut_left(&last, unit * 2);
    if (dxl_button(ui, "osk shift", k, st->shift ? "abc" : "Shift", st->shift ? DXL_W_PRIMARY : 0)) st->shift = !st->shift;
    dxl_cut_left(&last, GAP);
    if (dxl_button(ui, "osk space", dxl_cut_left(&last, unit * 3), "Space", 0)) put(buf, size, " ");
    dxl_cut_left(&last, GAP);
    int erased = dxl_button(ui, "osk erase", dxl_cut_left(&last, unit * 1.6f), "Erase", 0);
    dxl_cut_left(&last, GAP);
    int done = dxl_button(ui, "osk done", dxl_cut_left(&last, unit * 1.7f), "Done", DXL_W_PRIMARY);
    dxl_cut_left(&last, GAP);
    int cancel = dxl_button(ui, "osk cancel", last, "Cancel", 0);
    dxl_dialog_end(ui);

    if (erased || ui->in.clear) {
        erase(buf);
        ui->in.clear = 0;
    }
    if (done || ui->in.start) {
        ui->in.start = 0;
        ret = 1;
    } else if (cancel || back) {
        snprintf(buf, size, "%s", st->original);
        ret = -1;
    }
    return ret;
}
