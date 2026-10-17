/* Profiles: every profile the launcher keeps, each with its game, its
 * saves and its last run, the one in use marked. One is used from now on;
 * a new one made from an install, as the first run makes one (The game,
 * then Your profile, its Back the way out); one's settings duplicated, not
 * its saves; one renamed; one deleted -- its four folders to the desktop's
 * trash, never the one in use. The rows are the profiles; the buttons
 * beside them act on the one chosen, the row last focused. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/lastrun.h"
#include "core/log.h"
#include "core/paths.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROW_H  58
#define SIDE_W 260

/* A profile's line under its name: its game, its saves, its last run. 1
 * when its game is there to play. */
static int describe(dxl_shell *s, const dxl_profile *p, char *out, size_t size) {
    const char *path = dxl_session_install_path(&s->run->session, dxl_profile_get(p, "Install"));
    dxl_install in;
    memset(&in, 0, sizeof in);
    int ok = path && dxl_install_check(path, &in) == 0;
    char game[200], saves[32], last[160];
    if (ok) snprintf(game, sizeof game, "Deus Ex %s (%s)", in.build->version, in.build->edition);
    else snprintf(game, sizeof game, "%.190s", !path ? "no game" : in.problem[0] ? in.problem : "its game is not there");
    if (path) dxl_install_free(&in);
    int n = dxl_profile_count_saves(p);
    if (n == 1) snprintf(saves, sizeof saves, "1 save");
    else if (n) snprintf(saves, sizeof saves, "%d saves", n);
    else snprintf(saves, sizeof saves, "no saves");
    dxl_lastrun l;
    dxl_lastrun_load(&l, p->state_dir);
    if (!l.known) {
        snprintf(last, sizeof last, "never played");
    } else {
        char date[64];
        time_t t = (time_t)l.ended;
        struct tm tm;
        if (!localtime_r(&t, &tm) || !strftime(date, sizeof date, "%e %B %Y", &tm)) snprintf(date, sizeof date, "?");
        snprintf(last, sizeof last, l.clean ? "last played %s" : "its last run did not end cleanly, %s",
                 date[0] == ' ' ? date + 1 : date);
    }
    snprintf(out, size, "%.150s  \xc2\xb7  %s  \xc2\xb7  %.100s", game, saves, last);
    return ok;
}

/* The profiles again from their folders, the one of id chosen (NULL: the
 * one chosen before, else the one in use). */
static void load(dxl_shell *s, const char *id) {
    dxl_session *ss = &s->run->session;
    char keep[DXL_PROFILE_ID_MAX + 1];
    snprintf(keep, sizeof keep, "%s",
             id ? id : s->profile_sel < s->profile_count ? s->profiles[s->profile_sel].id : ss->profile.id);
    static char ids[DXL_PROFILES_MAX][DXL_PROFILE_ID_MAX + 1];
    size_t n = dxl_profiles_list(&ss->xdg, ids, DXL_PROFILES_MAX);
    if (n > DXL_PROFILES_MAX) n = DXL_PROFILES_MAX;
    s->profile_count = 0;
    s->profile_sel = 0;
    for (size_t i = 0; i < n; i++) {
        dxl_profile p;
        if (dxl_profile_open(&p, &ss->xdg, ids[i], NULL) != 0) continue;
        int k = s->profile_count++;
        snprintf(s->profiles[k].id, sizeof s->profiles[k].id, "%.64s", ids[i]);
        const char *name = dxl_profile_get(&p, "Name");
        snprintf(s->profiles[k].name, sizeof s->profiles[k].name, "%.95s", name && *name ? name : ids[i]);
        s->profiles[k].ok = describe(s, &p, s->profiles[k].detail, sizeof s->profiles[k].detail);
        s->profiles[k].active = !strcmp(ids[i], ss->profile.id);
        if (!strcmp(ids[i], keep)) s->profile_sel = k;
        dxl_profile_free(&p);
    }
    if (s->profile_count)
        snprintf(s->profile_name, sizeof s->profile_name, "%s", s->profiles[s->profile_sel].name);
}

void dxl_page_profiles_enter(dxl_shell *s) {
    if (!s->run) return;
    load(s, NULL);
}

/* A new profile: The game, then Your profile, for one not made yet; the
 * one in use before it kept, to go back to. */
static void start_new(dxl_shell *s) {
    dxl_session *ss = &s->run->session;
    char name[96], id[DXL_PROFILE_ID_MAX + 1];
    snprintf(name, sizeof name, "Deus Ex %d", s->profile_count + 1);
    dxl_profile_new_id(&ss->xdg, name, id, sizeof id);
    snprintf(s->making_before, sizeof s->making_before, "%s", ss->profile.id);
    dxl_err e;
    if (dxl_run_use_profile(s->run, id, &e) != 0) {
        dxl_shell_message(s, "No new profile", "%s", dxl_err_msg(&e));
        return;
    }
    s->making = 1;
    snprintf(s->begin_name, sizeof s->begin_name, "%s", name);
    if (s->has_chosen) dxl_install_free(&s->chosen);
    s->has_chosen = 0;
    s->looked = 0;
    s->focus[DXL_PAGE_FIND] = 0;
    dxl_shell_go(s, DXL_PAGE_FIND);
}

void dxl_shell_making_cancel(dxl_shell *s) {
    dxl_err e;
    if (dxl_run_use_profile(s->run, s->making_before, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    s->making = 0;
    dxl_log("profile: none made");
    dxl_shell_go(s, DXL_PAGE_PROFILES);
}

void dxl_page_profiles(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    float w = r.w < 1160 ? r.w : 1160;
    dxl_rect area = { r.x, r.y + 6, w - SIDE_W - 20, r.h - 6 };
    dxl_rect side = { r.x + w - SIDE_W, r.y + 6, SIDE_W, r.h - 6 };
    dxl_rect help_r = dxl_cut_bottom(&area, 36);
    dxl_cut_bottom(&area, 6);
    dxl_rect name_r = dxl_cut_bottom(&area, 46);
    dxl_cut_bottom(&area, 10);

    /* The profiles. */
    dxl_well(ui, area);
    dxl_list l;
    dxl_list_begin(ui, &l, "profiles", dxl_inset(area, 4), s->profile_count, ROW_H, 0);
    int use = -1, editing = ui->text_id != 0;
    const char *why = NULL;
    for (int i = 0; i < s->profile_count; i++) {
        char title[160];
        snprintf(title, sizeof title, "%s%s", s->profiles[i].name, s->profiles[i].active ? "  \xc2\xb7  in use" : "");
        int hits = dxl_list_row(ui, &l, i, title, s->profiles[i].detail, s->profiles[i].ok ? t->help : t->warn,
                                DXL_W_MAY_CUT);
        if ((hits & DXL_HIT_FOCUSED) && s->profile_sel != i && !editing) {
            s->profile_sel = i;
            snprintf(s->profile_name, sizeof s->profile_name, "%s", s->profiles[i].name);
        }
        if (hits & DXL_HIT_FOCUSED) why = "Enter or a click plays with this profile from now on.";
        if (hits & DXL_HIT_ACTIVATED) use = i;
        /* The one chosen, as the focus is on what acts on it. */
        dxl_rect rr;
        if (i == s->profile_sel && !(hits & DXL_HIT_FOCUSED) && dxl_list_rect(&l, i, &rr))
            dxl_fill(ui, (dxl_rect){ rr.x + 4, rr.y + 8, 3, rr.h - 16 }, t->accent);
    }
    dxl_list_end(ui, &l);
    int sel = s->profile_sel < s->profile_count ? s->profile_sel : -1;
    int sel_active = sel >= 0 && s->profiles[sel].active;

    /* Its name, edited in place. */
    uint32_t name_id = dxl_ui_id(ui, "profiles name");
    int kept = dxl_shell_answer(s, name_id) == 1;
    int named = dxl_text_field(ui, name_id, name_r, "Name", s->profile_name, sizeof s->profile_name, NULL,
                               sel < 0 ? DXL_W_DISABLED : 0);
    if (named == DXL_TEXT_OSK) dxl_shell_osk(s, name_id, "The profile's name", s->profile_name, sizeof s->profile_name);
    if (ui->focus == name_id) why = "The profile's name, as Home and this page show it.";
    if (sel >= 0 && (kept || named == DXL_TEXT_DONE)) {
        char *name = dxl_trim(s->profile_name);
        dxl_err e;
        if (!*name) {
            snprintf(s->profile_name, sizeof s->profile_name, "%s", s->profiles[sel].name);
        } else if (strcmp(name, s->profiles[sel].name) != 0) {
            char clean[96];
            snprintf(clean, sizeof clean, "%s", name);
            if (dxl_session_rename_profile(ss, s->profiles[sel].id, clean, &e) != 0)
                dxl_shell_message(s, "Not renamed", "%s", dxl_err_msg(&e));
            else
                dxl_log("profile %s: named %s", s->profiles[sel].id, clean);
            load(s, s->profiles[sel].id);
        }
    }

    /* What acts on it. */
    int b_use = dxl_button(ui, "profiles use", dxl_cut_top(&side, 48), "Use this one",
                           DXL_W_PRIMARY | DXL_W_LEFT | (sel < 0 || sel_active ? DXL_W_DISABLED : 0));
    dxl_cut_top(&side, 10);
    int b_new = dxl_button(ui, "profiles new", dxl_cut_top(&side, 48), "New profile...", DXL_W_LEFT);
    dxl_cut_top(&side, 10);
    int b_copy = dxl_button(ui, "profiles copy", dxl_cut_top(&side, 48), "Duplicate",
                            DXL_W_LEFT | (sel < 0 ? DXL_W_DISABLED : 0));
    dxl_cut_top(&side, 10);
    uint32_t del_id = dxl_ui_id(ui, "profiles delete");
    int b_delete = dxl_button_id(ui, del_id, dxl_cut_top(&side, 48), "Delete...",
                                 DXL_W_LEFT | (sel < 0 || sel_active ? DXL_W_DISABLED : 0));
    int b_back = dxl_button(ui, "profiles back", dxl_cut_bottom(&side, 48), "Back", DXL_W_LEFT);
    if (ui->focus == dxl_ui_id(ui, "profiles new"))
        why = "A profile of its own, made from an install as the first run makes one: its own settings, saves "
              "and screenshots.";
    else if (ui->focus == dxl_ui_id(ui, "profiles copy"))
        why = "A new profile with the chosen one's settings, keys and controller: none of its saves.";
    else if (ui->focus == del_id)
        why = "The chosen profile's settings, saves and screenshots to the trash, to be put back from there.";
    if (why) dxl_paragraph(ui, help_r, DXL_FONT_SMALL, why, t->help);

    if (dxl_shell_answer(s, del_id) == 1 && sel >= 0 && !sel_active) {
        dxl_err e;
        char name[96];
        snprintf(name, sizeof name, "%s", s->profiles[sel].name);
        if (dxl_session_delete_profile(ss, s->profiles[sel].id, &e) != 0) {
            dxl_shell_message(s, "Not deleted", "%s", dxl_err_msg(&e));
        } else {
            dxl_shell_toast(s, "%s is in the trash.", name);
            load(s, ss->profile.id);
        }
        return;
    }
    if (use >= 0 || (b_use && sel >= 0)) {
        int i = use >= 0 ? use : sel;
        dxl_err e;
        if (s->profiles[i].active) {
            dxl_shell_go(s, DXL_PAGE_HOME);
            return;
        }
        if (dxl_run_use_profile(s->run, s->profiles[i].id, &e) != 0) {
            dxl_shell_message(s, "Not changed", "%s", dxl_err_msg(&e));
            return;
        }
        /* Another profile's start: the next run's words were the other's,
         * and one whose last run left its Running.ini opens on Recovery, as
         * a start of it does. */
        s->note[0] = s->next_run[0] = '\0';
        dxl_shell_toast(s, "The profile is %s.", s->profiles[i].name);
        if (dxl_sentinel_exists(&s->run->sentinel)) {
            s->run->decision.screen = DXL_SCREEN_MAIN_RECOVERY;
            dxl_shell_go(s, DXL_PAGE_RECOVERY);
        } else {
            dxl_shell_go(s, DXL_PAGE_HOME);
        }
    } else if (b_new) {
        start_new(s);
    } else if (b_copy && sel >= 0) {
        char name[110], id[DXL_PROFILE_ID_MAX + 1];
        snprintf(name, sizeof name, "%.80s (copy)", s->profiles[sel].name);
        dxl_err e;
        if (dxl_session_copy_profile(ss, s->profiles[sel].id, name, id, sizeof id, &e) != 0) {
            dxl_shell_message(s, "Not duplicated", "%s", dxl_err_msg(&e));
        } else {
            dxl_shell_toast(s, "%s: its settings, none of its saves.", name);
            load(s, id);
        }
    } else if (b_delete && sel >= 0) {
        char title[140], text[400];
        snprintf(title, sizeof title, "Delete %s?", s->profiles[sel].name);
        snprintf(text, sizeof text, "Its settings, its saves and its screenshots go to the trash, and can be put "
                 "back from there.");
        dxl_shell_confirm(s, del_id, title, text, "Delete", "Keep it");
    } else if (b_back || (ui->in.back && !ui->text_id)) {
        dxl_shell_go(s, DXL_PAGE_HOME);
    }
}
