#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/process.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_S     0.18f   /* a page slides and fades in */
#define SLIDE_DP   28
#define TOAST_S    3.5
#define TOAST_FADE 0.4
#define FOOT_H     52
#define WIDEST     1360    /* past it, the margins grow */

void dxl_shell_init(dxl_shell *s, const char *version) {
    memset(s, 0, sizeof *s);
    s->version = version;
    s->result = -1;
    s->pad_mesh_family = -1;
    s->pad_find = s->pad_chosen = -1;
}

void dxl_shell_free(dxl_shell *s) {
    dxl_renderers_free(&s->renderers);
    dxl_found_list_free(&s->found);
    if (s->has_chosen) dxl_install_free(&s->chosen);
    s->has_chosen = 0;
    dxl_browser_free(&s->browser);
    free(s->log_tail);
    s->log_tail = NULL;
    dxl_page_log_free(s);
    dxl_padmesh_free(&s->pad_mesh);
    s->pad_mesh_family = -1;
    if (s->wizard.joy) SDL_CloseJoystick(s->wizard.joy);
    dxl_padmesh_free(&s->wizard.mesh);
    memset(&s->wizard, 0, sizeof s->wizard);
    free(s->pad_db);
    s->pad_db = NULL;
}

void dxl_shell_enter(dxl_shell *s, dxl_run *r, dxl_view v) {
    s->run = r;
    s->view = v;
    free(s->pad_db);
    s->pad_db = NULL;
    s->result = -1;
    s->dialog = DXL_DIALOG_NONE;
    s->repoint = v == DXL_VIEW_INSTALL;
    s->repoint_back = 0;
    dxl_page p = DXL_PAGE_HOME;
    switch (v) {
    case DXL_VIEW_FIRST_RUN: p = DXL_PAGE_WELCOME; break;
    case DXL_VIEW_INSTALL: p = DXL_PAGE_FIND; break;
    case DXL_VIEW_RECOVERY: p = DXL_PAGE_RECOVERY; break;
    case DXL_VIEW_GRAPHICS: p = DXL_PAGE_SETTINGS; s->settings_tab = 1; break;
    default: break;
    }
    s->page = p;
    s->focus[p] = 0;
    dxl_shell_go(s, p);
}

void dxl_shell_go(dxl_shell *s, dxl_page p) {
    if (s->page != p) {
        s->focus[s->page] = s->ui.focus;
        s->from = s->page;
        if (s->page == DXL_PAGE_SETTINGS) dxl_settings_keep(s);
    }
    s->page = p;
    s->page_since = -1;   /* from the next frame's start */
    s->ui.focus = s->focus[p];
    dxl_ui_forget(&s->ui);
    switch (p) {
    case DXL_PAGE_HOME: dxl_page_home_enter(s); break;
    case DXL_PAGE_FIND: dxl_page_find_enter(s); break;
    case DXL_PAGE_RECOVERY: dxl_page_recovery_enter(s); break;
    case DXL_PAGE_SETTINGS: dxl_page_settings_enter(s); break;
    case DXL_PAGE_LOG: dxl_page_log_enter(s); break;
    default: break;
    }
}

void dxl_shell_leave(dxl_shell *s) {
    if (s->page == DXL_PAGE_SETTINGS) dxl_settings_keep(s);
}

void dxl_shell_toast(dxl_shell *s, const char *fmt, ...) {
    if (s->toast_count == DXL_TOASTS) {
        memmove(&s->toasts[0], &s->toasts[1], sizeof s->toasts[0] * (DXL_TOASTS - 1));
        s->toast_count--;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->toasts[s->toast_count].text, sizeof s->toasts[0].text, fmt, ap);
    va_end(ap);
    /* From the next frame's start: one made between frames, as the window
     * waits, is not old by its first. */
    s->toasts[s->toast_count].at = -1;
    s->toast_count++;
}

void dxl_shell_message(dxl_shell *s, const char *title, const char *fmt, ...) {
    snprintf(s->message_title, sizeof s->message_title, "%s", title);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->message, sizeof s->message, fmt, ap);
    va_end(ap);
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog = DXL_DIALOG_MESSAGE;
}

void dxl_shell_close_dialog(dxl_shell *s) {
    s->dialog = DXL_DIALOG_NONE;
    s->ui.focus = s->dialog_return;
    dxl_ui_forget(&s->ui);
}

const char *dxl_shell_tilde(const char *path, char *buf, size_t size) {
    const char *home = getenv("HOME");
    size_t n = home ? strlen(home) : 0;
    if (!path) path = "";
    if (n > 1 && strncmp(path, home, n) == 0 && (path[n] == '/' || path[n] == '\0'))
        snprintf(buf, size, "~%s", path + n);
    else
        snprintf(buf, size, "%s", path);
    return buf;
}

void dxl_shell_ask_engine(dxl_shell *s) {
    if (s->engine_asked || !s->run) return;
    s->engine_asked = 1;
    dxl_engine_version v;
    s->engine_ok = dxl_run_engine_check(&s->run->session, &v, s->engine_brief, sizeof s->engine_brief,
                                        s->engine_problem, sizeof s->engine_problem) == 0;
    if (s->engine_ok) {
        snprintf(s->engine_version, sizeof s->engine_version, "%s", v.version);
        snprintf(s->engine_features, sizeof s->engine_features, "%s", v.features);
        s->engine_brief[0] = s->engine_problem[0] = '\0';
    } else {
        dxl_log("%s", s->engine_problem);
    }
}

int dxl_shell_choose_engine(dxl_shell *s, const char *path, char *brief, size_t brief_size, char *why,
                            size_t why_size) {
    char *prog = dxl_path_is_dir(path) ? dxl_engine_in(path) : dxl_xstrdup(path);
    if (!prog) {
        snprintf(brief, brief_size, "No VibeEngine or SurrealEngine in this folder.");
        snprintf(why, why_size, "There is no VibeEngine or SurrealEngine program in %s.", path);
        return 0;
    }
    dxl_engine_version v;
    if (dxl_engine_check(prog, &v, brief, brief_size, why, why_size) != 0) {
        dxl_log("engine refused: %s", why);
        free(prog);
        return 0;
    }
    dxl_session *ss = &s->run->session;
    dxl_session_set_engine(ss, prog);
    dxl_err e;
    if (dxl_session_save(ss, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    s->engine_asked = 0;
    dxl_shell_ask_engine(s);
    if (ss->engine_by_env)
        dxl_shell_toast(s, "Kept, but DXL_ENGINE names the engine played with: unset it to play with %s.", v.version);
    else if (!dxl_engine_pk3_found(prog))
        dxl_shell_toast(s, "The engine is %s, but it finds no SurrealEngine.pk3: put one beside it.", v.version);
    else
        dxl_shell_toast(s, "The engine is %s.", v.version);
    free(prog);
    return 1;
}

/* The pick's choices, in its order. */
enum { ENGINE_OWN, ENGINE_BROWSE, ENGINE_DIALOG };

void dxl_shell_engine_pick(dxl_shell *s, uint32_t id) {
    dxl_session *ss = &s->run->session;
    char *own = dxl_session_own_engine(ss);
    dxl_engine_version v;
    char brief[160], why[700], detail[200];
    if (dxl_engine_check(own, &v, brief, sizeof brief, why, sizeof why) == 0) snprintf(detail, sizeof detail, "%s", v.version);
    else snprintf(detail, sizeof detail, "%s", brief);
    free(own);
    const char *labels[3] = { "The launcher's own", "Another, in a folder...", "Another, by the system's dialog..." };
    const char *details[3] = { detail, "VibeEngine or SurrealEngine in it, a build of your own", "" };
    dxl_shell_pick_with(s, id, "The engine", labels, details, s->pick_folder ? 3 : 2,
                        ss->engine_by_env || !dxl_ini_get(ss->ini, "Launcher", "Engine") ? ENGINE_OWN : ENGINE_BROWSE);
}

void dxl_shell_engine_answer(dxl_shell *s, uint32_t id) {
    int got = dxl_shell_answer(s, id);
    dxl_session *ss = &s->run->session;
    if (got == ENGINE_OWN) {
        dxl_session_set_engine(ss, NULL);
        dxl_err e;
        if (dxl_session_save(ss, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
        s->engine_asked = 0;
        dxl_shell_ask_engine(s);
        if (s->engine_ok) dxl_shell_toast(s, "The engine is the launcher's own, %s.", s->engine_version);
        else dxl_shell_message(s, "The launcher's own engine", "%s", s->engine_problem);
    } else if (got == ENGINE_BROWSE) {
        char *dir = dxl_path_dirname(ss->engine);
        dxl_browser_open(s, dxl_path_is_dir(dir) ? dir : getenv("HOME"), DXL_BROWSE_ENGINE);
        free(dir);
    } else if (got == ENGINE_DIALOG && s->pick_folder) {
        s->pick_for = DXL_BROWSE_ENGINE;
        s->pick_folder(s);
    }
}

void dxl_shell_folder_picked(dxl_shell *s, const char *path) {
    char shown[1024];
    if (s->pick_for == DXL_BROWSE_ENGINE) {
        char brief[200], why[800];
        if (!dxl_shell_choose_engine(s, path, brief, sizeof brief, why, sizeof why))
            dxl_shell_message(s, "Not an engine to play with", "%s", why);
        return;
    }
    dxl_install in;
    dxl_install_check(path, &in);
    if (in.ok) dxl_shell_choose(s, &in);
    else dxl_shell_message(s, "Not one to play", "%s: %s.", dxl_shell_tilde(in.root, shown, sizeof shown), in.problem);
    dxl_install_free(&in);
}

void dxl_shell_repoint(dxl_shell *s, dxl_page back_to) {
    s->repoint = s->repoint_back = 1;
    s->repoint_to = back_to;
    s->looked = 0;
    dxl_shell_go(s, DXL_PAGE_FIND);
}

void dxl_shell_choose(dxl_shell *s, const dxl_install *in) {
    dxl_session *ss = &s->run->session;
    if (s->repoint) {
        char id[96];
        dxl_err e;
        dxl_session_remember_install(ss, in, id, sizeof id);
        dxl_profile_set(&ss->profile, "Install", id);
        if (dxl_session_save(ss, &e) != 0) {
            dxl_shell_message(s, "The profile was not changed", "%s", dxl_err_msg(&e));
            return;
        }
        dxl_log("profile %s: install %s (%s)", ss->profile.id, id, in->root);
        dxl_session_check_install(ss);
        /* Asked for from a page, that page again; from Recovery, as from a
         * profile whose install had gone, on to the game. */
        if (s->repoint_back && s->repoint_to != DXL_PAGE_RECOVERY) {
            char shown[1024];
            s->repoint = s->repoint_back = 0;
            dxl_shell_toast(s, "The profile plays the game in %s now.", dxl_shell_tilde(in->root, shown, sizeof shown));
            dxl_shell_go(s, s->repoint_to);
            return;
        }
        s->result = 1;
        return;
    }
    if (s->has_chosen) dxl_install_free(&s->chosen);
    s->chosen = *in;
    s->chosen.root = dxl_xstrdup(in->root);
    s->chosen.system_dir = dxl_xstrdup(in->system_dir);
    s->has_chosen = 1;
    memset(&s->plan, 0, sizeof s->plan);
    s->plan_error[0] = '\0';
    dxl_err e;
    if (dxl_profile_import(&ss->profile, s->chosen.system_dir, 1, &s->plan, &e) != 0)
        snprintf(s->plan_error, sizeof s->plan_error, "%s", dxl_err_msg(&e));
    /* The engine's own drawing; the Modern layout over the game's own three
     * buttons, or none, else the controls as they are: a layout of the
     * player's, or one of ours already. */
    s->begin_graphics = 0;
    s->begin_layout = 1;
    dxl_ini *usr = s->plan_error[0] ? NULL : dxl_profile_user_preview(s->plan.user_from);
    if (usr) {
        const dxl_pad_preset *now = dxl_bindings_detect(usr);
        int bound = 0;
        for (size_t k = 0; k < DXL_JOY_KEY_COUNT; k++) {
            const char *b = dxl_ini_get(usr, DXL_INPUT_SECTION, dxl_joy_keys[k]);
            bound += b && *b;
        }
        if (bound && (!now || strcmp(now->id, "classic") != 0)) s->begin_layout = 0;
        dxl_ini_free(usr);
    }
    s->focus[DXL_PAGE_IMPORT] = 0;
    dxl_shell_go(s, DXL_PAGE_IMPORT);
}

void dxl_shell_open(dxl_shell *s, const char *path) {
    if (!s->window || !path) return;
    /* A file URL, its bytes outside the unreserved ones escaped. */
    char url[4096];
    size_t n = snprintf(url, sizeof url, "file://");
    for (const unsigned char *p = (const unsigned char *)path; *p && n + 4 < sizeof url; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
            strchr("/-._~", *p))
            url[n++] = (char)*p;
        else
            n += snprintf(url + n, sizeof url - n, "%%%02X", *p);
    }
    url[n] = '\0';
    if (dxl_open_url(url) != 0) dxl_shell_toast(s, "Nothing opened %s", path);
}

void dxl_shell_probe(dxl_shell *s) {
    if (s->probe_state || !s->run) return;
    char *path = dxl_path_join(s->run->session.data_dir, "renderers.ini");
    dxl_renderers_free(&s->renderers);
    dxl_renderers_load(&s->renderers, path);
    free(path);
    /* Unresolved, every renderer the engine has stays offered. */
    dxl_gpu_probe none;
    memset(&none, 0, sizeof none);
    dxl_renderers_resolve(&s->renderers, &none);
    s->probe_state = 1;
    if (s->start_probe) s->start_probe(s);
}

void dxl_shell_probed(dxl_shell *s, const dxl_gpu_probe *p) {
    s->probe = *p;
    s->probe_state = 2;
    dxl_renderers_resolve(&s->renderers, &s->probe);
    dxl_log("probe: Vulkan %d (%s), OpenGL ES %d (%s), OpenGL %d%s%s", p->vulkan, p->vulkan_device, p->gles,
            p->gles_version, p->gl, p->note[0] ? "; " : "", p->note);
}

/* ---- the frame ------------------------------------------------------ */

static float margin(const dxl_ui *ui) {
    float m = ui->w * 0.05f;
    if (m < 24) m = 24;
    if (m > 72) m = 72;
    if (ui->w - 2 * m > WIDEST) m = (ui->w - WIDEST) / 2;
    return m;
}

static const char *page_title(dxl_page p) {
    switch (p) {
    case DXL_PAGE_WELCOME: return "Welcome";
    case DXL_PAGE_FIND: return "Where is the game?";
    case DXL_PAGE_IMPORT: return "Your profile";
    case DXL_PAGE_RECOVERY: return "Recovery";
    case DXL_PAGE_ABOUT: return "About";
    case DXL_PAGE_SETTINGS: return "Settings";
    case DXL_PAGE_LOG: return "The log";
    default: return "";
    }
}

/* The first run's three pages, as steps along the head. */
static void steps(dxl_shell *s, float right, float y) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    static const char *names[3] = { "1  Welcome", "2  The game", "3  Your profile" };
    int at = s->page == DXL_PAGE_WELCOME ? 0 : s->page == DXL_PAGE_FIND ? 1 : 2;
    float x = right;
    for (int i = 2; i >= 0; i--) {
        float w = dxl_text_size(ui, DXL_FONT_LABEL, names[i], 0).x;
        x -= w;
        dxl_rect r = { x, y, w + 1, 24 };
        dxl_label(ui, r, DXL_FONT_LABEL, names[i], i == at ? t->text_focus : t->help, DXL_W_LEFT);
        if (i == at) dxl_fill(ui, (dxl_rect){ x, y + 26, w, 2 }, t->accent);
        x -= 28;
    }
}

static float head(dxl_shell *s, float m, float shift) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float x = m + shift, w = ui->w - 2 * m;
    dxl_label(ui, (dxl_rect){ x, 24, w, 22 }, DXL_FONT_LABEL, "DXLAUNCHER", t->accent, DXL_W_LEFT);
    if (s->page == DXL_PAGE_HOME) {
        dxl_label(ui, (dxl_rect){ x, 44, w, 58 }, DXL_FONT_TITLE, "DEUS EX", t->title_text, DXL_W_LEFT);
        dxl_fill(ui, (dxl_rect){ x, 106, 64, 2 }, t->accent);
        return 132;
    }
    dxl_label(ui, (dxl_rect){ x, 46, w * 0.6f, 36 }, DXL_FONT_HEADING, page_title(s->page), t->title_text, DXL_W_LEFT);
    dxl_fill(ui, (dxl_rect){ x, 86, 56, 2 }, t->accent);
    if (!s->repoint && (s->page == DXL_PAGE_WELCOME || s->page == DXL_PAGE_FIND || s->page == DXL_PAGE_IMPORT))
        steps(s, x + w, 52);
    return 108;
}

static int hints_of(const dxl_shell *s, dxl_hint_item *it) {
    int n = 0;
    it[n++] = (dxl_hint_item){ DXL_HINT_CONFIRM, "Select" };
    if (s->dialog == DXL_DIALOG_OSK) {
        it[n++] = (dxl_hint_item){ DXL_HINT_CLEAR, "Erase" };
        it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Done" };
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Cancel" };
        return n;
    }
    if (s->dialog == DXL_DIALOG_PADTEST) {
        /* The pads drive nothing but the model: the keys' Esc, or the
         * pad's back held. */
        n = 0;
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, s->ui.mode == DXL_INPUT_PAD ? "Hold to stop" : "Stop testing" };
        return n;
    }
    if (s->dialog != DXL_DIALOG_NONE) {
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, s->dialog == DXL_DIALOG_MESSAGE ? "Close" : "Cancel" };
        return n;
    }
    switch (s->page) {
    case DXL_PAGE_HOME:
        if (s->run && s->run->session.install.ok && s->engine_ok) it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Play" };
        break;
    case DXL_PAGE_RECOVERY:
        it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Play" };
        break;
    case DXL_PAGE_FIND:
        if (!s->repoint || s->repoint_back) it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Back" };
        break;
    case DXL_PAGE_IMPORT:
    case DXL_PAGE_ABOUT:
    case DXL_PAGE_LOG:
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Back" };
        break;
    case DXL_PAGE_SETTINGS:
        if (s->ui.capture_id) {
            n = 0;
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Cancel" };
        } else if (s->ui.text_id) {
            n = 0;
            it[n++] = (dxl_hint_item){ DXL_HINT_CONFIRM, "Keep" };
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Cancel" };
        } else {
            it[n++] = (dxl_hint_item){ DXL_HINT_TAB_NEXT, "Next page" };
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Done" };
        }
        break;
    default:
        break;
    }
    return n;
}

/* The foot: what the launcher is on, and what to press. A hint clicked is
 * the frame's input, as its key would be. */
static void foot(dxl_shell *s, dxl_rect r, float m) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_fill(ui, r, dxl_rgba_fade(t->backdrop_bottom, 0.85f));
    dxl_fill(ui, (dxl_rect){ r.x, r.y, r.w, 1 }, dxl_rgba_fade(t->panel_light, 0.25f));

    dxl_hint_item items[4];
    int n = hints_of(s, items);
    dxl_rect hints = { r.x + m, r.y, r.w - 2 * m, r.h };
    int hit = dxl_hints(ui, hints, items, n);
    if (hit == DXL_HINT_CONFIRM) ui->in.activate++;
    else if (hit == DXL_HINT_BACK) ui->in.back++;
    else if (hit == DXL_HINT_PLAY) ui->in.start++;
    else if (hit == DXL_HINT_CLEAR) ui->in.clear++;

    char status[400];
    const dxl_run *run = s->run;
    if (run && dxl_profile_exists(&run->session.profile)) {
        const char *name = dxl_profile_get(&run->session.profile, "Name");
        snprintf(status, sizeof status, "%s%s%s", name ? name : run->session.profile.id,
                 s->engine_ok ? "  \xc2\xb7  " : "", s->engine_ok ? s->engine_version : "");
    } else {
        snprintf(status, sizeof status, "DXLauncher %s", s->version ? s->version : "");
    }
    dxl_rect left = { r.x + m, r.y, r.w * 0.45f, r.h };
    dxl_label(ui, left, DXL_FONT_SMALL, status, t->help, DXL_W_LEFT | DXL_W_MAY_CUT);
}

static void toasts(dxl_shell *s, float m) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float w = 360, y = 18;
    if (w > ui->w - 2 * m) w = ui->w - 2 * m;
    for (int i = 0; i < s->toast_count; i++) {
        double age = ui->now - s->toasts[i].at;
        float a = 1;
        if (age > TOAST_S - TOAST_FADE) a = (float)((TOAST_S - age) / TOAST_FADE);
        if (age < 0.15) a = (float)(age / 0.15);
        if (a < 1) dxl_ui_animate(ui);
        if (a <= 0) continue;
        float th = dxl_text_size(ui, DXL_FONT_SMALL, s->toasts[i].text, w - 28).y;
        dxl_rect r = { ui->w - m - w, y, w, th + 22 };
        dxl_bevel_style st = { dxl_rgba_fade(t->panel_top, a), dxl_rgba_fade(t->panel_bottom, a),
                               dxl_rgba_fade(t->panel_light, a), dxl_rgba_fade(t->panel_shadow, a), 6 };
        dxl_bevel(ui, r, &st);
        dxl_fill(ui, (dxl_rect){ r.x + 6, r.y + 8, 3, r.h - 16 }, dxl_rgba_fade(t->accent, a));
        dxl_text_draw(ui, DXL_FONT_SMALL, s->toasts[i].text, r.x + 18, r.y + 11, w - 28, dxl_rgba_fade(t->text_focus, a), 1);
        y += r.h + 8;
    }
}

static void expire_toasts(dxl_shell *s) {
    int k = 0;
    for (int i = 0; i < s->toast_count; i++) {
        if (s->toasts[i].at < 0) s->toasts[i].at = s->ui.now;
        if (s->ui.now - s->toasts[i].at < TOAST_S) s->toasts[k++] = s->toasts[i];
    }
    s->toast_count = k;
}

double dxl_shell_idle_for(const dxl_shell *s) {
    /* A frame the kit asked for (a caret's blink), and each toast's fade. */
    double soonest = -1;
    if (s->ui.wake > 0) soonest = s->ui.wake > s->ui.now ? s->ui.wake - s->ui.now : 0;
    for (int i = 0; i < s->toast_count; i++) {
        if (s->toasts[i].at < 0) return 0;
        double left = (s->toasts[i].at + TOAST_S - TOAST_FADE) - s->ui.now;
        if (left < 0) left = 0;
        if (soonest < 0 || left < soonest) soonest = left;
    }
    return soonest;
}

void dxl_shell_frame(dxl_shell *s, double now) {
    dxl_ui *ui = &s->ui;
    dxl_ui_begin(ui, now);
    if (s->page_since < 0) s->page_since = now;
    expire_toasts(s);
    if (ui->in.quit) s->result = 0;

    float m = margin(ui);
    dxl_rect above = { 0, 0, ui->w, ui->h - FOOT_H };
    dxl_rect bottom = { 0, ui->h - FOOT_H, ui->w, FOOT_H };
    dxl_backdrop(ui);

    /* The foot first: a hint clicked is input for the page in this frame. */
    int dialog = s->dialog != DXL_DIALOG_NONE;
    dxl_ui_scope(ui, dialog ? 1 : 0);
    foot(s, bottom, m);
    dxl_ui_scope(ui, 0);
    int back = ui->in.back;
    int start = ui->in.start;
    if (dialog) {
        /* The dialog's; the page's own are its widgets', under it. */
        ui->in.back = 0;
        ui->in.start = 0;
    }

    float t = dxl_ui_tween(ui, s->page_since, PAGE_S);
    float shift = (1 - t) * SLIDE_DP;
    float head_h = head(s, m, shift);
    dxl_rect r = { m + shift, head_h, ui->w - 2 * m, ui->h - head_h - FOOT_H - 18 };
    switch (s->page) {
    case DXL_PAGE_HOME: dxl_page_home(s, r); break;
    case DXL_PAGE_WELCOME: dxl_page_welcome(s, r); break;
    case DXL_PAGE_FIND: dxl_page_find(s, r); break;
    case DXL_PAGE_IMPORT: dxl_page_import(s, r); break;
    case DXL_PAGE_RECOVERY: dxl_page_recovery(s, r); break;
    case DXL_PAGE_ABOUT: dxl_page_about(s, r); break;
    case DXL_PAGE_SETTINGS: dxl_page_settings(s, r); break;
    case DXL_PAGE_LOG: dxl_page_log(s, r); break;
    default: break;
    }
    if (t < 1) {
        const dxl_theme *th = &ui->theme;
        dxl_gradient(ui, above, dxl_rgba_fade(th->backdrop_top, 1 - t), dxl_rgba_fade(th->backdrop_bottom, 1 - t));
    }
    if (dialog && s->dialog != DXL_DIALOG_NONE) {
        ui->in.start = start;
        dxl_dialogs(s, above, back);
    } else if (s->dialog != DXL_DIALOG_NONE) {
        dxl_dialogs(s, above, 0);
    }
    toasts(s, m);
    dxl_ui_end(ui);
}
