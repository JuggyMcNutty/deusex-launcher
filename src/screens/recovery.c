/* Recovery: after a run that did not end cleanly (its Running.ini still
 * there), or -safe. How the last run ended, the engine's error, the end of
 * its log; Play again, the log itself, Quit. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/paths.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *log_path(const dxl_shell *s) {
    return dxl_path_join(s->run->session.profile.logs_dir, DXL_LASTRUN_LOG);
}

void dxl_page_recovery_enter(dxl_shell *s) {
    free(s->log_tail);
    char *log = log_path(s);
    s->log_tail = dxl_lastrun_tail(log, 60);
    free(log);
    dxl_shell_ask_engine(s);
}

/* The log's last lines that fit in r, one line each. */
static void tail(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_well(ui, r);
    dxl_rect in = dxl_inset(r, 10);
    if (!s->log_tail || !*s->log_tail) {
        dxl_label(ui, dxl_cut_top(&in, 22), DXL_FONT_MONO, "(no log)", t->help, DXL_W_LEFT);
        return;
    }
    float lh = dxl_text_line(ui, DXL_FONT_MONO);
    int fit = (int)(in.h / lh);
    if (fit < 1) return;
    /* From the end back, as many lines as fit. */
    const char *start = s->log_tail + strlen(s->log_tail);
    int lines = 0;
    while (start > s->log_tail && lines < fit) {
        const char *p = start - 1;
        if (p > s->log_tail && *p == '\n') p--;
        while (p > s->log_tail && p[-1] != '\n') p--;
        start = p;
        lines++;
    }
    dxl_clip_push(ui, in);
    for (const char *p = start; *p;) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) : strlen(p);
        char line[512];
        if (n >= sizeof line) n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = '\0';
        if (n) dxl_label(ui, dxl_cut_top(&in, lh), DXL_FONT_MONO, line, t->text, DXL_W_LEFT | DXL_W_MAY_CUT);
        else dxl_cut_top(&in, lh);
        if (!e) break;
        p = e + 1;
    }
    dxl_clip_pop(ui);
}

void dxl_page_recovery(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_lastrun *l = &s->run->last;
    int safe = s->run->decision.screen == DXL_SCREEN_MAIN_SAFE;
    int bad = l->known && !l->clean;
    float w = r.w < 980 ? r.w : 980;
    dxl_rect panel = { r.x, r.y + 6, w, r.h - 6 };
    dxl_rect in = dxl_window(ui, panel, bad || !safe ? "The last run did not end cleanly" : "Safe mode");
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 14);
    const float nw = 120, lh = 29;
    char v[800];

    if (safe) {
        float h = dxl_paragraph(ui, in, DXL_FONT_BODY,
                                l->known ? "DXLauncher was started with -safe, which opens this page before the "
                                           "game; how the last run ended is below."
                                         : "DXLauncher was started with -safe, which opens this page before the game.",
                                t->text);
        dxl_cut_top(&in, h + 10);
    }
    if (l->known) {
        if (l->clean) snprintf(v, sizeof v, "cleanly");
        else dxl_lastrun_describe(l, v, sizeof v);
        dxl_field(ui, &in, lh, nw, "How it ended", v, l->clean ? t->text : t->warn, DXL_W_MAY_CUT);
        time_t tt = (time_t)l->ended;
        struct tm tm;
        if (localtime_r(&tt, &tm) && strftime(v, sizeof v, "%e %B %Y, %H:%M:%S", &tm))
            dxl_field(ui, &in, lh, nw, "When", v[0] == ' ' ? v + 1 : v, t->text, 0);
        if (l->error[0]) dxl_field(ui, &in, lh, nw, "Its error", l->error, t->bad, DXL_W_MAY_CUT);
    } else if (!safe) {
        dxl_field(ui, &in, lh, nw, "How it ended", "not known: the run left no record", t->warn, DXL_W_MAY_CUT);
    }
    dxl_cut_top(&in, 8);
    dxl_label(ui, dxl_cut_top(&in, 24), DXL_FONT_LABEL, "The end of its log", t->help, DXL_W_LEFT);
    dxl_cut_top(&in, 4);
    if (in.h > 40) tail(s, in);

    int play = dxl_button(ui, "recovery play", dxl_cut_left(&buttons, 200), "Play", DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 12);
    int open = dxl_button(ui, "recovery log", dxl_cut_left(&buttons, 200), "Open the log", 0);
    int quit = dxl_button(ui, "recovery quit", dxl_cut_right(&buttons, 150), "Quit", 0);

    if (play || ui->in.start) {
        s->result = 1;
    } else if (open) {
        char *log = log_path(s);
        dxl_shell_open(s, log);
        free(log);
    } else if (quit) {
        s->result = 0;
    }
}
