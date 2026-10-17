/* The first run's pages: Welcome; The game, which the profile is made
 * from; Your profile, what making it will do, shown before it is done. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/paths.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A row of buttons along the bottom of r: its rect cut off r. */
static dxl_rect button_row(dxl_rect *r) {
    dxl_rect row = dxl_cut_bottom(r, 46);
    dxl_cut_bottom(r, 14);
    return row;
}

/* A window's height for content dp of it above its buttons, no taller
 * than the page: its title bar, padding and buttons around it. */
static float fit(dxl_rect r, float content) {
    float h = content + 133;
    return h < r.h - 6 ? h : r.h - 6;
}

/* ---- Welcome -------------------------------------------------------- */

void dxl_page_welcome(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float w = r.w < 760 ? r.w : 760;
    static const char *const text[] = {
        "DXLauncher plays your copy of Deus Ex on VibeEngine, natively on this computer.",
        "It keeps its own copy of the game's settings, and your saves and screenshots, in your home "
        "folder. The game's folder is never written to.",
        "First, where the game is.",
    };
    float content = 0;
    for (size_t i = 0; i < sizeof text / sizeof *text; i++)
        content += dxl_text_size(ui, DXL_FONT_BODY, text[i], w - 44).y + 12;
    dxl_rect panel = { r.x, r.y + 6, w, fit(r, content) };
    dxl_rect in = dxl_window(ui, panel, "DXLauncher");
    dxl_rect buttons = button_row(&in);

    for (size_t i = 0; i < sizeof text / sizeof *text; i++) {
        float h = dxl_paragraph(ui, in, DXL_FONT_BODY, text[i], i == 2 ? t->text_focus : t->text);
        dxl_cut_top(&in, h + 12);
    }

    int find = dxl_button(ui, "welcome find", dxl_cut_left(&buttons, 230), "Find the game", DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 12);
    int quit = dxl_button(ui, "welcome quit", dxl_cut_left(&buttons, 150), "Quit", 0);
    if (find) dxl_shell_go(s, DXL_PAGE_FIND);
    else if (quit) s->result = 0;
}

/* ---- The game ------------------------------------------------------- */

void dxl_page_find_enter(dxl_shell *s) {
    if (s->looked || !s->run) return;
    s->looked = 1;
    const char *known[17];
    size_t n = dxl_session_installs(&s->run->session, known, 16);
    known[n < 16 ? n : 16] = NULL;
    dxl_finder_quick(&s->found, NULL, s->run->opt.game, known);
    dxl_log("first run: %d install(s) found", s->found.count);
}

/* What a row says of an install. */
static dxl_rgba describe(const dxl_shell *s, const dxl_found *f, char *out, size_t size) {
    const dxl_theme *t = &s->ui.theme;
    const dxl_install *in = &f->install;
    if (in->ok) {
        snprintf(out, size, "Deus Ex %s (%s), %s%s%s", in->build->version, in->build->edition, f->source,
                 in->problem[0] ? ": " : "", in->problem);
        return in->problem[0] ? t->warn : t->good;
    }
    snprintf(out, size, "%s, %s", in->problem, f->source);
    return in->found ? t->warn : t->bad;
}

void dxl_page_find(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float w = r.w < 900 ? r.w : 900;
    dxl_rect panel = { r.x, r.y + 6, w, r.h - 6 };
    dxl_rect in = dxl_window(ui, panel, "Deus Ex on this computer");
    dxl_rect buttons = button_row(&in);

    if (s->repoint) {
        /* The profile's game: gone, the one it had and why; or the one it
         * plays, which another chosen here takes the place of. */
        const dxl_install *was = &s->run->session.install;
        char shown[1024], line[1600];
        if (s->repoint_back && was->ok)
            snprintf(line, sizeof line, "The profile plays the game in %s. A folder chosen here is its game "
                     "from now on.", dxl_shell_tilde(was->root, shown, sizeof shown));
        else
            snprintf(line, sizeof line, "The profile's game is not there to play: %s%s%s.",
                     was->root ? dxl_shell_tilde(was->root, shown, sizeof shown) : "it has none",
                     was->problem[0] ? ", " : "", was->problem);
        float h = dxl_paragraph(ui, in, DXL_FONT_BODY, line, s->repoint_back && was->ok ? t->text : t->warn);
        dxl_cut_top(&in, h + 10);
    }
    int picked = -1;
    if (s->found.count == 0) {
        dxl_paragraph(ui, in, DXL_FONT_BODY,
                      "It is not in the usual places: Steam's library, Heroic's and GOG's folders. Show "
                      "DXLauncher the folder the game is in, the one with System and Maps in it.",
                      t->text);
    } else {
        float hint = dxl_paragraph(ui, in, DXL_FONT_SMALL, "Choose the copy to play:", t->help);
        dxl_cut_top(&in, hint + 8);
        dxl_well(ui, in);
        dxl_list l;
        dxl_list_begin(ui, &l, "found", dxl_inset(in, 4), s->found.count, 54, 0);
        for (int i = 0; i < s->found.count; i++) {
            char path[1024], detail[800];
            dxl_rgba c = describe(s, &s->found.items[i], detail, sizeof detail);
            dxl_shell_tilde(s->found.items[i].install.root, path, sizeof path);
            if (dxl_list_row(ui, &l, i, path, detail, c, DXL_W_TAIL | DXL_W_MAY_CUT) & DXL_HIT_ACTIVATED) picked = i;
        }
        dxl_list_end(ui, &l);
    }

    int browse = dxl_button(ui, "find browse", dxl_cut_left(&buttons, 170), "Browse...",
                            s->found.count ? 0 : DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 12);
    int dialog = dxl_button(ui, "find system", dxl_cut_left(&buttons, 230), "The system's dialog...", 0);
    int quits = s->repoint && !s->repoint_back;
    int back = dxl_button(ui, "find back", dxl_cut_right(&buttons, 150), quits ? "Quit" : "Back", 0);

    if (picked >= 0) {
        const dxl_install *g = &s->found.items[picked].install;
        char shown[1024];
        if (g->ok) dxl_shell_choose(s, g);
        else dxl_shell_message(s, "Not one to play", "%s: %s.", dxl_shell_tilde(g->root, shown, sizeof shown), g->problem);
    } else if (browse) {
        const char *start = s->has_chosen ? s->chosen.root : getenv("HOME");
        dxl_browser_open(s, start ? start : "/");
    } else if (dialog) {
        if (s->pick_folder) s->pick_folder(s);
    } else if (quits) {
        if (back) s->result = 0;
    } else if (s->repoint && (back || ui->in.back)) {
        s->repoint = s->repoint_back = 0;
        dxl_shell_go(s, DXL_PAGE_RECOVERY);
    } else if (back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_WELCOME);
    }
}

/* ---- Your profile -------------------------------------------------- */

/* A file of the install's, by its path in the install. */
static const char *in_install(const dxl_shell *s, const char *path, char *buf, size_t size) {
    const char *root = s->chosen.root;
    size_t n = root ? strlen(root) : 0;
    if (n && strncmp(path, root, n) == 0 && path[n] == '/') return path + n + 1;
    return dxl_shell_tilde(path, buf, size);
}

void dxl_page_import(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_import *p = &s->plan;
    float w = r.w < 900 ? r.w : 900;
    char title[1100], shown[1024], v[1200], buf[1024];
    snprintf(title, sizeof title, "From %s", dxl_shell_tilde(s->chosen.root, shown, sizeof shown));
    char cut[1100];
    dxl_text_fit_tail(ui, DXL_FONT_BUTTON, title, w - 40, cut, sizeof cut);
    const float nw = 150, lh = 29;
    float content = s->plan_error[0] ? dxl_text_size(ui, DXL_FONT_BODY, s->plan_error, w - 44).y
                                     : (7 + (p->master_replaced != 0)) * lh + 10 + dxl_text_line(ui, DXL_FONT_SMALL);
    dxl_rect panel = { r.x, r.y + 6, w, fit(r, content) };
    dxl_rect in = dxl_window(ui, panel, cut);
    dxl_rect buttons = button_row(&in);

    if (s->plan_error[0]) {
        dxl_paragraph(ui, in, DXL_FONT_BODY, s->plan_error, t->bad);
    } else {
        const dxl_install *g = &s->chosen;
        snprintf(v, sizeof v, "Deus Ex %s (%s)", g->build ? g->build->version : "?", g->build ? g->build->edition : "?");
        dxl_field(ui, &in, lh, nw, "The game", v, t->text, DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "Its settings", in_install(s, p->system_from, buf, sizeof buf), t->text,
                  DXL_W_TAIL | DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "Your controls", in_install(s, p->user_from, buf, sizeof buf), t->text,
                  DXL_W_TAIL | DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "The engine's", p->engine_from[0] ? in_install(s, p->engine_from, buf, sizeof buf)
                                                                     : "VibeEngine's own defaults",
                  t->text, DXL_W_TAIL | DXL_W_MAY_CUT);
        if (p->saves) snprintf(v, sizeof v, "%d to copy", p->saves);
        else snprintf(v, sizeof v, "none to copy");
        dxl_field(ui, &in, lh, nw, "Saves", v, t->text, DXL_W_MAY_CUT);
        if (p->shots) snprintf(v, sizeof v, "%d to copy", p->shots);
        else snprintf(v, sizeof v, "none to copy");
        dxl_field(ui, &in, lh, nw, "Screenshots", v, t->text, DXL_W_MAY_CUT);
        if (p->master_replaced)
            dxl_field(ui, &in, lh, nw, "Master server", "333networks', for GameSpy's, which is closed", t->text,
                      DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "Kept in", dxl_shell_tilde(s->run->session.profile.dir, buf, sizeof buf), t->text,
                  DXL_W_TAIL | DXL_W_MAY_CUT);
        dxl_cut_top(&in, 10);
        dxl_paragraph(ui, in, DXL_FONT_SMALL, "Nothing in the game's folder changes.", t->help);
    }

    int make = dxl_button(ui, "import make", dxl_cut_left(&buttons, 230), "Make the profile",
                          DXL_W_PRIMARY | (s->plan_error[0] ? DXL_W_DISABLED : 0));
    int back = dxl_button(ui, "import back", dxl_cut_right(&buttons, 150), "Back", 0);

    if (make) {
        dxl_import done;
        dxl_err e;
        if (dxl_session_make_profile(&s->run->session, &s->chosen, NULL, &done, &e) != 0) {
            dxl_shell_message(s, "The profile was not made", "%s", dxl_err_msg(&e));
            return;
        }
        char saves[32], shots[40];
        snprintf(saves, sizeof saves, done.saves == 1 ? "1 save" : "%d saves", done.saves);
        snprintf(shots, sizeof shots, done.shots == 1 ? "1 screenshot" : "%d screenshots", done.shots);
        if (done.saves && done.shots)
            snprintf(s->note, sizeof s->note, "The profile is made: %s and %s copied from the game's folder.", saves, shots);
        else if (done.saves || done.shots)
            snprintf(s->note, sizeof s->note, "The profile is made: %s copied from the game's folder.",
                     done.saves ? saves : shots);
        else
            snprintf(s->note, sizeof s->note, "The profile is made.");
        s->result = 1;
    } else if (back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_FIND);
    }
}
