/* Recovery: after a run that did not end cleanly (its Running.ini still
 * there), or -safe. How the last run ended, the engine's error, what to
 * do about it, the end of its log, the next run's own words; Play again,
 * the fix, the log itself, the defaults, Quit. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/advice.h"
#include "core/log.h"
#include "core/paths.h"
#include "ui/controls.h"
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
    dxl_shell_probe(s);
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

/* The renderers this machine runs, for the fix: their labels, and which
 * is Settings.json's now. */
static int renderer_choices(dxl_shell *s, const char **labels, int max, int *current) {
    char *path = dxl_path_join(s->run->session.profile.engine_dir, "Settings.json");
    char *def = dxl_path_join(s->run->session.data_dir, "engine-settings.json.default");
    dxl_engine_settings *es = dxl_es_open(path, def);
    const char *now = dxl_es_choice(es, DXL_ES_RENDER_TYPE);
    int n = 0;
    *current = -1;
    for (size_t i = 0; i < s->renderers.count && n < max; i++) {
        const dxl_renderer *rd = &s->renderers.items[i];
        if (!rd->selectable) continue;
        if (!dxl_stricmp(rd->engine_type, now)) *current = n;
        labels[n++] = rd->label;
    }
    dxl_es_free(es);
    free(def);
    free(path);
    return n;
}

/* The engine's renderer from the next run: the one of that label. */
static void choose_renderer(dxl_shell *s, const char *label) {
    for (size_t i = 0; label && i < s->renderers.count; i++) {
        const dxl_renderer *rd = &s->renderers.items[i];
        if (!rd->selectable || strcmp(rd->label, label) != 0) continue;
        char *path = dxl_path_join(s->run->session.profile.engine_dir, "Settings.json");
        char *def = dxl_path_join(s->run->session.data_dir, "engine-settings.json.default");
        dxl_engine_settings *es = dxl_es_open(path, def);
        dxl_err e;
        if (dxl_es_set_choice(es, DXL_ES_RENDER_TYPE, rd->engine_type) == 0 && dxl_es_save(es, &e) == 0) {
            dxl_log("renderer: %s from the next run", rd->engine_type);
            dxl_shell_toast(s, "The game's renderer is %s from its next run.", rd->label);
        } else {
            dxl_shell_toast(s, "The renderer was not changed.");
        }
        dxl_es_free(es);
        free(def);
        free(path);
        return;
    }
}

/* The next run's words (the Launcher page's next-run rows), on or off. */
static int next_run_get(const dxl_shell *s, const char *id) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    snprintf(st.next_run, sizeof st.next_run, "%s", s->next_run);
    const dxl_setting *row = dxl_setting_find(id);
    return row && dxl_setting_bool(&st, row);
}

static void next_run_set(dxl_shell *s, const char *id, int on) {
    dxl_stores st;
    memset(&st, 0, sizeof st);
    snprintf(st.next_run, sizeof st.next_run, "%s", s->next_run);
    const dxl_setting *row = dxl_setting_find(id);
    if (row) dxl_setting_set(&st, row, on ? "1" : "0");
    snprintf(s->next_run, sizeof s->next_run, "%s", st.next_run);
}

/* Those words as toggles, two by two, under their heading. */
static void next_run(dxl_shell *s, dxl_rect r) {
    static const struct { const char *id, *label; } rows[] = {
        { "next.nosound", "No sound" }, { "next.windowed", "In a window" },
        { "next.defaultres", "At 640 x 480" }, { "next.nojoy", "No pad" },
    };
    dxl_ui *ui = &s->ui;
    dxl_label(ui, dxl_cut_top(&r, 24), DXL_FONT_LABEL, "The next run alone", ui->theme.help, DXL_W_LEFT);
    float w = (r.w - 10) / 2, h = (r.h - 4) / 2;
    for (int i = 0; i < 4; i++) {
        dxl_rect cell = { r.x + (float)(i % 2) * (w + 10), r.y + (float)(i / 2) * (h + 4), w, h };
        int on = next_run_get(s, rows[i].id);
        if (dxl_toggle(ui, dxl_ui_id(ui, rows[i].id), cell, rows[i].label, &on, 0)) next_run_set(s, rows[i].id, on);
    }
}

/* Back to the defaults: one page of the settings, or all of them. */
static void reset_page(dxl_shell *s, int page) {
    int was_open = s->settings_open;
    dxl_page_settings_enter(s);
    if (!s->settings_open) return;
    int pages = (int)dxl_settings_page_count();
    dxl_settings_defaults(s, page < pages ? dxl_settings_page((size_t)page) : NULL);
    dxl_shell_toast(s, "%s back to the defaults.", page < pages ? dxl_settings_page((size_t)page) : "Every setting");
    if (!was_open) dxl_settings_keep(s);
}

void dxl_page_recovery(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_lastrun *l = &s->run->last;
    int safe = s->run->decision.screen == DXL_SCREEN_MAIN_SAFE;
    int bad = l->known && !l->clean;
    const dxl_advice *advice = dxl_advice_for(l);
    float w = r.w < 1000 ? r.w : 1000;
    dxl_rect panel = { r.x, r.y + 6, w, r.h - 6 };
    dxl_rect in = dxl_window(ui, panel, bad || !safe ? "The last run did not end cleanly" : "Safe mode");
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 14);
    dxl_rect words = dxl_cut_bottom(&in, 24 + 2 * 38 + 4);
    dxl_cut_bottom(&in, 12);
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
    if (advice) {
        dxl_cut_top(&in, 6);
        float h = dxl_paragraph_typed(ui, in, DXL_FONT_BODY, advice->advice, t->text_focus, s->page_since);
        dxl_cut_top(&in, h);
    }
    dxl_cut_top(&in, 10);
    /* The log's end where three lines of it fit: Read the log has it all. */
    if (in.h >= 28 + 3 * dxl_text_line(ui, DXL_FONT_MONO) + 20) {
        dxl_label(ui, dxl_cut_top(&in, 24), DXL_FONT_LABEL, "The end of its log", t->help, DXL_W_LEFT);
        dxl_cut_top(&in, 4);
        tail(s, in);
    }

    /* Play; the advice's fix; the log; the defaults; Quit. */
    int fix = advice ? advice->fix : DXL_FIX_NONE;
    int play = dxl_button(ui, "recovery play", dxl_cut_left(&buttons, 140), "Play", DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 10);
    int fixed = 0;
    if (fix == DXL_FIX_RENDERER || fix == DXL_FIX_NOSOUND || fix == DXL_FIX_INSTALL) {
        static const char *const fixes[] = { NULL, "Another renderer...", "Play without sound",
                                             "The game's folder..." };
        fixed = dxl_button(ui, "recovery fix", dxl_cut_left(&buttons, 210), fixes[fix], 0);
        dxl_cut_left(&buttons, 10);
    }
    int read = dxl_button(ui, "recovery read", dxl_cut_left(&buttons, 160), "Read the log", 0);
    dxl_cut_left(&buttons, 10);
    int defaults = dxl_button(ui, "recovery defaults", dxl_cut_left(&buttons, 170), "The defaults...", 0);
    int quit = dxl_button(ui, "recovery quit", dxl_cut_right(&buttons, 120), "Quit", 0);
    /* After the buttons, so Play is the page's first focus. */
    next_run(s, words);

    uint32_t fix_id = dxl_ui_id(ui, "recovery fix"), defaults_id = dxl_ui_id(ui, "recovery defaults");
    static const char *labels[16];
    int got = dxl_shell_answer(s, fix_id);
    if (got >= 0) choose_renderer(s, dxl_shell_picked(s, got));
    got = dxl_shell_answer(s, defaults_id);
    if (got >= 0) reset_page(s, got);

    if (play || ui->in.start) {
        s->result = 1;
    } else if (fixed && fix == DXL_FIX_RENDERER) {
        dxl_shell_probe(s);
        int current = 0, n = renderer_choices(s, labels, 16, &current);
        if (n) dxl_shell_pick(s, fix_id, "The game's renderer", labels, n, current);
        else dxl_shell_message(s, "No other renderer", "No renderer this engine has runs here.");
    } else if (fixed && fix == DXL_FIX_NOSOUND) {
        next_run_set(s, "next.nosound", 1);
        s->result = 1;
    } else if (fixed && fix == DXL_FIX_INSTALL) {
        dxl_shell_repoint(s, DXL_PAGE_RECOVERY);
    } else if (read) {
        s->log_which = 0;
        dxl_shell_go(s, DXL_PAGE_LOG);
    } else if (defaults) {
        int pages = (int)dxl_settings_page_count(), n = 0;
        for (int i = 0; i < pages && n < 15; i++) labels[n++] = dxl_settings_page((size_t)i);
        labels[n++] = "Every page";
        dxl_shell_pick(s, defaults_id, "Back to the defaults", labels, n, 0);
    } else if (quit) {
        s->result = 0;
    }
}
