#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/bigscreen.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/upkg_assets.h"
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
    s->save_snap_slot = DXL_SNAP_NONE;
}

void dxl_shell_release(dxl_shell *s) {
    dxl_art_free(&s->art);
    if (s->save_snap) SDL_DestroyTexture(s->save_snap);
    s->save_snap = NULL;
    s->save_snap_slot = DXL_SNAP_NONE;
    s->save_snap_read = 0;
    dxl_thumbs_clear(s->thumbs);
    dxl_thumbs_clear(s->shot_whole);
    s->thumbs_pending = 0;
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
    free(s->themes);
    s->themes = NULL;
    s->theme_count = 0;
    s->themes_from[0] = '\0';
    dxl_shell_release(s);
    dxl_thumbs_free(s->thumbs);
    dxl_thumbs_free(s->shot_whole);
    s->thumbs = s->shot_whole = NULL;
    dxl_saves_free(&s->save_list);
    dxl_shots_free(&s->shot_list);
    for (int i = 0; s->servers && i < s->server_count; i++) dxl_server_free(&s->servers[i]);
    free(s->servers);
    s->servers = NULL;
    s->server_count = 0;
    free(s->music);
    s->music = NULL;
    for (int i = 0; i < DXL_SOUND_COUNT; i++) {
        free(s->sounds[i]);
        s->sounds[i] = NULL;
    }
    s->sounds_from[0] = '\0';
}

/* A file of the install by its folder's and its own names, any case. */
static char *install_file(const char *root, const char *dir, const char *name) {
    char *d = dxl_path_resolve_ci(root, dir);
    char *f = d ? dxl_path_resolve_ci(d, name) : NULL;
    free(d);
    return f;
}

/* The install's title music and menu sounds, those of the one before
 * gone; none for no install. */
static void read_sounds(dxl_shell *s, const char *root) {
    free(s->music);
    s->music = NULL;
    s->music_len = 0;
    for (int i = 0; i < DXL_SOUND_COUNT; i++) {
        free(s->sounds[i]);
        s->sounds[i] = NULL;
        s->sound_len[i] = 0;
    }
    if (!*root) return;
    dxl_upkg p;
    dxl_err e;
    char type[16];
    char *umx = install_file(root, "Music", "Title_Music.umx");
    if (umx && dxl_upkg_load(&p, umx, &e) == 0) {
        s->music = dxl_upkg_file(&p, dxl_upkg_find(&p, "Music", "Title_Music"), &s->music_len, type, sizeof type, &e);
        dxl_upkg_free(&p);
    }
    if (!s->music) dxl_log("sound: no title music (%s)", umx ? dxl_err_msg(&e) : "no Music/Title_Music.umx");
    free(umx);
    char *u = install_file(root, "System", "DeusExSounds.u");
    if (u && dxl_upkg_load(&p, u, &e) == 0) {
        for (int i = 0; i < DXL_SOUND_COUNT; i++)
            s->sounds[i] = dxl_upkg_file(&p, dxl_upkg_find(&p, "Sound", dxl_sound_names[i]), &s->sound_len[i], type,
                                         sizeof type, &e);
        dxl_upkg_free(&p);
    } else {
        dxl_log("sound: no menu sounds (%s)", u ? dxl_err_msg(&e) : "no System/DeusExSounds.u");
    }
    free(u);
}

void dxl_shell_volumes(dxl_shell *s) {
    if (!s->audio || !s->run) return;
    const dxl_ini *ini = s->run->session.ini;
    const char *m = dxl_ini_get(ini, "Launcher", "MusicVolume"), *v = dxl_ini_get(ini, "Launcher", "SoundVolume");
    dxl_audio_volumes(s->audio, m ? (float)atof(m) : 0.5f, v ? (float)atof(v) : 0.7f);
}

void dxl_shell_sound(dxl_shell *s) {
    const dxl_install *in = s->run ? &s->run->session.install : NULL;
    const char *root = in && in->ok ? in->root : "";
    if (strcmp(s->sounds_from, root) != 0) {
        snprintf(s->sounds_from, sizeof s->sounds_from, "%s", root);
        read_sounds(s, root);
        s->audio_given = 0;
    }
    if (!s->audio || s->audio_given) return;
    s->audio_given = 1;
    /* Each sound the install has, and none of another's for those it
     * lacks. */
    int given = 0;
    for (int i = 0; i < DXL_SOUND_COUNT; i++) {
        if (!s->sounds[i]) {
            dxl_audio_sound(s->audio, (dxl_sound)i, NULL, 0);
            continue;
        }
        if (dxl_audio_sound(s->audio, (dxl_sound)i, s->sounds[i], s->sound_len[i]) == 0) {
            given++;
        } else {
            dxl_log("sound: %s is no WAV SDL reads", dxl_sound_names[i]);
            dxl_audio_sound(s->audio, (dxl_sound)i, NULL, 0);
        }
    }
    int music = s->music && dxl_audio_music(s->audio, s->music, s->music_len) == 0;
    if (!s->music) dxl_audio_music(s->audio, NULL, 0);
    if (s->music && !music) dxl_log("sound: the title music does not play");
    dxl_shell_volumes(s);
    if (*root) dxl_log("sound: %s and %d menu sounds", music ? "the title music" : "no music", given);
}

void dxl_shell_cue(dxl_shell *s, int cues) {
    if (!s->audio || !cues) return;
    static const struct { int cue; dxl_sound sound; } order[] = {
        { DXL_CUE_PLAY, DXL_SOUND_OK }, { DXL_CUE_BACK, DXL_SOUND_CANCEL }, { DXL_CUE_OPEN, DXL_SOUND_ACTIVATE },
        { DXL_CUE_PRESS, DXL_SOUND_PRESS }, { DXL_CUE_SLIDE, DXL_SOUND_SLIDER }, { DXL_CUE_FOCUS, DXL_SOUND_FOCUS },
    };
    for (size_t i = 0; i < sizeof order / sizeof *order; i++)
        if (cues & order[i].cue) {
            dxl_audio_play(s->audio, order[i].sound);
            return;
        }
}

void dxl_shell_big(dxl_shell *s, int on) {
    on = on != 0;
    if (on == s->big) return;
    if (s->set_big) {
        s->set_big(s, on);
    } else {
        s->big = on;
        dxl_ui_set_big(&s->ui, on);
    }
    if (s->big == on) dxl_log("the big screen: %s", on ? "on" : "off");
    else dxl_log("the big screen: still %s, the desktop did not switch it", s->big ? "on" : "off");
}

/* A player who knows of the big screen, by F11 or the Settings row, is not
 * offered it, now or later. */
static void big_known(dxl_shell *s) {
    s->big_offered = 1;
    if (s->run) dxl_ini_set_bool(s->run->session.ini, "Launcher", "BigScreenOffered", 1);
}

void dxl_shell_big_setting(dxl_shell *s, const char *value) {
    big_known(s);
    dxl_big_setting st = dxl_big_setting_of(value);
    int on = st == DXL_BIG_ON;
    if (st == DXL_BIG_AUTO) on = dxl_big_screen_wanted(st, s->run && s->run->opt.big_screen, NULL);
    dxl_shell_big(s, on);
}

/* The offer's question, and its answer read. */
#define BIG_OFFER "big screen offer"

void dxl_shell_offer_big(dxl_shell *s) {
    dxl_shell_confirm(s, dxl_ui_id(&s->ui, BIG_OFFER), "The big screen",
                      "A controller is in use. On the big screen the launcher fills the screen, its type larger, for "
                      "a television or a handheld. F11 switches between the two; Settings' Launcher page keeps the "
                      "choice.",
                      "Switch to it", "Stay as it is");
}

/* Offered once, on Home or Welcome, as a pad is first used there in a
 * window that is not the big screen and may be -- launcher.ini's
 * BigScreen automatic, BigScreenOffered not yet -- with no other dialog's
 * answer waiting to be read: the press that brought it the question's,
 * not the page's, its move undone and its sound not played. Switched to,
 * the big screen always. */
static void offer_big(dxl_shell *s) {
    dxl_ui *ui = &s->ui;
    if (!s->run) return;
    dxl_ini *ini = s->run->session.ini;
    int a = dxl_shell_answer(s, dxl_ui_id(ui, BIG_OFFER));
    if (a == 1) {
        dxl_ini_set(ini, "Launcher", "BigScreen", "on");
        dxl_shell_big(s, 1);
    }
    if (a >= 0) {
        dxl_err e;
        if (dxl_profile_exists(&s->run->session.profile) && dxl_session_save(&s->run->session, &e) != 0)
            dxl_log("warning: %s", dxl_err_msg(&e));
        return;
    }
    if (s->big || !s->set_big || s->big_offered || ui->mode != DXL_INPUT_PAD || s->dialog != DXL_DIALOG_NONE ||
        s->answered || (s->page != DXL_PAGE_HOME && s->page != DXL_PAGE_WELCOME))
        return;
    if (dxl_big_setting_of(dxl_ini_get(ini, "Launcher", "BigScreen")) != DXL_BIG_AUTO ||
        dxl_ini_get_bool(ini, "Launcher", "BigScreenOffered", 0))
        return;
    big_known(s);
    ui->focus = ui->focus_was;
    ui->focus_moved = 0;
    ui->cues = 0;
    memset(ui->in.nav, 0, sizeof ui->in.nav);
    ui->in.activate = ui->in.back = ui->in.start = ui->in.clear = ui->in.tab = ui->in.untab = 0;
    ui->in.shoulder_l = ui->in.shoulder_r = ui->in.page_up = ui->in.page_down = 0;
    dxl_shell_offer_big(s);
}

void dxl_shell_theme(dxl_shell *s) {
    const char *sys = s->run && s->run->session.install.ok ? s->run->session.install.system_dir : NULL;
    if (sys && strcmp(s->themes_from, sys) != 0) {
        snprintf(s->themes_from, sizeof s->themes_from, "%s", sys);
        if (!s->themes) s->themes = dxl_xmalloc(sizeof *s->themes * DXL_MENU_THEMES_MAX);
        char *u = dxl_path_resolve_ci(sys, "DeusEx.u");
        dxl_err e;
        s->theme_count = u ? dxl_menu_themes_read(u, s->themes, DXL_MENU_THEMES_MAX, &e) : -1;
        if (s->theme_count < 0) {
            dxl_log("menu themes: %s", u ? dxl_err_msg(&e) : "no DeusEx.u");
            s->theme_count = 0;
        }
        free(u);
    }
    /* The player's, as the game takes it: User.ini's, else the class's. */
    const char *want = NULL;
    dxl_ini *usr = s->run && s->run->config ? dxl_config_user_ini(s->run->config) : NULL;
    if (usr) want = dxl_ini_get(usr, "DeusEx.DeusExPlayer", "MenuThemeName");
    if (!want || !*want) want = "Grey";
    const dxl_menu_theme *mt = sys ? dxl_menu_theme_find(s->themes, s->theme_count, want) : NULL;
    dxl_theme t;
    if (mt) {
        dxl_rgba menu[DXL_MENU_COLORS];
        for (int i = 0; i < DXL_MENU_COLORS; i++)
            menu[i] = (dxl_rgba){ mt->colors[i][0], mt->colors[i][1], mt->colors[i][2], 255 };
        dxl_theme_from_menu(&t, mt->name, menu);
    } else {
        dxl_theme_default(&t);
    }
    if (strcmp(t.name, s->ui.theme.name) != 0 || memcmp(t.menu, s->ui.theme.menu, sizeof t.menu) != 0) {
        dxl_ui_set_theme(&s->ui, &t);
        dxl_log("theme: %s", t.name);
    }
}

void dxl_shell_enter(dxl_shell *s, dxl_run *r, dxl_view v) {
    s->run = r;
    s->view = v;
    free(s->pad_db);
    s->pad_db = NULL;
    s->result = -1;
    s->dialog = DXL_DIALOG_NONE;
    s->ui.pads_off = 0;
    s->repoint = v == DXL_VIEW_INSTALL;
    s->repoint_back = 0;
    s->making = 0;
    if (v == DXL_VIEW_FIRST_RUN) snprintf(s->begin_name, sizeof s->begin_name, "Deus Ex");
    dxl_shell_theme(s);
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
    /* A link's server, the start's: Home to Multiplayer, asking; after
     * another screen's Play, joined (app/run.c). */
    char link[256];
    if (v == DXL_VIEW_HOME && r->opt.join && dxl_server_link(r->opt.join, link, sizeof link)) {
        dxl_shell_join_link(s, link);
        r->opt.join = NULL;
    }
}

void dxl_shell_go(dxl_shell *s, dxl_page p) {
    /* Asked for as a page draws: once it is drawn. */
    if (s->in_page) {
        s->go_pending = 1;
        s->go_to = p;
        return;
    }
    /* The system's folder dialog's answer is the page's it was shown from. */
    s->pick_open = 0;
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
    case DXL_PAGE_PROFILES: dxl_page_profiles_enter(s); break;
    case DXL_PAGE_SAVES: dxl_page_saves_enter(s); break;
    case DXL_PAGE_SHOTS: dxl_page_shots_enter(s); break;
    case DXL_PAGE_SERVERS: dxl_page_servers_enter(s); break;
    default: break;
    }
}

int dxl_shell_engine_has(const dxl_shell *s, const char *feature) {
    if (!s->engine_ok) return 0;
    dxl_engine_version v;
    memset(&v, 0, sizeof v);
    snprintf(v.features, sizeof v.features, "%s", s->engine_features);
    return dxl_engine_has(&v, feature);
}

const dxl_save *dxl_shell_continue(const dxl_shell *s) {
    if (!s->run || !s->run->session.install.ok || !dxl_shell_engine_has(s, "loadgame-url")) return NULL;
    return dxl_saves_latest(&s->save_list);
}

void dxl_shell_play_save(dxl_shell *s, int slot) {
    if (!s->run) return;
    dxl_save_url(slot, s->run->url, sizeof s->run->url);
    dxl_log("play: the save in slot %d (%s)", slot, s->run->url);
    s->result = 1;
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
    s->dialog_since = s->ui.now;
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
        if (ss->engine_by_env) dxl_shell_toast(s, "Kept, but DXL_ENGINE names the engine played with: unset it to "
                                                  "play with the launcher's own.");
        else if (s->engine_ok) dxl_shell_toast(s, "The engine is the launcher's own, %s.", s->engine_version);
        else dxl_shell_message(s, "The launcher's own engine", "%s", s->engine_problem);
    } else if (got == ENGINE_BROWSE) {
        char *dir = dxl_path_dirname(ss->engine);
        dxl_browser_open(s, dxl_path_is_dir(dir) ? dir : getenv("HOME"), DXL_BROWSE_ENGINE);
        free(dir);
    } else if (got == ENGINE_DIALOG) {
        dxl_shell_pick_folder(s, DXL_BROWSE_ENGINE);
    }
}

void dxl_shell_pick_folder(dxl_shell *s, int what) {
    if (!s->pick_folder) return;
    s->pick_for = what;
    s->pick_open = 1;
    s->pick_page = s->page;
    s->pick_repoint = s->repoint;
    s->pick_making = s->making;
    s->pick_folder(s);
}

void dxl_shell_folder_picked(dxl_shell *s, const char *path) {
    char shown[1024];
    if (!s->pick_open || s->page != s->pick_page || s->repoint != s->pick_repoint || s->making != s->pick_making ||
        s->dialog != DXL_DIALOG_NONE) {
        dxl_log("the folder dialog's answer, %s, is for a page gone: dropped", path);
        s->pick_open = 0;
        return;
    }
    s->pick_open = 0;
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
    /* An install for nothing to make: the profile in use is made, and this
     * is no repoint (a folder dialog's answer late, say). */
    if (!s->repoint && !s->making && dxl_profile_exists(&ss->profile)) {
        dxl_log("an install chosen with no profile to make: %s left as it is", ss->profile.id);
        return;
    }
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
        dxl_shell_theme(s);
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
    case DXL_PAGE_PROFILES: return "Profiles";
    case DXL_PAGE_SAVES: return "Saves";
    case DXL_PAGE_SHOTS: return "Screenshots";
    case DXL_PAGE_SERVERS: return "Multiplayer";
    default: return "";
    }
}

/* The first run's three pages, as steps along the head; a new profile's
 * two. */
static void steps(dxl_shell *s, float right, float y) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    static const char *first_run[3] = { "1  Welcome", "2  The game", "3  Your profile" };
    static const char *making[3] = { "", "1  The game", "2  Your profile" };
    const char **names = s->making ? making : first_run;
    int at = s->page == DXL_PAGE_WELCOME ? 0 : s->page == DXL_PAGE_FIND ? 1 : 2;
    float x = right;
    for (int i = 2; i >= (s->making ? 1 : 0); i--) {
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
        /* The game's own "Deus Ex", from its logo, where the install has
         * it; else ours. */
        const dxl_install *in = s->run ? &s->run->session.install : NULL;
        dxl_art_load(&s->art, ui, in && in->ok ? in->root : NULL);
        if (!dxl_art_wordmark(ui, &s->art, (dxl_rect){ x - 4, 46, w, 56 }))
            dxl_label(ui, (dxl_rect){ x, 44, w, 58 }, DXL_FONT_TITLE, "DEUS EX", t->title_text, DXL_W_LEFT);
        dxl_fill(ui, (dxl_rect){ x, 106, 64, 2 }, t->accent);
        return 132;
    }
    dxl_label_typed(ui, (dxl_rect){ x, 46, w * 0.6f, 36 }, DXL_FONT_HEADING, page_title(s->page), t->title_text,
                    DXL_W_LEFT, s->page_since);
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
        if (dxl_shell_continue(s)) it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Continue" };
        else if (s->run && s->run->session.install.ok && s->engine_ok) it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Play" };
        break;
    case DXL_PAGE_SAVES:
        if (s->run && s->run->session.install.ok && dxl_shell_engine_has(s, "loadgame-url") &&
            s->save_sel < s->save_list.count && s->save_list.items[s->save_sel].readable)
            it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Load" };
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Back" };
        break;
    case DXL_PAGE_SHOTS:
        it[n++] = (dxl_hint_item){ DXL_HINT_BACK, s->shot_open ? "Close" : "Back" };
        break;
    case DXL_PAGE_SERVERS:
        if (s->ui.text_id) {
            n = 0;
            it[n++] = (dxl_hint_item){ DXL_HINT_CONFIRM, "Keep" };
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Cancel" };
        } else {
            if (s->run && s->run->session.install.ok && s->engine_ok && s->servers_shown)
                it[n++] = (dxl_hint_item){ DXL_HINT_PLAY, "Join" };
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Back" };
        }
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
    case DXL_PAGE_PROFILES:
        if (s->ui.text_id) {
            n = 0;
            it[n++] = (dxl_hint_item){ DXL_HINT_CONFIRM, "Keep" };
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Cancel" };
        } else {
            it[n++] = (dxl_hint_item){ DXL_HINT_BACK, "Back" };
        }
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
    /* Under an edit a press here is its hints' -- Keep, Cancel -- not a
     * click away from the field, which would keep it before Cancel. */
    if (ui->text_id && ui->in.pressed && ui->in.px >= r.x && ui->in.px < r.x + r.w && ui->in.py >= r.y &&
        ui->in.py < r.y + r.h)
        ui->in.pressed = 0;
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
    /* A page come up and not drawn yet: its first frame now, whatever input
     * follows or does not. */
    if (s->page_since < 0) return 0;
    /* A frame the kit asked for (a caret's blink), and each toast's fade. */
    double soonest = -1;
    if (s->ui.wake > 0) soonest = s->ui.wake > s->ui.now ? s->ui.wake - s->ui.now : 0;
    /* Thumbnails on their way: looked for a few times a second. */
    if (s->thumbs_pending && (soonest < 0 || soonest > 0.05)) soonest = 0.05;
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
    dxl_dialog dialog_was = s->dialog;
    dxl_shell_sound(s);
    /* The pads drive the window but under the wizard and the test, which
     * take them -- by what is up, so no way a dialog goes leaves them off. */
    ui->pads_off = s->dialog == DXL_DIALOG_PADWIZARD || s->dialog == DXL_DIALOG_PADTEST;
    dxl_ui_begin(ui, now);
    if (s->page_since < 0) s->page_since = now;
    expire_toasts(s);
    if (ui->in.quit) s->result = 0;
    /* F11: the big screen on or off, for as long as the window is up; a
     * player who knows of it is not offered it. */
    if (ui->in.toggle_big) {
        dxl_shell_big(s, !s->big);
        big_known(s);
    }
    offer_big(s);
    /* A link's server, handed over: Multiplayer, once nothing is half done. */
    if (s->join_link[0] && s->page != DXL_PAGE_SERVERS && s->dialog == DXL_DIALOG_NONE && !s->answered &&
        !s->repoint && !s->making && s->page != DXL_PAGE_WELCOME && s->page != DXL_PAGE_FIND &&
        s->page != DXL_PAGE_IMPORT)
        dxl_shell_go(s, DXL_PAGE_SERVERS);

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
    s->in_page = 1;
    switch (s->page) {
    case DXL_PAGE_HOME: dxl_page_home(s, r); break;
    case DXL_PAGE_WELCOME: dxl_page_welcome(s, r); break;
    case DXL_PAGE_FIND: dxl_page_find(s, r); break;
    case DXL_PAGE_IMPORT: dxl_page_import(s, r); break;
    case DXL_PAGE_RECOVERY: dxl_page_recovery(s, r); break;
    case DXL_PAGE_ABOUT: dxl_page_about(s, r); break;
    case DXL_PAGE_SETTINGS: dxl_page_settings(s, r); break;
    case DXL_PAGE_LOG: dxl_page_log(s, r); break;
    case DXL_PAGE_PROFILES: dxl_page_profiles(s, r); break;
    case DXL_PAGE_SAVES: dxl_page_saves(s, r); break;
    case DXL_PAGE_SHOTS: dxl_page_shots(s, r); break;
    case DXL_PAGE_SERVERS: dxl_page_servers(s, r); break;
    default: break;
    }
    s->in_page = 0;
    /* Pictures the page asked for and not read yet: frames until they are,
     * to show them. Off the page, none waited for. */
    s->thumbs_pending = s->page == DXL_PAGE_SHOTS && (dxl_thumbs_busy(s->thumbs) || dxl_thumbs_busy(s->shot_whole));
    if (s->go_pending) {
        s->go_pending = 0;
        dxl_shell_go(s, s->go_to);
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
    /* A dialog come up, and play, for the menu's sounds. */
    if (dialog_was == DXL_DIALOG_NONE && s->dialog != DXL_DIALOG_NONE) ui->cues |= DXL_CUE_OPEN;
    if (s->result == 1) ui->cues |= DXL_CUE_PLAY;
}
