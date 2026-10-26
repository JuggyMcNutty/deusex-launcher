/* Settings: the registry's pages as tabs (core/settings.h), each row the
 * control its kind is drawn with, the focused row's help under them. A
 * change goes to the open stores at once; the files are written as the
 * page is left, Done or not. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/bindings.h"
#include "core/engine_sections.h"
#include "core/keys.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/profile.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_H 46
#define KEYS_PAGE "Keyboard & mouse"
#define PAD_PAGE "Controller"
#define SLOT_W 170

/* The page's rows, in the order shown: its own, then its advanced ones. */
int dxl_settings_rows(const char *page, const dxl_setting **out, int max) {
    int n = 0;
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < dxl_settings_count() && n < max; i++) {
            const dxl_setting *st = dxl_setting_at(i);
            if (strcmp(st->page, page) != 0 || st->kind == DXL_SET_HEADING) continue;
            if (((st->flags & DXL_SET_ADVANCED) != 0) == pass) out[n++] = st;
        }
    return n;
}

static char *settings_json(const dxl_shell *s) {
    return dxl_path_join(s->run->session.profile.engine_dir, "Settings.json");
}

void dxl_page_settings_enter(dxl_shell *s) {
    if (s->settings_open || !s->run || !s->run->config) return;
    dxl_session *ss = &s->run->session;
    memset(&s->stores, 0, sizeof s->stores);
    s->stores.system = dxl_config_ini(s->run->config);
    /* The generation the engine will read, whole (core/engine_sections.h). */
    dxl_engine_sections_seed(s->stores.system);
    s->stores.user = dxl_config_user_ini(s->run->config);
    s->stores.launcher = ss->ini;
    s->stores.profile = ss->profile.ini;
    char *path = settings_json(s), *def = dxl_path_join(ss->data_dir, "engine-settings.json.default");
    s->engine_settings = dxl_es_open(path, def);
    s->stores.engine = s->engine_settings;
    snprintf(s->stores.next_run, sizeof s->stores.next_run, "%s", s->next_run);
    free(def);
    free(path);
    /* The text rows' values, edited in place while the page is up. */
    s->text_count = 0;
    for (size_t i = 0; i < dxl_settings_count() && s->text_count < 8; i++) {
        const dxl_setting *st = dxl_setting_at(i);
        if (st->kind != DXL_SET_TEXT) continue;
        s->text_rows[s->text_count] = st;
        dxl_setting_get(&s->stores, st, s->texts[s->text_count], sizeof s->texts[0]);
        s->text_count++;
    }
    s->settings_open = 1;
}

void dxl_settings_keep(dxl_shell *s) {
    if (!s->settings_open) return;
    snprintf(s->next_run, sizeof s->next_run, "%s", s->stores.next_run);
    dxl_err e;
    if (s->run->config && dxl_config_save(s->run->config, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    dxl_engine_settings *es = s->engine_settings;
    if (es && dxl_es_dirty(es)) {
        if (dxl_es_save(es, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
        else if (dxl_es_kept(es))
            dxl_log("%s could not be read (%s): kept as %s, and written anew", dxl_es_path(es), dxl_es_problem(es),
                    dxl_es_kept(es));
    }
    if (dxl_session_save(&s->run->session, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    dxl_es_free(s->engine_settings);
    s->engine_settings = NULL;
    memset(&s->stores, 0, sizeof s->stores);
    s->settings_open = 0;
    dxl_log("settings kept");
}

static void live(dxl_shell *s, const dxl_setting *st);

void dxl_settings_defaults(dxl_shell *s, const char *page) {
    if (!s->settings_open) return;
    dxl_session *ss = &s->run->session;
    const char *sys = ss->install.system_dir;
    dxl_stores fresh;
    memset(&fresh, 0, sizeof fresh);
    /* A mod's profile: the mod's own, where it has them. */
    const char *mod_dir = dxl_profile_get(&ss->profile, "Mod");
    dxl_mod mod;
    int modded = mod_dir && *mod_dir && dxl_mod_check(mod_dir, &mod, NULL) == 0;
    if (sys) dxl_profile_defaults(sys, modded ? &mod : NULL, &fresh.system, &fresh.user);
    if (modded) dxl_mod_free(&mod);
    char *def = dxl_path_join(ss->data_dir, "engine-settings.json.default");
    fresh.engine = dxl_es_open("", def);
    /* The renderer a profile made here begins on (dxl_shell_no_gpu) */
    if (fresh.engine && dxl_shell_no_gpu(s)) dxl_es_set_choice(fresh.engine, DXL_ES_RENDER_TYPE, "Software");
    dxl_settings_reset(&s->stores, &fresh, page);
    /* The keys as DefUser.ini has them; the controller's, its Joy lines. */
    if ((!page || !strcmp(page, KEYS_PAGE)) && fresh.user && s->stores.user) dxl_keys_reset(s->stores.user, fresh.user);
    if ((!page || !strcmp(page, PAD_PAGE)) && fresh.user && s->stores.user)
        for (size_t k = 0; k < DXL_JOY_KEY_COUNT; k++) {
            const char *v = dxl_ini_get(fresh.user, DXL_INPUT_SECTION, dxl_joy_keys[k]);
            dxl_bindings_set(s->stores.user, dxl_joy_keys[k], v ? v : "");
        }
    for (int i = 0; i < s->text_count; i++)
        dxl_setting_get(&s->stores, s->text_rows[i], s->texts[i], sizeof s->texts[0]);
    /* What the rows reset change in the window, at once: those of the page
     * alone, the rest as they were. */
    for (size_t i = 0; i < dxl_settings_count(); i++)
        if (!page || !strcmp(dxl_setting_at(i)->page, page)) live(s, dxl_setting_at(i));
    dxl_es_free(fresh.engine);
    dxl_ini_free(fresh.system);
    dxl_ini_free(fresh.user);
    free(def);
}

static char *text_of(dxl_shell *s, const dxl_setting *st, size_t *size) {
    for (int i = 0; i < s->text_count; i++)
        if (s->text_rows[i] == st) {
            *size = sizeof s->texts[i];
            return s->texts[i];
        }
    return NULL;
}

/* A choice row's choices: its own; the renderers this machine runs; the
 * presets and Custom; a size's, the one the profile has first when it is
 * none of them. values[i] is what choose takes. */
static int choices_of(dxl_shell *s, const dxl_setting *st, const char **labels, char values[][64], char shown[][64],
                      int max) {
    int n = 0;
    if (st->flags & DXL_SET_RENDERER) {
        dxl_shell_probe(s);
        /* The profile's own, where the probe found it does not run here:
         * shown as it is and said so, not as the first that runs. */
        char now[64];
        dxl_setting_get(&s->stores, st, now, sizeof now);
        for (size_t i = 0; i < s->renderers.count && n < max; i++) {
            const dxl_renderer *rd = &s->renderers.items[i];
            snprintf(values[n], 64, "%s", rd->engine_type);
            if (rd->selectable) {
                labels[n++] = rd->label;
            } else if (now[0] && !dxl_stricmp(values[n], now)) {
                snprintf(shown[n], 64, "%s (not on this machine)", rd->label);
                labels[n] = shown[n];
                n++;
            }
        }
        return n;
    }
    if (st->at[0].codec == DXL_CODEC_WIDTH) {
        char now[64];
        dxl_setting_get(&s->stores, st, now, sizeof now);
        int known = 0;
        for (int i = 0; st->choices[i].value; i++) known |= !dxl_stricmp(st->choices[i].value, now);
        int w, h;
        if (!known && sscanf(now, "%dx%d", &w, &h) == 2) {
            snprintf(values[n], 64, "%s", now);
            snprintf(shown[n], 64, "%d x %d", w, h);
            labels[n] = shown[n];
            n++;
        }
    }
    for (int i = 0; st->choices && st->choices[i].value && n < max; i++) {
        snprintf(values[n], 64, "%s", st->choices[i].value);
        labels[n++] = st->choices[i].label;
    }
    return n;
}

static int current_of(dxl_shell *s, const dxl_setting *st, char values[][64], int count) {
    char now[128];
    if (st->flags & DXL_SET_PRESET) {
        const dxl_preset *p = dxl_preset_matching(&s->stores);
        snprintf(now, sizeof now, "%s", p ? p->id : "custom");
    } else {
        dxl_setting_get(&s->stores, st, now, sizeof now);
    }
    for (int i = 0; i < count; i++)
        if (!dxl_stricmp(values[i], now)) return i;
    return 0;
}

static void choose(dxl_shell *s, const dxl_setting *st, const char *value) {
    if (st->flags & DXL_SET_PRESET) {
        for (size_t i = 0; i < dxl_preset_count(); i++)
            if (!strcmp(dxl_preset_at(i)->id, value)) dxl_preset_apply(&s->stores, dxl_preset_at(i));
        return;
    }
    dxl_setting_set(&s->stores, st, value);
    live(s, st);
}

/* What the launcher's own settings change in its window, at once. */
static void live(dxl_shell *s, const dxl_setting *st) {
    if (!strcmp(st->id, "launcher.ui_scale")) s->ui.user_scale = (float)dxl_setting_number(&s->stores, st);
    else if (!strcmp(st->id, "launcher.reduced_motion")) s->ui.reduced_motion = dxl_setting_bool(&s->stores, st);
    else if (!strcmp(st->id, "launcher.big_screen")) {
        char v[16];
        dxl_setting_get(&s->stores, st, v, sizeof v);
        dxl_shell_big_setting(s, v);
    }
    else if (!strcmp(st->id, "hud.menu_theme")) dxl_shell_theme(s);
    else if (!strcmp(st->id, "launcher.music_volume") || !strcmp(st->id, "launcher.sound_volume")) dxl_shell_volumes(s);
}

/* The engine's row: its version, or what is wrong with it; its choices. */
static void engine_row(dxl_shell *s, const dxl_setting *st, uint32_t id, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    dxl_shell_ask_engine(s);
    dxl_shell_engine_answer(s, id);
    if (dxl_value_row(ui, id, r, st->label, s->engine_ok ? s->engine_version : s->engine_brief,
                      s->engine_ok ? ui->theme.text : ui->theme.bad, 0))
        dxl_shell_engine_pick(s, id);
}

/* The profile's game: its folder, or what is wrong with it; The game page
 * for another. */
static void game_row(dxl_shell *s, const dxl_setting *st, uint32_t id, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_install *in = &s->run->session.install;
    char shown[1024];
    const char *value = in->root ? dxl_shell_tilde(in->root, shown, sizeof shown) : "none";
    if (dxl_value_row(ui, id, r, st->label, in->ok ? value : (in->problem[0] ? in->problem : value),
                      in->ok ? ui->theme.text : ui->theme.bad, 0))
        dxl_shell_repoint(s, DXL_PAGE_SETTINGS);
}

/* The help line of the engine's and the game's rows: what they are now. */
static void row_why(dxl_shell *s, const dxl_setting *st, char *out, size_t size) {
    char shown[1024];
    const dxl_session *ss = &s->run->session;
    if (!strcmp(st->id, "launcher.engine")) {
        if (!s->engine_ok) snprintf(out, size, "%s", s->engine_problem);
        else snprintf(out, size, "%s: %s, with %s.%s", dxl_shell_tilde(ss->engine, shown, sizeof shown),
                      s->engine_version, s->engine_features,
                      ss->engine_by_env ? " DXL_ENGINE names it, over the one chosen here." : "");
    } else if (!strcmp(st->id, "profile.game")) {
        const dxl_install *in = &ss->install;
        if (in->ok)
            snprintf(out, size, "Deus Ex %s (%s), in %s. %s", in->build->version, in->build->edition,
                     dxl_shell_tilde(in->root, shown, sizeof shown), st->help);
        else snprintf(out, size, "%s", st->help);
    } else {
        out[0] = '\0';
    }
}

/* One row: the control for its kind, the change kept in the stores. */
void dxl_settings_row(dxl_shell *s, const dxl_setting *st, uint32_t id, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    char v[512];
    switch (st->kind) {
    case DXL_SET_BOOL: {
        int on = dxl_setting_bool(&s->stores, st);
        if (dxl_toggle(ui, id, r, st->label, &on, 0)) {
            dxl_setting_set(&s->stores, st, on ? "1" : "0");
            live(s, st);
        }
        break;
    }
    case DXL_SET_NUMBER: {
        double n = dxl_setting_number(&s->stores, st);
        char shown[64];
        snprintf(shown, sizeof shown, st->format ? st->format : "%.2f", n * (st->shown_scale ? st->shown_scale : 1));
        if (dxl_slider(ui, id, r, st->label, &n, st->min, st->max, st->step, shown, 0)) {
            snprintf(v, sizeof v, "%.6g", n);
            dxl_setting_set(&s->stores, st, v);
            if (strcmp(st->id, "launcher.ui_scale") != 0) live(s, st);
        }
        /* The launcher's size follows its slider once the pointer lets go of
         * it: under a drag, the slider itself would move. */
        if (!strcmp(st->id, "launcher.ui_scale") && ui->held != id) live(s, st);
        break;
    }
    case DXL_SET_CHOICE: {
        static const char *labels[DXL_PICK_MAX];
        static char values[DXL_PICK_MAX][64], shown[DXL_PICK_MAX][64];
        /* Anti-aliasing on a GPU that multisamples badly: Off alone. The row
         * still takes the focus, so the list's moves pass through it and its
         * help line says why (dxl_page_settings). */
        if (!strcmp(st->id, DXL_ANTIALIAS_ROW)) {
            dxl_shell_probe(s);
            if (s->stores.no_msaa) {
                static const char *const off[] = { "Off" };
                int index = 0;
                dxl_spinner(ui, id, r, st->label, &index, off, 1, 0);
                break;
            }
        }
        int count = choices_of(s, st, labels, values, shown, DXL_PICK_MAX);
        const char *picked = dxl_shell_picked(s, dxl_shell_answer(s, id));
        for (int i = 0; picked && i < count; i++)
            if (!strcmp(labels[i], picked)) choose(s, st, values[i]);
        int index = current_of(s, st, values, count);
        int r2 = dxl_spinner(ui, id, r, st->label, &index, labels, count, 0);
        if (r2 == DXL_SPIN_CHANGED) choose(s, st, values[index]);
        else if (r2 == DXL_SPIN_LIST) dxl_shell_pick(s, id, st->label, labels, count, index);
        break;
    }
    case DXL_SET_TEXT: {
        size_t size = 0;
        char *buf = text_of(s, st, &size);
        if (!buf) break;
        /* Kept, the field shows what was: the row's characters alone, or its
         * default for none. */
        int kept = dxl_shell_answer(s, id) == 1;
        int r2 = dxl_text_field(ui, id, r, st->label, buf, size, NULL, 0);
        if (r2 == DXL_TEXT_OSK) dxl_shell_osk(s, id, st->label, buf, size);
        if (kept || r2 == DXL_TEXT_DONE) {
            dxl_setting_set(&s->stores, st, buf);
            dxl_setting_get(&s->stores, st, buf, size);
        }
        break;
    }
    case DXL_SET_ACTION:
        if (!strcmp(st->id, "launcher.engine")) engine_row(s, st, id, r);
        else if (!strcmp(st->id, "profile.game")) game_row(s, st, id, r);
        else if (dxl_button_id(ui, id, r, st->label, DXL_W_LEFT)) dxl_log("settings: %s", st->id);
        break;
    default:
        break;
    }
}

/* ---- the keys ------------------------------------------------------------ */

/* The actions a key is bound to, the game's key menu's (core/bindings.h),
 * but its "Nothing". */
static int action_count(void) { return (int)dxl_pad_action_count() - 1; }
static const dxl_pad_action *action(int a) { return dxl_pad_action_at((size_t)a + 1); }

static const char *action_label(const char *command) {
    int i = dxl_pad_action_find(command);
    return i > 0 ? dxl_pad_action_at((size_t)i)->label : command;
}

/* The action taken off the slot's key: the key's other commands kept. */
static void unbind(dxl_ini *user, const char *key, const char *command) {
    char name[32];
    snprintf(name, sizeof name, "%s", key);   /* key may be the ini's own */
    dxl_key_unbind(user, name, command);
}

/* key for action a in slot, both: as well as what it runs; the slot's old
 * key let go of. A key that runs the action already keeps its binding. */
static void bind(dxl_shell *s, int a, int slot, const char *key, int both) {
    dxl_ini *user = s->stores.user;
    const dxl_pad_action *act = action(a);
    const char *keys[2];
    char old[32] = "", now[512];
    int n = dxl_keys_bound(user, act->command, keys, 2);
    if (slot < n) snprintf(old, sizeof old, "%s", keys[slot]);
    snprintf(now, sizeof now, "%s", dxl_key_binding(user, key));
    if (dxl_key_binds(now, act->command)) {
        /* as it is */
    } else if (both && now[0]) {
        char combined[600];
        snprintf(combined, sizeof combined, "%s|%s", now, act->command);
        dxl_key_bind(user, key, combined);
    } else {
        dxl_key_bind(user, key, act->command);
    }
    if (old[0] && dxl_stricmp(old, key) != 0) unbind(user, old, act->command);
    dxl_log("keys: %s for %s%s", key, act->command, both ? ", and as it was" : "");
}

static void captured(dxl_shell *s, int a, int slot) {
    dxl_ui *ui = &s->ui;
    const char *key = NULL;
    switch (ui->in.captured) {
    case DXL_CAPTURED_KEY: key = dxl_key_from_scancode(ui->in.captured_code); break;
    case DXL_CAPTURED_MOUSE: key = dxl_key_from_mouse(ui->in.captured_code); break;
    case DXL_CAPTURED_WHEEL: key = dxl_key_from_wheel(ui->in.captured_code); break;
    default: break;
    }
    if (ui->in.captured == DXL_CAPTURED_PAD || ui->in.captured == DXL_CAPTURED_TRIGGER) {
        dxl_shell_toast(s, "A pad's buttons are bound on the Controller page.");
        return;
    }
    if (!key) {
        dxl_shell_toast(s, "The game does not see that key: the engine's window passes it on as none.");
        return;
    }
    const dxl_pad_action *act = action(a);
    const char *other = dxl_key_binding(s->stores.user, key);
    if (*other && !dxl_key_binds(other, act->command)) {
        char text[600];
        snprintf(text, sizeof text, "%s is bound to %s. Bind it to %s instead, or to both?", dxl_key_label(key),
                 action_label(other), act->label);
        s->bind_action = a;
        s->bind_slot = slot;
        snprintf(s->bind_key, sizeof s->bind_key, "%s", key);
        dxl_shell_confirm3(s, dxl_ui_id(ui, "keys conflict"), dxl_key_label(key), text, "Instead", "Both", "Cancel");
        return;
    }
    bind(s, a, slot, key, 0);
}

/* An action's row: its label, and its two keys' slots. */
static void key_row(dxl_shell *s, const dxl_list *l, int i, int a, dxl_rect r, const char **why) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    const dxl_pad_action *act = action(a);
    const char *bound[2] = { NULL, NULL };
    int n = dxl_keys_bound(s->stores.user, act->command, bound, 2);
    /* Copied: a slot's change rewrites the ini's lines under the names. */
    char names[2][32];
    const char *keys[2] = { NULL, NULL };
    for (int c = 0; c < n && c < 2; c++) {
        snprintf(names[c], sizeof names[c], "%s", bound[c]);
        keys[c] = names[c];
    }
    int lit = 0;
    for (int c = 0; c < 2; c++)
        if (ui->focus == dxl_list_cell_id(l, i, c)) lit = 1;
    if (lit) dxl_fill(ui, r, t->row_focus);
    dxl_rect label = { r.x + 16, r.y, r.w - 2 * SLOT_W - 48, r.h };
    dxl_label(ui, label, DXL_FONT_BODY, act->label, lit ? t->text_focus : t->text, DXL_W_LEFT | DXL_W_MAY_CUT);
    for (int c = 0; c < 2; c++) {
        dxl_rect slot = { r.x + r.w - 16 - (2 - c) * SLOT_W - (1 - c) * 10, r.y + 5, SLOT_W, r.h - 10 };
        uint32_t id = dxl_list_cell_id(l, i, c);
        int seen = !keys[c] || dxl_key_seen(keys[c]);
        int got = dxl_key_slot(ui, id, slot, keys[c] ? dxl_key_label(keys[c]) : NULL, seen ? 0 : DXL_W_WARN);
        char name[96];
        snprintf(name, sizeof name, "%s, key %d", act->label, c + 1);
        dxl_ui_name(ui, id, name);
        if (got == DXL_SLOT_CAPTURED) captured(s, a, c);
        else if (got == DXL_SLOT_CLEARED && keys[c]) unbind(s->stores.user, keys[c], act->command);
        if (ui->focus == id && keys[c] && !seen)
            *why = "The engine's window does not pass this key on, so the game never sees it: bind another.";
    }
}

/* The help line: the row's, and its choice's. */
static void help(dxl_shell *s, const dxl_setting *st, dxl_rect r) {
    if (!st) return;
    dxl_ui *ui = &s->ui;
    char text[1200];
    snprintf(text, sizeof text, "%s", st->help ? st->help : "");
    if (st->kind == DXL_SET_CHOICE) {
        int i = dxl_setting_choice(&s->stores, st);
        if (i >= 0 && st->choices[i].help) {
            size_t len = strlen(text);
            snprintf(text + len, sizeof text - len, "%s%s", len ? " " : "", st->choices[i].help);
        }
    }
    dxl_paragraph_typed(ui, r, DXL_FONT_SMALL, text, ui->theme.help, ui->focus_since);
}

void dxl_page_settings(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    if (!s->settings_open) dxl_page_settings_enter(s);
    if (!s->settings_open) {
        dxl_paragraph(ui, r, DXL_FONT_BODY, "There is no profile to keep settings in yet.", t->warn);
        return;
    }
    s->stores.no_msaa = s->probe_state == 2 && dxl_gpu_msaa_broken(&s->probe);
    int pages = (int)dxl_settings_page_count();
    const char *names[16];
    for (int i = 0; i < pages && i < 16; i++) names[i] = dxl_settings_page((size_t)i);
    if (s->settings_tab >= pages) s->settings_tab = 0;

    float w = r.w < 1100 ? r.w : 1100;
    dxl_rect area = { r.x, r.y, w, r.h };
    int busy = ui->text_id || ui->capture_id;
    int was = s->settings_tab;
    if (!busy) dxl_tabs(ui, "settings tabs", dxl_cut_top(&area, 42), names, pages, &s->settings_tab);
    else dxl_cut_top(&area, 42);
    if (s->settings_tab != was) dxl_ui_forget(ui);
    dxl_cut_top(&area, 10);

    dxl_rect buttons = dxl_cut_bottom(&area, 46);
    dxl_cut_bottom(&area, 10);
    dxl_rect help_r = dxl_cut_bottom(&area, 54);
    dxl_cut_bottom(&area, 8);

    const dxl_setting *rows[64];
    int n = dxl_settings_rows(names[s->settings_tab], rows, 64), settings_n = n;
    int keys_page = !strcmp(names[s->settings_tab], KEYS_PAGE) && s->stores.user;
    int pad_page = !strcmp(names[s->settings_tab], PAD_PAGE);
    if (keys_page) n += action_count();
    const dxl_setting *focused = NULL;
    const char *why = NULL;
    dxl_list l;
    if (pad_page) {
        dxl_page_controller(s, area, &why, &focused);
        n = 0;
    } else {
        dxl_well(ui, area);
        char list_id[64];
        snprintf(list_id, sizeof list_id, "settings %s", names[s->settings_tab]);
        dxl_list_begin(ui, &l, list_id, dxl_inset(area, 4), n, ROW_H, DXL_LIST_WHOLE_ROWS);
    }
    for (int i = 0; i < n; i++) {
        dxl_rect rr;
        if (!dxl_list_rect(&l, i, &rr)) continue;
        uint32_t id = dxl_list_row_id(&l, i);
        if (i < settings_n) {
            dxl_settings_row(s, rows[i], id, rr);
            if (ui->focus == id) focused = rows[i];
        } else {
            key_row(s, &l, i, i - settings_n, rr, &why);
            if (ui->focus == id || ui->focus == dxl_list_cell_id(&l, i, 1))
                why = why ? why : "Enter or a click takes the next key, mouse button or wheel notch; Delete takes "
                                  "the key away.";
        }
    }
    if (!pad_page) dxl_list_end(ui, &l);
    char now[1200] = "";
    if (!why && focused && focused->kind == DXL_SET_ACTION) {
        row_why(s, focused, now, sizeof now);
        if (now[0]) why = now;
    }
    if (!why && focused && !strcmp(focused->id, DXL_ANTIALIAS_ROW) && s->stores.no_msaa) {
        snprintf(now, sizeof now, "Off, and kept off: this GPU, %s, resolves multisampling into speckle at the "
                 "edges of what it draws.", s->probe.vulkan_device);
        why = now;
    }
    if (why) dxl_paragraph_typed(ui, help_r, DXL_FONT_SMALL, why, t->help, ui->focus_since);
    else help(s, focused, help_r);
    int conflict = dxl_shell_answer(s, dxl_ui_id(ui, "keys conflict"));
    if (conflict > 0) bind(s, s->bind_action, s->bind_slot, s->bind_key, conflict == 2);

    /* As -changevideo opened it, as the original's renderer page: on to the
     * game; not with an engine --version refuses, which it says beside
     * Play, as Home does. Opened again from Home, Done. */
    int graphics = s->settings_plays;
    if (graphics) dxl_shell_ask_engine(s);
    int refused = graphics && !s->engine_ok;
    int reset = dxl_button(ui, "settings reset", dxl_cut_left(&buttons, 230), "Back to the defaults", 0);
    int done = dxl_button(ui, "settings done", dxl_cut_right(&buttons, 150), graphics ? "Play" : "Done",
                          DXL_W_PRIMARY | (refused ? DXL_W_DISABLED : 0));
    if (refused) {
        char said[300];
        snprintf(said, sizeof said, "The engine: %s. The Launcher page has another.", s->engine_brief);
        dxl_rect beside = { buttons.x + 16, buttons.y, buttons.w - 32, buttons.h };
        dxl_label(ui, beside, DXL_FONT_SMALL, said, t->bad, DXL_W_RIGHT | DXL_W_MAY_CUT);
    }
    if (reset) {
        dxl_shell_confirm(s, dxl_ui_id(ui, "settings reset"), names[s->settings_tab],
                          "Every setting on this page back to its default?", "Reset", "Keep them");
    }
    if (dxl_shell_answer(s, dxl_ui_id(ui, "settings reset")) == 1) {
        dxl_settings_defaults(s, names[s->settings_tab]);
        dxl_shell_toast(s, "%s: back to the defaults.", names[s->settings_tab]);
    }
    if (done && graphics) {
        dxl_settings_keep(s);
        s->result = 1;
    } else if (done || (ui->in.back && !busy)) {
        dxl_shell_go(s, DXL_PAGE_HOME);
    }
}
