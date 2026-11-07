/* The log: a run's output, or the engine's own log, line by line in the
 * mono type, from its end; scrolled by the keys, the wheel and a pad. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/lastrun.h"
#include "core/paths.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What is read of a log: its last 4 MB. */
#define READ_MAX (4L << 20)

static const char *const names[3] = { "The last run", "The run before", "The engine's own" };

static char *path_of(const dxl_shell *s, int which) {
    const dxl_profile *p = &s->run->session.profile;
    switch (which) {
    case 1: return dxl_path_join(p->logs_dir, DXL_PREVIOUSRUN_LOG);
    case 2: return dxl_path_join(p->engine_dir, "DXE-Log-LastRun.txt");
    default: return dxl_path_join(p->logs_dir, DXL_LASTRUN_LOG);
    }
}

void dxl_page_log_free(dxl_shell *s) {
    free(s->log_text);
    free(s->log_starts);
    s->log_text = NULL;
    s->log_starts = NULL;
    s->log_lines = 0;
}

static void load(dxl_shell *s) {
    dxl_page_log_free(s);
    char *path = path_of(s, s->log_which);
    FILE *f = fopen(path, "rb");
    free(path);
    if (!f) return;
    long size = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
    if (size < 0) { fclose(f); return; }   /* unseekable, or past what a long holds */
    long from = size > READ_MAX ? size - READ_MAX : 0;
    if (fseek(f, from, SEEK_SET) != 0) { fclose(f); return; }
    s->log_text = dxl_xmalloc((size_t)(size - from) + 1);
    size_t n = fread(s->log_text, 1, (size_t)(size - from), f);
    fclose(f);
    s->log_text[n] = '\0';
    /* A cut start begins at the next whole line. */
    char *text = s->log_text;
    if (from > 0) {
        char *nl = strchr(text, '\n');
        if (nl) text = nl + 1;
    }
    int cap = 1024;
    s->log_starts = dxl_xmalloc(sizeof *s->log_starts * (size_t)cap);
    for (char *p = text; *p;) {
        if (s->log_lines == cap) {
            cap *= 2;
            s->log_starts = dxl_xrealloc(s->log_starts, sizeof *s->log_starts * (size_t)cap);
        }
        s->log_starts[s->log_lines++] = p - s->log_text;
        char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    s->log_top = 1 << 30;   /* from its end */
}

void dxl_page_log_enter(dxl_shell *s) { load(s); }

/* Line i, its tabs as spaces and no CR. */
static void line(const dxl_shell *s, int i, char *out, size_t size) {
    const char *p = s->log_text + s->log_starts[i];
    size_t n = 0;
    for (; *p && *p != '\n' && n + 4 < size; p++) {
        if (*p == '\r') continue;
        if (*p == '\t') {
            memcpy(out + n, "    ", 4);
            n += 4;
        } else {
            out[n++] = *p;
        }
    }
    out[n] = '\0';
}

void dxl_page_log(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float w = r.w < 1200 ? r.w : 1200;
    dxl_rect area = { r.x, r.y, w, r.h };
    int which = s->log_which;
    dxl_tabs(ui, "log tabs", dxl_cut_top(&area, 42), names, 3, &s->log_which);
    if (s->log_which != which) load(s);
    dxl_cut_top(&area, 10);
    dxl_rect buttons = dxl_cut_bottom(&area, 46);
    dxl_cut_bottom(&area, 12);

    /* The view: one focusable, its up and down its own but off either
     * end, where the kit's move stands and takes the focus out of it, to
     * the tabs above or the buttons below, as a list's does. */
    uint32_t vid = dxl_ui_id(ui, "log view");
    int hits = dxl_ui_focusable(ui, vid, area, 0);
    dxl_well(ui, area);
    if (hits & DXL_HIT_FOCUSED) dxl_outline(ui, area, 1, dxl_rgba_fade(t->accent, 0.5f));
    dxl_rect in = dxl_inset(area, 12);
    float lh = dxl_text_line(ui, DXL_FONT_MONO);
    int shown = (int)(in.h / lh);
    if (shown < 1) shown = 1;
    int max = s->log_lines - shown;
    if (max < 0) max = 0;
    if (s->log_top > max) s->log_top = max;
    int down = ui->in.nav[DXL_DIR_DOWN], up = ui->in.nav[DXL_DIR_UP];
    int off_end = (down > up && s->log_top >= max) || (up > down && s->log_top <= 0);
    int steps = off_end ? 0 : dxl_ui_take_ud(ui, vid);
    if (hits & DXL_HIT_FOCUSED) {
        steps += (ui->in.page_down - ui->in.page_up) * shown;
        ui->in.page_down = ui->in.page_up = 0;
    }
    if (ui->in.wheel != 0 && (hits & DXL_HIT_HOVERED)) {
        steps -= (int)(ui->in.wheel * 3);
        ui->in.wheel = 0;
    }
    s->log_top += steps;
    if (s->log_top > max) s->log_top = max;
    if (s->log_top < 0) s->log_top = 0;

    if (!s->log_lines) {
        dxl_label(ui, dxl_cut_top(&in, lh), DXL_FONT_MONO, "(no log)", t->help, DXL_W_LEFT);
    } else {
        dxl_clip_push(ui, in);
        char text[1024];
        for (int i = s->log_top; i < s->log_lines && i < s->log_top + shown; i++) {
            line(s, i, text, sizeof text);
            if (*text) dxl_label(ui, dxl_cut_top(&in, lh), DXL_FONT_MONO, text, t->text, DXL_W_LEFT | DXL_W_MAY_CUT);
            else dxl_cut_top(&in, lh);
        }
        dxl_clip_pop(ui);
        char where[96];
        snprintf(where, sizeof where, "lines %d to %d of %d", s->log_top + 1,
                 s->log_top + shown < s->log_lines ? s->log_top + shown : s->log_lines, s->log_lines);
        dxl_label(ui, (dxl_rect){ buttons.x + 380, buttons.y, buttons.w - 380 - 170, buttons.h }, DXL_FONT_SMALL, where,
                  t->help, DXL_W_RIGHT | DXL_W_MAY_CUT);
    }

    int back = dxl_button(ui, "log back", dxl_cut_right(&buttons, 150), "Back", 0);
    if (back || ui->in.back) {
        dxl_shell_go(s, s->from == DXL_PAGE_LOG ? DXL_PAGE_HOME : s->from);
    }
}
