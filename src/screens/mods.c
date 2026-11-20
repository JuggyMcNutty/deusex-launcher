/* Mods: the mods a profile can be made of (core/mods.h) -- those in the
 * game's folder, in the launcher's mods folder, and the profiles' own --
 * each with where it is, whose settings it starts from and the profiles
 * that play it, and, where the engine can be asked, whether its packages
 * declare natives the engine lacks: native code of its own, which does not
 * play, and of which no profile is made. The one chosen is played with its
 * profile, or has one
 * made, Your profile showing what it is made of before it is; another is
 * shown by the launcher's folder browser or the system's dialog, and is
 * listed while the launcher runs, and by its profile from then on; the
 * launcher's mods folder opens in the desktop's file manager. The rows are
 * the mods; the buttons beside them act on the one chosen, the row last
 * focused. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/argv.h"
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

/* What the audits are of: the engine and the game. Caller frees. */
static char *audit_key(const dxl_shell *s) {
    const dxl_session *ss = &s->run->session;
    const char *game = ss->install.ok ? ss->install.root : "";
    size_t n = strlen(ss->engine) + strlen(game) + 2;
    char *key = dxl_xmalloc(n);
    snprintf(key, n, "%s\n%s", ss->engine, game);
    return key;
}

static void audits_free(dxl_shell *s) {
    for (int i = 0; i < s->mod_audit_count; i++) free(s->mod_audits[i].dir);
    s->mod_audit_count = 0;
}

const dxl_mod_natives *dxl_shell_mod_natives(const dxl_shell *s, const char *dir) {
    if (!s->run || !s->audit_for) return NULL;
    char *key = audit_key(s);
    int same = !strcmp(key, s->audit_for);
    free(key);
    for (int i = 0; same && i < s->mod_audit_count; i++)
        if (!strcmp(s->mod_audits[i].dir, dir)) return &s->mod_audits[i].natives;
    return NULL;
}

/* Whether the engine is to be asked of the mod's natives before it has a
 * profile: one that can be asked, its answer not in yet. */
static int audit_pending(const dxl_shell *s, const dxl_mod *m) {
    return s->run->session.install.ok && dxl_shell_engine_has(s, "audit-natives") && !dxl_shell_mod_natives(s, m->dir);
}

/* What row i says, and the profiles that play its mod. */
static void describe(dxl_shell *s, int i) {
    dxl_session *ss = &s->run->session;
    const dxl_mod *m = &s->mods.items[i];
    char shown[1024], who[160], natives[600];
    char ids[8][DXL_PROFILE_ID_MAX + 1];
    size_t n = dxl_session_mod_profiles(ss, m->dir, ids, 8);
    s->mod_rows[i].profiles = (int)n;
    s->mod_rows[i].profile[0] = s->mod_rows[i].profile_name[0] = '\0';
    if (n) {
        snprintf(s->mod_rows[i].profile, sizeof s->mod_rows[i].profile, "%s", ids[0]);
        dxl_profile p;
        int opened = dxl_profile_open(&p, &ss->xdg, ids[0], NULL) == 0;
        const char *name = opened ? dxl_profile_get(&p, "Name") : NULL;
        const char *shown_name = name && *name ? name : ids[0];
        snprintf(s->mod_rows[i].profile_name, sizeof s->mod_rows[i].profile_name, "%.*s",
                 dxl_utf8_fit(shown_name, 95), shown_name);
        if (opened) dxl_profile_free(&p);
        if (n == 1) snprintf(who, sizeof who, "its profile: %s", s->mod_rows[i].profile_name);
        else snprintf(who, sizeof who, "%d profiles play it", (int)n);
    } else {
        snprintf(who, sizeof who, "no profile yet");
    }
    const char *dir = dxl_shell_tilde(m->dir, shown, sizeof shown);
    /* A mod the engine's audit found native code of its own in: that it
     * does not play, first (bugs.md LCORE.27). */
    dxl_mod_natives_text(dxl_shell_mod_natives(s, m->dir), natives, sizeof natives);
    s->mod_rows[i].plays_not = natives[0] != '\0';
    if (natives[0])
        snprintf(s->mod_rows[i].detail, sizeof s->mod_rows[i].detail, "%.*s  \xc2\xb7  %s, %.*s",
                 dxl_utf8_fit(natives, 300), natives, dxl_mod_where_text(m->where), dxl_utf8_fit(dir, 200), dir);
    else
        snprintf(s->mod_rows[i].detail, sizeof s->mod_rows[i].detail, "%s, %.*s  \xc2\xb7  %s  \xc2\xb7  %s",
                 dxl_mod_where_text(m->where), dxl_utf8_fit(dir, 300), dir,
                 m->ini ? "its own settings" : "the game's settings", who);
}

/* The rows of the mod at dir said again. */
static void describe_dir(dxl_shell *s, const char *dir) {
    for (int i = 0; i < s->mods.count; i++)
        if (!strcmp(s->mods.items[i].dir, dir)) describe(s, i);
}

/* The answer for the mod being asked kept, in place of one kept before. */
static void audit_keep(dxl_shell *s, const dxl_mod_natives *n) {
    char *dir = s->auditing;
    s->auditing = NULL;
    if (!dir) return;
    int at = -1;
    for (int i = 0; i < s->mod_audit_count; i++)
        if (!strcmp(s->mod_audits[i].dir, dir)) at = i;
    if (at < 0) {
        if (s->mod_audit_count == DXL_MODS_MAX) audits_free(s);
        at = s->mod_audit_count++;
        s->mod_audits[at].dir = dxl_xstrdup(dir);
    }
    if (n) s->mod_audits[at].natives = *n;
    else memset(&s->mod_audits[at].natives, 0, sizeof s->mod_audits[at].natives);
    describe_dir(s, dir);
    free(dir);
}

void dxl_shell_audit_next(dxl_shell *s) {
    if (!s->run || s->auditing || s->page != DXL_PAGE_MODS || !s->run->session.install.ok ||
        !dxl_shell_engine_has(s, "audit-natives"))
        return;
    /* Another engine or game: the audits kept are not its. */
    char *key = audit_key(s);
    if (!s->audit_for || strcmp(s->audit_for, key) != 0) {
        audits_free(s);
        free(s->audit_for);
        s->audit_for = key;
    } else {
        free(key);
    }
    for (;;) {
        const dxl_mod *next = NULL;
        for (int i = 0; i < s->mods.count && !next; i++)
            if (!dxl_shell_mod_natives(s, s->mods.items[i].dir)) next = &s->mods.items[i];
        if (!next) return;
        char **argv = dxl_session_audit_argv(&s->run->session, s->run->session.install.root, next);
        s->auditing = dxl_xstrdup(next->dir);
        if (s->start_audit && s->start_audit(s, argv) == 0) {
            dxl_argv_free(argv);
            describe_dir(s, s->auditing);
            return;
        }
        dxl_mod_natives n;
        int ok = dxl_session_audit_run(argv, NULL, &n) == 0;
        dxl_argv_free(argv);
        audit_keep(s, ok ? &n : NULL);
    }
}

void dxl_shell_audited(dxl_shell *s, const dxl_mod_natives *n) {
    audit_keep(s, n);
    dxl_shell_audit_next(s);
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
    dxl_shell_audit_next(s);
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
        dxl_shell_audit_next(s);
    }
    s->mod_sel = at;
    dxl_shell_toast(s, "%s: a mod, with %d package%s.", m->name, m->packages, m->packages == 1 ? "" : "s");
}

/* Its profile made, Your profile first: on the game the profile in use
 * plays, the one before it gone back to if it is not made; none of a mod
 * that does not play. */
void dxl_shell_make_mod(dxl_shell *s, const dxl_mod *m) {
    dxl_session *ss = &s->run->session;
    /* None made of a mod with native code of its own, which cannot play,
     * nor before the engine has said whether it has (bugs.md LCORE.27). */
    const dxl_mod_natives *natives = dxl_shell_mod_natives(s, m->dir);
    if (dxl_mod_natives_lack(natives)) {
        char why[600];
        dxl_mod_natives_text(natives, why, sizeof why);
        dxl_shell_message(s, "This mod does not play",
                          "%s: it %s. DXEngine plays the mods with no native code of their own.", m->name, why);
        return;
    }
    if (audit_pending(s, m)) {
        dxl_shell_toast(s, "The engine is still reading %s's packages: a moment.", m->name);
        return;
    }
    dxl_install in;
    if (!ss->install.ok || dxl_install_check(ss->install.root, &in) != 0) {
        if (ss->install.ok) dxl_install_free(&in);
        dxl_shell_message(s, "No game to play it on",
                          "A mod plays on the game, and the profile in use has none to play: choose its game first, "
                          "on Settings' Launcher page.");
        return;
    }
    char id[DXL_PROFILE_ID_MAX + 1];
    if (dxl_profile_new_id(&ss->xdg, m->name, id, sizeof id) != 0) {
        dxl_install_free(&in);
        dxl_shell_message(s, "No new profile", "Every id for a profile named %s is taken.", m->name);
        return;
    }
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
    snprintf(s->begin_name, sizeof s->begin_name, "%.*s", dxl_utf8_fit(m->name, 95), m->name);
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
            int hits = dxl_list_row(ui, &l, i, s->mods.items[i].name, s->mod_rows[i].detail,
                                    s->mod_rows[i].plays_not ? t->bad : t->help, DXL_W_MAY_CUT);
            if ((hits & DXL_HIT_FOCUSED) && s->mod_sel != i) s->mod_sel = i;
            if (hits & DXL_HIT_FOCUSED)
                why = s->mod_rows[i].profiles  ? "Enter or a click plays with its profile from now on."
                      : s->mod_rows[i].plays_not ? "It has native code of its own, which DXEngine does not have: "
                                                   "it does not play."
                      : audit_pending(s, &s->mods.items[i]) ? "The engine reads its packages for native code of "
                                                              "its own first: a moment."
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
    /* A profile is made of one the engine has found no native code of its
     * own in, or cannot be asked of. */
    int can_make = sel >= 0 && !s->mod_rows[sel].plays_not && !audit_pending(s, &s->mods.items[sel]);

    /* What acts on it. */
    int b_use = dxl_button(ui, "mods use", dxl_cut_top(&side, 48), "Use its profile",
                           DXL_W_LEFT | (has_profile ? DXL_W_PRIMARY : DXL_W_DISABLED));
    dxl_cut_top(&side, 10);
    int b_make = dxl_button(ui, "mods make", dxl_cut_top(&side, 48), "Make its profile...",
                            DXL_W_LEFT | (!can_make ? DXL_W_DISABLED : has_profile ? 0 : DXL_W_PRIMARY));
    dxl_cut_top(&side, 10);
    int b_browse = dxl_button(ui, "mods browse", dxl_cut_top(&side, 48), "Browse...", DXL_W_LEFT);
    dxl_cut_top(&side, 10);
    int b_dialog = dxl_button(ui, "mods system", dxl_cut_top(&side, 48), "The system's dialog...", DXL_W_LEFT);
    dxl_cut_top(&side, 10);
    int b_folder = dxl_button(ui, "mods folder", dxl_cut_top(&side, 48), "The mods folder", DXL_W_LEFT);
    int b_back = dxl_button(ui, "mods back", dxl_cut_bottom(&side, 48), "Back", DXL_W_LEFT);
    if (ui->focus == dxl_ui_id(ui, "mods use") && has_profile) {
        snprintf(text, sizeof text, "%s, its profile, played from now on.", s->mod_rows[sel].profile_name);
        why = text;
    } else if (ui->focus == dxl_ui_id(ui, "mods make")) {
        why = "A profile of its own, on the game the profile in use plays: the mod's folders before the game's, "
              "its own settings and saves.";
    } else if (ui->focus == dxl_ui_id(ui, "mods browse") || ui->focus == dxl_ui_id(ui, "mods system")) {
        why = "A mod's folder anywhere else, the one with its System folder in it.";
    } else if (ui->focus == dxl_ui_id(ui, "mods folder")) {
        why = "The launcher's own mods folder, in the desktop's file manager: a mod's folder put in it is listed "
              "here.";
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
        else dxl_shell_make_mod(s, &s->mods.items[act]);
    } else if (b_use && has_profile) {
        dxl_shell_use_profile(s, s->mod_rows[sel].profile, s->mod_rows[sel].profile_name);
    } else if (b_make && can_make) {
        dxl_shell_make_mod(s, &s->mods.items[sel]);
    } else if (b_browse) {
        dxl_browser_open(s, sel >= 0 ? s->mods.items[sel].dir : dxl_fs_home(), DXL_BROWSE_MOD);
    } else if (b_dialog) {
        dxl_shell_pick_folder(s, DXL_BROWSE_MOD);
    } else if (b_folder) {
        char *folder = dxl_session_mods_dir(ss);
        dxl_path_mkdirs(folder);
        dxl_shell_open(s, folder);
        free(folder);
    } else if (b_back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_PROFILES);
    }
}
