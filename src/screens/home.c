#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/paths.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* The profile's saves: its SaveNNNN folders and QuickSave. */
static int count_saves(const char *dir) {
    DIR *d = dir ? opendir(dir) : NULL;
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (dxl_strnicmp(e->d_name, "Save", 4) == 0 && e->d_name[4] >= '0' && e->d_name[4] <= '9') n++;
        else if (dxl_stricmp(e->d_name, "QuickSave") == 0) n++;
    }
    closedir(d);
    return n;
}

void dxl_page_home_enter(dxl_shell *s) {
    dxl_shell_ask_engine(s);
    s->saves = s->run ? count_saves(s->run->session.profile.save_dir) : 0;
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

void dxl_page_home(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    int playable = ss->install.ok && s->engine_ok;

    float menu_w = r.w * 0.3f;
    if (menu_w < 240) menu_w = 240;
    if (menu_w > 320) menu_w = 320;
    dxl_rect menu = { r.x, r.y + 6, menu_w, r.h - 6 };
    int play = dxl_button(ui, "home play", dxl_cut_top(&menu, 48), "Play",
                          DXL_W_PRIMARY | DXL_W_LEFT | (playable ? 0 : DXL_W_DISABLED));
    dxl_cut_top(&menu, 10);
    int about = dxl_button(ui, "home about", dxl_cut_top(&menu, 48), "About", DXL_W_LEFT);
    dxl_cut_top(&menu, 10);
    int quit = dxl_button(ui, "home quit", dxl_cut_top(&menu, 48), "Quit", DXL_W_LEFT);

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
    dxl_field(ui, &in, lh, nw, "Engine", s->engine_ok ? s->engine_version : s->engine_problem,
              s->engine_ok ? t->text : t->bad, DXL_W_MAY_CUT);
    if (s->saves) snprintf(v, sizeof v, "%d", s->saves);
    else snprintf(v, sizeof v, "none yet");
    dxl_field(ui, &in, lh, nw, "Saves", v, t->text, 0);
    c = last_run(s, v, sizeof v);
    dxl_field(ui, &in, lh, nw, "Last run", v, c, DXL_W_MAY_CUT);

    if (s->note[0]) {
        dxl_rect note = { px, panel.y + panel.h + 14, pw, r.y + r.h - (panel.y + panel.h + 14) };
        dxl_paragraph(ui, note, DXL_FONT_SMALL, s->note, t->good);
    }

    if (play || (ui->in.start && playable)) s->result = 1;
    else if (about) dxl_shell_go(s, DXL_PAGE_ABOUT);
    else if (quit) s->result = 0;
}
