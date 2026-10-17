#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/paths.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void dxl_page_home_enter(dxl_shell *s) {
    dxl_shell_ask_engine(s);
    s->saves = s->run ? dxl_profile_count_saves(&s->run->session.profile) : 0;
}

static void when(long t, char *out, size_t size) {
    time_t tt = (time_t)t;
    struct tm tm;
    if (!localtime_r(&tt, &tm) || !strftime(out, size, "%e %B %Y, %H:%M", &tm)) snprintf(out, size, "?");
    if (out[0] == ' ') memmove(out, out + 1, strlen(out));
}

/* What Home says of the last run: how it ended, and when. */
static dxl_rgba last_run(const dxl_shell *s, char *out, size_t size) {
    const dxl_lastrun *l = &s->run->last;
    const dxl_theme *t = &s->ui.theme;
    if (!l->known) {
        snprintf(out, size, "never, with this profile");
        return t->text;
    }
    char date[64], how[600];
    when(l->ended, date, sizeof date);
    if (l->clean) {
        snprintf(out, size, "ended cleanly, %s", date);
        return t->text;
    }
    dxl_lastrun_describe(l, how, sizeof how);
    snprintf(out, size, "%s, %s", how, date);
    return t->warn;
}

/* Home's buttons: the game's main menu's, where the install has them. */
static int button_id(dxl_shell *s, uint32_t id, dxl_rect r, const char *label, int flags) {
    if (dxl_art_has_buttons(&s->art)) return dxl_art_button(&s->ui, &s->art, id, r, label, flags);
    return dxl_button_id(&s->ui, id, r, label, flags);
}

static int button(dxl_shell *s, const char *name, dxl_rect r, const char *label, int flags) {
    return button_id(s, dxl_ui_id(&s->ui, name), r, label, flags);
}

void dxl_page_home(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    int playable = ss->install.ok && s->engine_ok;

    float menu_w = r.w * 0.3f;
    if (menu_w < 240) menu_w = 240;
    if (menu_w > 320) menu_w = 320;
    dxl_rect menu = { r.x, r.y + 6, menu_w, r.h - 6 };
    int play = button(s, "home play", dxl_cut_top(&menu, 48), "Play",
                      DXL_W_PRIMARY | DXL_W_LEFT | (playable ? 0 : DXL_W_DISABLED));
    dxl_cut_top(&menu, 10);
    /* What keeps it from playing, put right from here: the first focus. */
    uint32_t engine_id = dxl_ui_id(ui, "home engine");
    int game_fix = 0;
    if (!ss->install.ok) {
        game_fix = button(s, "home game", dxl_cut_top(&menu, 48), "The game's folder...", DXL_W_LEFT);
        dxl_cut_top(&menu, 10);
    }
    if (!s->engine_ok) {
        dxl_shell_engine_answer(s, engine_id);
        if (button_id(s, engine_id, dxl_cut_top(&menu, 48), "The engine...", DXL_W_LEFT))
            dxl_shell_engine_pick(s, engine_id);
        dxl_cut_top(&menu, 10);
    }
    int settings = button(s, "home settings", dxl_cut_top(&menu, 48), "Settings", DXL_W_LEFT);
    dxl_cut_top(&menu, 10);
    int profiles = button(s, "home profiles", dxl_cut_top(&menu, 48), "Profiles", DXL_W_LEFT);
    dxl_cut_top(&menu, 10);
    int about = button(s, "home about", dxl_cut_top(&menu, 48), "About", DXL_W_LEFT);
    dxl_cut_top(&menu, 10);
    int quit = button(s, "home quit", dxl_cut_top(&menu, 48), "Quit", DXL_W_LEFT);

    /* The profile: its game, the engine, its saves, its last run. */
    float px = r.x + menu_w + 40, pw = r.x + r.w - px;
    if (pw > 640) pw = 640;
    const char *name = dxl_profile_get(&ss->profile, "Name");
    char title[200], v[1024], shown[1024];
    snprintf(title, sizeof title, "Profile: %s", name ? name : ss->profile.id);
    dxl_rect panel = { px, r.y + 6, pw, 5 + 32 + 16 + 5 * 30 + 22 };
    dxl_rect in = dxl_window(ui, panel, title);
    const float nw = 104, lh = 30;

    dxl_field(ui, &in, lh, nw, "Game", dxl_shell_tilde(ss->install.root, shown, sizeof shown), t->text,
              DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_rgba c = t->text;
    if (!ss->install.ok) {
        snprintf(v, sizeof v, "%s", ss->install.problem[0] ? ss->install.problem : "not one the engine plays");
        c = t->bad;
    } else {
        snprintf(v, sizeof v, "Deus Ex %s (%s)%s%s", ss->install.build->version, ss->install.build->edition,
                 ss->install.problem[0] ? ": " : "", ss->install.problem);
        if (ss->install.problem[0]) c = t->warn;
    }
    dxl_field(ui, &in, lh, nw, "Build", v, c, DXL_W_MAY_CUT);
    dxl_field(ui, &in, lh, nw, "Engine", s->engine_ok ? s->engine_version : s->engine_brief,
              s->engine_ok ? t->text : t->bad, DXL_W_MAY_CUT);
    if (s->saves) snprintf(v, sizeof v, "%d", s->saves);
    else snprintf(v, sizeof v, "none yet");
    dxl_field(ui, &in, lh, nw, "Saves", v, t->text, 0);
    c = last_run(s, v, sizeof v);
    dxl_field(ui, &in, lh, nw, "Last run", v, c, DXL_W_MAY_CUT);

    /* Under it what the first run did, or why the engine is refused, whole. */
    dxl_rect note = { px, panel.y + panel.h + 14, pw, r.y + r.h - (panel.y + panel.h + 14) };
    if (!s->engine_ok && s->engine_problem[0])
        dxl_paragraph_typed(ui, note, DXL_FONT_SMALL, s->engine_problem, t->bad, s->page_since);
    else if (s->note[0]) dxl_paragraph_typed(ui, note, DXL_FONT_SMALL, s->note, t->good, s->page_since);

    if (game_fix) dxl_shell_repoint(s, DXL_PAGE_HOME);
    else if (play || (ui->in.start && playable)) s->result = 1;
    else if (settings) dxl_shell_go(s, DXL_PAGE_SETTINGS);
    else if (profiles) dxl_shell_go(s, DXL_PAGE_PROFILES);
    else if (about) dxl_shell_go(s, DXL_PAGE_ABOUT);
    else if (quit) s->result = 0;
}
