/* Mods: the mods a profile can be made of (core/mods.h) -- those in the
 * game's folder, in the launcher's mods folder, and the profiles' own --
 * each with where it is, whose settings it starts from and the profiles
 * that play it. The one chosen is played with its profile, or has one
 * made, Your profile showing what it is made of before it is; another is
 * shown by the launcher's folder browser, and is listed while the launcher
 * runs, and by its profile from then on. The rows are
 * the mods; the buttons beside them act on the one chosen, the row last
 * focused. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_H  58
#define SIDE_W 260

/* What row i says, and the profiles that play its mod. */
static void describe(dxl_shell *s, int i) {
    dxl_session *ss = &s->run->session;
    const dxl_mod *m = &s->mods.items[i];
    char shown[1024], who[160];
    char ids[8][DXL_PROFILE_ID_MAX + 1];
    size_t n = dxl_session_mod_profiles(ss, m->dir, ids, 8);
    s->mod_rows[i].profiles = (int)n;
    s->mod_rows[i].profile[0] = s->mod_rows[i].profile_name[0] = '\0';
    if (n) {
        snprintf(s->mod_rows[i].profile, sizeof s->mod_rows[i].profile, "%s", ids[0]);
        dxl_profile p;
        int opened = dxl_profile_open(&p, &ss->xdg, ids[0], NULL) == 0;
        const char *name = opened ? dxl_profile_get(&p, "Name") : NULL;
        snprintf(s->mod_rows[i].profile_name, sizeof s->mod_rows[i].profile_name, "%.95s", name && *name ? name : ids[0]);
        if (opened) dxl_profile_free(&p);
        if (n == 1) snprintf(who, sizeof who, "its profile: %s", s->mod_rows[i].profile_name);
        else snprintf(who, sizeof who, "%d profiles play it", (int)n);
    } else {
        snprintf(who, sizeof who, "no profile yet");
    }
    snprintf(s->mod_rows[i].detail, sizeof s->mod_rows[i].detail, "%s, %.300s  \xc2\xb7  %s  \xc2\xb7  %s",
             dxl_mod_where_text(m->where), dxl_shell_tilde(m->dir, shown, sizeof shown),
             m->ini ? "its own settings" : "the game's settings", who);
}

void dxl_page_mods_enter(dxl_shell *s) {
    if (!s->run) return;
    char keep[1024];
    snprintf(keep, sizeof keep, "%s", s->mod_sel < s->mods.count ? s->mods.items[s->mod_sel].dir : "");
    dxl_mod_list_free(&s->mods);
    dxl_session_mods(&s->run->session, s->run->session.install.ok ? s->run->session.install.root : NULL, &s->mods);
    for (int i = 0; i < s->mods_shown.count; i++)
        if (dxl_path_is_dir(s->mods_shown.items[i].dir)) dxl_mod_list_add(&s->mods, &s->mods_shown.items[i]);
    s->mod_sel = 0;
    for (int i = 0; i < s->mods.count; i++) {
        describe(s, i);
        if (!strcmp(s->mods.items[i].dir, keep)) s->mod_sel = i;
    }
    dxl_log("mods: %d found", s->mods.count);
}

void dxl_shell_mod_shown(dxl_shell *s, const dxl_mod *m) {
    int at = -1;
    for (int i = 0; i < s->mods.count; i++)
        if (!strcmp(s->mods.items[i].dir, m->dir)) at = i;
    if (at < 0) {
        if (dxl_mod_list_add(&s->mods, m) != 0) {
            dxl_shell_message(s, "Not listed", "The Mods page lists %d mods at most.", DXL_MODS_MAX);
            return;
        }
        dxl_mod_list_add(&s->mods_shown, m);
        at = s->mods.count - 1;
        describe(s, at);
        dxl_log("mods: %s shown, %s", m->name, m->dir);
    }
    s->mod_sel = at;
    dxl_shell_toast(s, "%s: a mod, with %d package%s.", m->name, m->packages, m->packages == 1 ? "" : "s");
}

/* Its profile made, Your profile first: on the game the profile in use
 * plays, the one before it gone back to if it is not made. */
static void start_mod(dxl_shell *s, const dxl_mod *m) {
    dxl_session *ss = &s->run->session;
    dxl_install in;
    if (!ss->install.ok || dxl_install_check(ss->install.root, &in) != 0) {
        if (ss->install.ok) dxl_install_free(&in);
        dxl_shell_message(s, "No game to play it on",
                          "A mod plays on the game, and the profile in use has none to play: choose its game first, "
                          "on Settings' Launcher page.");
        return;
    }
    char id[DXL_PROFILE_ID_MAX + 1];
    dxl_profile_new_id(&ss->xdg, m->name, id, sizeof id);
    snprintf(s->making_before, sizeof s->making_before, "%s", ss->profile.id);
    dxl_err e;
    if (dxl_run_use_profile(s->run, id, &e) != 0) {
        dxl_install_free(&in);
        dxl_shell_message(s, "No new profile", "%s", dxl_err_msg(&e));
        return;
    }
    s->making = 1;
    dxl_mod_free(&s->making_mod);
    dxl_mod_copy(&s->making_mod, m);
    snprintf(s->begin_name, sizeof s->begin_name, "%.95s", m->name);
    if (s->has_chosen) dxl_install_free(&s->chosen);
    s->chosen = in;
    s->has_chosen = 1;
    memset(&s->plan, 0, sizeof s->plan);
    s->plan_error[0] = '\0';
    if (dxl_profile_import_mod(&ss->profile, s->chosen.system_dir, &s->making_mod, 1, &s->plan, &e) != 0)
        snprintf(s->plan_error, sizeof s->plan_error, "%s", dxl_err_msg(&e));
    dxl_shell_begin_defaults(s);
    s->focus[DXL_PAGE_IMPORT] = 0;
    dxl_shell_go(s, DXL_PAGE_IMPORT);
}

void dxl_page_mods(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    float w = r.w < 1160 ? r.w : 1160;
    dxl_rect area = { r.x, r.y + 6, w - SIDE_W - 20, r.h - 6 };
    dxl_rect side = { r.x + w - SIDE_W, r.y + 6, SIDE_W, r.h - 6 };
    dxl_rect help_r = dxl_cut_bottom(&area, 54);
    dxl_cut_bottom(&area, 6);

    /* The mods; with none, where they would be. */
    int act = -1;
    const char *why = NULL;
    char text[1600], shown[1024];
    if (!s->mods.count) {
        char *folder = dxl_session_mods_dir(ss);
        snprintf(text, sizeof text,
                 "No mods here: none in the game's folder, nor in the launcher's mods folder, %s. A mod is a "
                 "folder of its own with a System folder of its packages: put one there, or show DXLauncher "
                 "where one is.",
                 dxl_shell_tilde(folder, shown, sizeof shown));
        free(folder);
        dxl_paragraph(ui, area, DXL_FONT_BODY, text, t->text);
    } else {
        dxl_well(ui, area);
        dxl_list l;
        dxl_list_begin(ui, &l, "mods", dxl_inset(area, 4), s->mods.count, ROW_H, 0);
        for (int i = 0; i < s->mods.count; i++) {
            int hits = dxl_list_row(ui, &l, i, s->mods.items[i].name, s->mod_rows[i].detail, t->help,
                                    DXL_W_MAY_CUT);
            if ((hits & DXL_HIT_FOCUSED) && s->mod_sel != i) s->mod_sel = i;
            if (hits & DXL_HIT_FOCUSED)
                why = s->mod_rows[i].profiles ? "Enter or a click plays with its profile from now on."
                                              : "Enter or a click makes it a profile of its own.";
            if (hits & DXL_HIT_ACTIVATED) act = i;
            dxl_rect rr;
            if (i == s->mod_sel && !(hits & DXL_HIT_FOCUSED) && dxl_list_rect(&l, i, &rr))
                dxl_fill(ui, (dxl_rect){ rr.x + 4, rr.y + 8, 3, rr.h - 16 }, t->accent);
        }
        dxl_list_end(ui, &l);
    }
    int sel = s->mod_sel < s->mods.count ? s->mod_sel : -1;
    int has_profile = sel >= 0 && s->mod_rows[sel].profiles > 0;

    /* What acts on it. */
    int b_use = dxl_button(ui, "mods use", dxl_cut_top(&side, 48), "Use its profile",
                           DXL_W_LEFT | (has_profile ? DXL_W_PRIMARY : DXL_W_DISABLED));
    dxl_cut_top(&side, 10);
    int b_make = dxl_button(ui, "mods make", dxl_cut_top(&side, 48), "Make its profile...",
                            DXL_W_LEFT | (sel < 0 ? DXL_W_DISABLED : has_profile ? 0 : DXL_W_PRIMARY));
    dxl_cut_top(&side, 10);
    int b_browse = dxl_button(ui, "mods browse", dxl_cut_top(&side, 48), "Browse...", DXL_W_LEFT);
    int b_back = dxl_button(ui, "mods back", dxl_cut_bottom(&side, 48), "Back", DXL_W_LEFT);
    if (ui->focus == dxl_ui_id(ui, "mods use") && has_profile) {
        snprintf(text, sizeof text, "%s, its profile, played from now on.", s->mod_rows[sel].profile_name);
        why = text;
    } else if (ui->focus == dxl_ui_id(ui, "mods make")) {
        why = "A profile of its own, on the game the profile in use plays: the mod's folders before the game's, "
              "its own settings and saves.";
    } else if (ui->focus == dxl_ui_id(ui, "mods browse")) {
        why = "A mod's folder anywhere else, the one with its System folder in it.";
    }
    /* A mod's text is its own .int files', which only an engine with
     * int-paths reads from its folder. */
    if (s->engine_ok && !dxl_shell_engine_has(s, "int-paths"))
        dxl_paragraph(ui, help_r, DXL_FONT_SMALL,
                      "This engine reads a mod's text from the game's System folder alone, so a mod's own text is "
                      "missing: a DXEngine with the int-paths feature reads it from the mod's folder.",
                      t->warn);
    else if (why)
        dxl_paragraph_typed(ui, help_r, DXL_FONT_SMALL, why, t->help, ui->focus_since);

    if (act >= 0) {
        s->mod_sel = act;
        if (s->mod_rows[act].profiles) dxl_shell_use_profile(s, s->mod_rows[act].profile, s->mod_rows[act].profile_name);
        else start_mod(s, &s->mods.items[act]);
    } else if (b_use && has_profile) {
        dxl_shell_use_profile(s, s->mod_rows[sel].profile, s->mod_rows[sel].profile_name);
    } else if (b_make && sel >= 0) {
        start_mod(s, &s->mods.items[sel]);
    } else if (b_browse) {
        dxl_browser_open(s, sel >= 0 ? s->mods.items[sel].dir : dxl_fs_home(), DXL_BROWSE_MOD);
    } else if (b_back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_PROFILES);
    }
}
