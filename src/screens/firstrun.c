/* The first run's pages: Welcome; The game, which the profile is made
 * from; Your profile, what making it will do, shown before it is done --
 * a new profile's too, and a mod's (screens/mods.c). */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/engine_settings.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "ui/controls.h"
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
        "DXLauncher plays your copy of Deus Ex on DXEngine, natively on this computer.",
        ("It keeps its own copy of the game's settings, and your saves and screenshots, in your home "
         "folder. The game's folder is never written to."),
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
    dxl_shell_find(s);
}

void dxl_shell_find(dxl_shell *s) {
    const char *known[17];
    size_t n = dxl_session_installs(&s->run->session, known, 16);
    known[n < 16 ? n : 16] = NULL;
    dxl_found_list_free(&s->found);
    s->looked = s->finding = s->find_focus = 1;
    dxl_finder_where w;
    memset(&w, 0, sizeof w);
    w.given = s->run->opt.game;
    w.known = known;
    if (s->start_find) {
        s->start_find(s, &w);
        return;
    }
    dxl_finder_find(&s->found, &w);
    dxl_shell_found_all(s);
}

void dxl_shell_found(dxl_shell *s, const dxl_found *f) {
    if (dxl_found_list_add(&s->found, f) == 0) dxl_log("the finder: %s, %s", f->install.root, f->source);
}

void dxl_shell_found_all(dxl_shell *s) {
    s->finding = 0;
    dxl_log("the finder: %d install(s) found", s->found.count);
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
    /* The player's own move, the pointer's or a click: the focus theirs
     * from now on. */
    if (ui->focus_moved || ui->in.pointer_moved || ui->in.pressed || ui->in.activate) s->find_focus = 0;
    if (s->found.count == 0) {
        dxl_paragraph(ui, in, DXL_FONT_BODY,
                      s->finding ? "Looking for it in spruceOS's Ports folder, Roms/PORTS on the card, and on the "
                                   "drives mounted here..."
                                 : "It is not in spruceOS's Ports folder, Roms/PORTS on the card, nor on the drives "
                                   "mounted here. Show DXLauncher the folder the game is in, the one with System and "
                                   "Maps in it.",
                      t->text);
    } else {
        float hint = dxl_paragraph(ui, in, DXL_FONT_SMALL,
                                   s->finding ? "Choose the copy to play, or wait: the launcher is still looking."
                                              : "Choose the copy to play:",
                                   t->help);
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
        /* The first found has the focus, though it came after the page did,
         * until the player moves it. */
        if (s->find_focus) {
            s->find_focus = 0;
            dxl_list_focus(ui, &l, 0);
        }
        dxl_list_end(ui, &l);
    }

    int browse = dxl_button(ui, "find browse", dxl_cut_left(&buttons, 170), "Browse...",
                            s->found.count ? 0 : DXL_W_PRIMARY);
    int quits = s->repoint && !s->repoint_back;
    int back = dxl_button(ui, "find back", dxl_cut_right(&buttons, 150), quits ? "Quit" : "Back", 0);
    if (s->making && (back || ui->in.back)) {
        dxl_shell_making_cancel(s);
        return;
    }

    if (picked >= 0) {
        const dxl_install *g = &s->found.items[picked].install;
        char shown[1024];
        if (g->ok) dxl_shell_choose(s, g);
        else dxl_shell_message(s, "Not one to play", "%s: %s.", dxl_shell_tilde(g->root, shown, sizeof shown), g->problem);
    } else if (browse) {
        const char *start = s->has_chosen ? s->chosen.root : dxl_fs_home();
        dxl_browser_open(s, start, DXL_BROWSE_GAME);
    } else if (quits) {
        if (back) s->result = 0;
    } else if (s->repoint && (back || ui->in.back)) {
        s->repoint = s->repoint_back = 0;
        dxl_shell_go(s, s->repoint_to);
    } else if (back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_WELCOME);
    }
}

/* ---- Your profile -------------------------------------------------- */

/* A new profile's name kept: its id after the name, the profile not made
 * yet that one, and what making it will do again. */
static void renamed(dxl_shell *s) {
    dxl_session *ss = &s->run->session;
    char id[DXL_PROFILE_ID_MAX + 1];
    dxl_profile_new_id(&ss->xdg, s->begin_name, id, sizeof id);
    if (!strcmp(id, ss->profile.id)) return;
    dxl_err e;
    if (dxl_run_use_profile(s->run, id, &e) != 0) {
        dxl_log("warning: %s", dxl_err_msg(&e));
        return;
    }
    memset(&s->plan, 0, sizeof s->plan);
    s->plan_error[0] = '\0';
    int planned = s->making_mod.dir
                      ? dxl_profile_import_mod(&ss->profile, s->chosen.system_dir, &s->making_mod, 1, &s->plan, &e)
                      : dxl_profile_import(&ss->profile, s->chosen.system_dir, 1, &s->plan, &e);
    if (planned != 0) snprintf(s->plan_error, sizeof s->plan_error, "%s", dxl_err_msg(&e));
}

/* A file of the install's, by its path in the install; of the mod's, by
 * its path in the mod's folder, after the mod's name. */
static const char *in_install(const dxl_shell *s, const char *path, char *buf, size_t size) {
    const char *root = s->chosen.root, *mod = s->making_mod.dir;
    size_t n = root ? strlen(root) : 0, k = mod ? strlen(mod) : 0;
    if (k && strncmp(path, mod, k) == 0 && path[k] == '/') {
        snprintf(buf, size, "%s/%s", s->making_mod.name, path + k + 1);
        return buf;
    }
    if (n && strncmp(path, root, n) == 0 && path[n] == '/') return path + n + 1;
    return dxl_shell_tilde(path, buf, size);
}

/* A profile begun on a machine whose GPU has no driver the GPU renderers
 * can use, as the probe found it (dxl_shell_no_gpu): on the software
 * renderer, which needs none. 1 when it was. */
static int begin_on_software(dxl_shell *s) {
    if (!dxl_shell_no_gpu(s)) return 0;
    char *path = dxl_path_join(s->run->session.profile.engine_dir, "Settings.json");
    char *def = dxl_path_join(s->run->session.data_dir, "engine-settings.json.default");
    dxl_engine_settings *es = dxl_es_open(path, def);
    dxl_err e;
    int done = es && dxl_es_set_choice(es, DXL_ES_RENDER_TYPE, "Software") == 0 && dxl_es_save(es, &e) == 0;
    if (done) dxl_log("renderer: Software, the probe having found no GPU driver");
    dxl_es_free(es);
    free(def);
    free(path);
    return done;
}

void dxl_page_import(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_import *p = &s->plan;
    const dxl_mod *mod = s->making_mod.dir ? &s->making_mod : NULL;
    /* What the machine draws with, known by the time the profile is made */
    dxl_shell_probe(s);
    float w = r.w < 900 ? r.w : 900;
    char title[1100], shown[1024], v[1200], buf[1024];
    snprintf(title, sizeof title, "From %s", dxl_shell_tilde(mod ? mod->dir : s->chosen.root, shown, sizeof shown));
    char cut[1100];
    dxl_text_fit_tail(ui, DXL_FONT_BUTTON, title, w - 40, cut, sizeof cut);
    const float nw = 150, ch = 40;
    /* Where the profile is kept, under its name and looks: as many lines
     * as the folder's path wraps to. */
    char kept_in[1200];
    snprintf(kept_in, sizeof kept_in, "Nothing in the game's folder%s changes: the profile is kept in %s.",
             mod ? " or the mod's" : "", dxl_shell_tilde(s->run->session.profile.dir, buf, sizeof buf));
    /* How it begins, on lines under the fields: its name, the graphics,
     * and with a pad connected its layout. The fields' lines closer on a
     * short window. */
    /* A mod's row takes the place of the screenshots'. */
    int pad = SDL_HasGamepad(), rows = 6 + (p->master_replaced != 0);
    float extra = 10 + ch + 10 + ch + 10 + dxl_text_size(ui, DXL_FONT_SMALL, kept_in, w - 44).y,
          room = (r.h - 6 - 133 - extra) / (float)rows;
    float lh = room < 29 ? (room > 22 ? room : 22) : 29;
    float content = s->plan_error[0] ? dxl_text_size(ui, DXL_FONT_BODY, s->plan_error, w - 44).y : rows * lh + extra;
    dxl_rect panel = { r.x, r.y + 6, w, fit(r, content) };
    dxl_rect in = dxl_window(ui, panel, cut);
    dxl_rect buttons = button_row(&in);

    if (s->plan_error[0]) {
        dxl_paragraph(ui, in, DXL_FONT_BODY, s->plan_error, t->bad);
    } else {
        const dxl_install *g = &s->chosen;
        snprintf(v, sizeof v, "Deus Ex %s (%s)", g->build ? g->build->version : "?", g->build ? g->build->edition : "?");
        dxl_field(ui, &in, lh, nw, "The game", v, t->text, DXL_W_MAY_CUT);
        if (mod) {
            snprintf(v, sizeof v, "%s, its folders before the game's", mod->name);
            dxl_field(ui, &in, lh, nw, "The mod", v, t->text, DXL_W_MAY_CUT);
        }
        dxl_field(ui, &in, lh, nw, "Its settings", in_install(s, p->system_from, buf, sizeof buf), t->text,
                  DXL_W_TAIL | DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "Your controls", in_install(s, p->user_from, buf, sizeof buf), t->text,
                  DXL_W_TAIL | DXL_W_MAY_CUT);
        dxl_field(ui, &in, lh, nw, "The engine's", p->engine_from[0] ? in_install(s, p->engine_from, buf, sizeof buf)
                                                                     : "DXEngine's own defaults",
                  t->text, DXL_W_TAIL | DXL_W_MAY_CUT);
        if (p->saves) snprintf(v, sizeof v, "%d to copy", p->saves);
        else snprintf(v, sizeof v, "none to copy");
        dxl_field(ui, &in, lh, nw, "Saves", v, t->text, DXL_W_MAY_CUT);
        if (!mod) {
            if (p->shots) snprintf(v, sizeof v, "%d to copy", p->shots);
            else snprintf(v, sizeof v, "none to copy");
            dxl_field(ui, &in, lh, nw, "Screenshots", v, t->text, DXL_W_MAY_CUT);
        }
        if (p->master_replaced)
            dxl_field(ui, &in, lh, nw, "Master server", "333networks', for GameSpy's, which is closed", t->text,
                      DXL_W_MAY_CUT);
        dxl_cut_top(&in, 10);
        /* Its name; a new profile's folder is named after it. */
        uint32_t name_id = dxl_ui_id(ui, "import name");
        int kept = dxl_shell_answer(s, name_id) == 1;
        int named = dxl_text_field(ui, name_id, dxl_cut_top(&in, ch), "Name", s->begin_name, sizeof s->begin_name,
                                   NULL, 0);
        if (named == DXL_TEXT_OSK) dxl_shell_osk(s, name_id, "The profile's name", s->begin_name, sizeof s->begin_name);
        if (kept || named == DXL_TEXT_DONE) {
            char *name = dxl_trim(s->begin_name);
            if (!*name) snprintf(s->begin_name, sizeof s->begin_name, "Deus Ex");
            else memmove(s->begin_name, name, strlen(name) + 1);
            if (s->making) renamed(s);
        }
        dxl_cut_top(&in, 10);
        static const char *looks[8], *layouts[8];
        int nl = 0, nk = 0;
        looks[nl++] = "The engine's defaults";
        for (size_t i = 0; i < dxl_preset_count() && nl < 8; i++) looks[nl++] = dxl_preset_at(i)->label;
        dxl_rect line = dxl_cut_top(&in, ch), left = line, right = line;
        left.w = right.w = (line.w - 16) / 2;
        right.x = left.x + left.w + 16;
        dxl_spinner(ui, dxl_ui_id(ui, "import graphics"), left, "Graphics", &s->begin_graphics, looks, nl, 0);
        if (pad) {
            layouts[nk++] = "As imported";
            for (size_t i = 0; i < dxl_pad_preset_count() && nk < 8; i++) layouts[nk++] = dxl_pad_preset_at(i)->label;
            dxl_spinner(ui, dxl_ui_id(ui, "import layout"), right, "Controller", &s->begin_layout, layouts, nk, 0);
        }
        dxl_cut_top(&in, 10);
        dxl_paragraph(ui, in, DXL_FONT_SMALL, kept_in, t->help);
    }

    /* Enter makes it, the choices above it as they are. */
    if (!ui->focus && !s->plan_error[0]) ui->focus = dxl_ui_id(ui, "import make");
    int make = dxl_button(ui, "import make", dxl_cut_left(&buttons, 230), "Make the profile",
                          DXL_W_PRIMARY | (s->plan_error[0] ? DXL_W_DISABLED : 0));
    int back = dxl_button(ui, "import back", dxl_cut_right(&buttons, 150), "Back", 0);

    if (make) {
        dxl_import done;
        dxl_err e;
        if (dxl_session_make_profile(&s->run->session, &s->chosen, mod, s->begin_name, &done, &e) != 0) {
            dxl_shell_message(s, "The profile was not made", "%s", dxl_err_msg(&e));
            return;
        }
        const dxl_preset *looks_with = s->begin_graphics > 0 ? dxl_preset_at((size_t)s->begin_graphics - 1) : NULL;
        const dxl_pad_preset *layout = pad && s->begin_layout > 0 ? dxl_pad_preset_at((size_t)s->begin_layout - 1) : NULL;
        if (dxl_session_begin_with(&s->run->session, looks_with, layout, &e) != 0)
            dxl_log("warning: the profile's graphics and layout: %s", dxl_err_msg(&e));
        int software = begin_on_software(s);
        char saves[32], shots[40];
        snprintf(saves, sizeof saves, done.saves == 1 ? "1 save" : "%d saves", done.saves);
        snprintf(shots, sizeof shots, done.shots == 1 ? "1 screenshot" : "%d screenshots", done.shots);
        if (done.saves && done.shots)
            snprintf(s->note, sizeof s->note, "The profile is made: %s and %s copied from the game's folder.", saves, shots);
        else if (done.saves || done.shots)
            snprintf(s->note, sizeof s->note, "The profile is made: %s copied from the %s folder.",
                     done.saves ? saves : shots, mod ? "mod's" : "game's");
        else
            snprintf(s->note, sizeof s->note, "The profile is made.");
        if (software) {
            size_t n = strlen(s->note);
            snprintf(s->note + n, sizeof s->note - n, " It draws on the software renderer: no GPU driver was found.");
        }
        dxl_mod_free(&s->making_mod);
        if (s->making) {
            /* From the Profiles page: the run's profile this one now, Home;
             * the next run's words were the other's. */
            if (dxl_run_use_profile(s->run, s->run->session.profile.id, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
            s->making = 0;
            s->next_run[0] = '\0';
            dxl_shell_theme(s);
            dxl_shell_go(s, DXL_PAGE_HOME);
            return;
        }
        s->result = 1;
    } else if (back || (ui->in.back && !ui->text_id)) {
        if (mod) dxl_shell_making_cancel(s);
        else dxl_shell_go(s, DXL_PAGE_FIND);
    }
}
