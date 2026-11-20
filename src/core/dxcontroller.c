/* core/dxcontroller.h: the option's rules, the patched DeusEx.u, and what a
 * start writes for the mod and takes back. */
#include "core/dxcontroller.h"
#include "core/bindings.h"
#include "core/cmdline.h"
#include "core/engine_settings.h"
#include "core/log.h"
#include "core/patch.h"
#include "core/paths.h"
#include "core/sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* profile.ini: what a start changed, kept before it changes it --
 * [DXController.Undo] Made, and the player's Console and Root where the ini
 * had them; [DXController.Input] the player's own bindings of the keys the
 * mod binds, where User.ini had them. */
#define UNDO  "DXController.Undo"
#define INPUT "DXController.Input"
#define ENGINE_SECTION "Engine.Engine"
/* The game's own, Default.ini's: a console the stock DeusEx.u has. */
#define STOCK_CONSOLE "Engine.Console"
#define STOCK_ROOT    "DeusEx.DeusExRootWindow"

/* The mod's launcher's bindings (its README's Controls): the triggers, the
 * shoulders and the stick clicks are its console's own, bound to nothing. */
static const dxl_dxc_binding bindings[] = {
    { "Joy1", "Jump" }, { "Joy2", "ReloadWeapon" }, { "Joy3", "ParseRightClick" }, { "Joy4", "SwitchAmmo" },
    { "Joy5", "" }, { "Joy6", "" }, { "Joy7", "TogglePlayerMenuWindow" }, { "Joy8", "ShowMainMenu" },
    { "Joy9", "" }, { "Joy10", "" }, { "Joy11", "" }, { "Joy12", "" }, { "Joy13", "" }, { "Joy14", "" },
    { "Joy15", "" }, { "Joy16", "" },
    { "JoyPovUp", "ActivateBelt 1" }, { "JoyPovLeft", "ActivateBelt 2" }, { "JoyPovRight", "ActivateBelt 3" },
    { "JoyPovDown", "ActivateBelt 4" },
    { "JoyX", "Axis aStrafe" }, { "JoyY", "Axis aBaseY" }, { "JoyU", "Axis aTurn" }, { "JoyV", "Axis aLookUp" },
    { "JoyZ", "" }, { "JoyR", "" },
};

size_t dxl_dxc_binding_count(void) { return sizeof bindings / sizeof *bindings; }
const dxl_dxc_binding *dxl_dxc_binding_at(size_t i) { return i < dxl_dxc_binding_count() ? &bindings[i] : NULL; }

int dxl_dxc_option(const dxl_profile *p) { return dxl_ini_get_bool(p->ini, "Profile", DXL_DXC_OPTION, 1); }

static char *app_file(const dxl_dxc_place *w, const char *leaf) {
    char *dir = dxl_path_join(w->app_data, DXL_DXC_FOLDER);
    char *f = dxl_path_join(dir, leaf);
    free(dir);
    return f;
}

/* The player's DeusEx.u's SHA-1, read again only when its size or time
 * changed: the Controller page asks each frame. */
static int stock_sha1(const char *path, char hex[41]) {
    static char last_path[1024], last_hex[41];
    static long long last_size = -1;
    static long last_time = -1;
    long long size = dxl_path_size(path);
    long time = dxl_path_mtime(path);
    if (size >= 0 && size == last_size && time == last_time && !strcmp(path, last_path)) {
        memcpy(hex, last_hex, 41);
        return 0;
    }
    if (size < 0 || dxl_sha1_file(path, hex) != 0) return -1;
    snprintf(last_path, sizeof last_path, "%s", path);
    memcpy(last_hex, hex, 41);
    last_size = size;
    last_time = time;
    return 0;
}

int dxl_dxc_able(const dxl_dxc_place *w, char *why, size_t size) {
    const char *mod = dxl_profile_get(w->profile, "Mod");
    if (mod && *mod) {
        snprintf(why, size, "A mod's profile plays the mod alone: DXController is for the game itself.");
        return 0;
    }
    if (!w->engine_has) {
        snprintf(why, size, "This engine has no pad mode for it: the one that came with the launcher has, "
                            "chosen in Settings, Launcher, The engine.");
        return 0;
    }
    char *patch = app_file(w, DXL_DXC_PATCH), *package = app_file(w, DXL_DXC_PACKAGE);
    dxl_patch_info info;
    int whole = dxl_patch_info_read(patch, &info, NULL) == 0 && dxl_path_size(package) > 0;
    free(patch);
    free(package);
    if (!whole) {
        snprintf(why, size, "The launcher's data/%s is not whole, DXController.u or its patch missing: "
                            "install the launcher again.",
                 DXL_DXC_FOLDER);
        return 0;
    }
    char *stock = w->game_system ? dxl_path_resolve_ci(w->game_system, "DeusEx.u") : NULL;
    char hex[41];
    int ours = stock && stock_sha1(stock, hex) == 0 && !strcmp(hex, info.source_sha1);
    free(stock);
    if (!ours) {
        snprintf(why, size, "The game's DeusEx.u is not the one the patch is for (GOG's and Steam's): a "
                            "translation or a mod changed it.");
        return 0;
    }
    return 1;
}

char *dxl_dxc_make(const dxl_dxc_place *w, dxl_err *err) {
    char *patch = app_file(w, DXL_DXC_PATCH);
    dxl_patch_info info;
    if (dxl_patch_info_read(patch, &info, err) != 0) {
        free(patch);
        return NULL;
    }
    char leaf[32];
    snprintf(leaf, sizeof leaf, "%.12s-%.12s", info.source_sha1, info.result_sha1);
    char *base = dxl_path_join(w->data_home, DXL_DXC_FOLDER), *dir = dxl_path_join(base, leaf);
    char *made = dxl_path_join(dir, "DeusEx.u"), *stock = dxl_path_resolve_ci(w->game_system, "DeusEx.u");
    char hex[41];
    int rc = 0;
    if (dxl_path_size(made) != (long long)info.result_size || dxl_sha1_file(made, hex) != 0 ||
        strcmp(hex, info.result_sha1) != 0) {
        if (!stock) {
            dxl_err_set(err, "the game's System has no DeusEx.u");
            rc = -1;
        } else if (dxl_path_mkdirs(dir) != 0) {
            dxl_err_set(err, "cannot make %s", dir);
            rc = -1;
        } else {
            rc = dxl_patch_apply(stock, patch, made, err);
        }
    }
    free(patch);
    free(base);
    free(made);
    free(stock);
    if (rc != 0) {
        free(dir);
        return NULL;
    }
    return dir;
}

/* A made folder's name: <12 hex digits>-<12 hex digits> (dxl_dxc_make). */
static int made_leaf(const char *s) {
    if (strlen(s) != 25 || s[12] != '-') return 0;
    for (int i = 0; i < 25; i++)
        if (i != 12 && !strchr("0123456789abcdef", s[i])) return 0;
    return 1;
}

/* A path of the profile's that is one of ours: the packages ("*.u") of a
 * folder named dxcontroller (the app's), or of a made folder in one (the
 * data home's), by either slash; not a mod's own System beneath a folder
 * of that name. */
static int ours(const char *value) {
    size_t n = strlen(value);
    if (n < 5 || dxl_stricmp(value + n - 3, "*.u") != 0 || (value[n - 4] != '/' && value[n - 4] != '\\'))
        return 0;
    char *v = dxl_xstrndup(value, n - 4);
    for (char *c = v; *c; c++)
        if (*c == '\\') *c = '/';
    const char *last = strrchr(v, '/');
    int hit = !dxl_stricmp(last ? last + 1 : v, DXL_DXC_FOLDER);
    if (!hit && last && made_leaf(last + 1)) {
        *(char *)last = '\0';
        const char *up = strrchr(v, '/');
        hit = !dxl_stricmp(up ? up + 1 : v, DXL_DXC_FOLDER);
    }
    free(v);
    return hit;
}

/* The profile's paths without ours; with first's before them, when given.
 * Left as they are where they are so already. */
static void set_paths(dxl_ini *sys, const char *const *first, size_t nfirst) {
    size_t n = dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0), kept = 0;
    const char **values = dxl_xmalloc((n + 1) * sizeof *values);
    dxl_ini_get_all(sys, "Core.System", "Paths", values, n);
    char **copies = dxl_xmalloc((n + 1) * sizeof *copies);
    int changed = 0;
    for (size_t i = 0; i < n; i++) {
        if (i < nfirst ? strcmp(values[i], first[i]) != 0 : ours(values[i])) changed = 1;
        if (!ours(values[i])) copies[kept++] = dxl_xstrdup(values[i]);
    }
    if (n < nfirst) changed = 1;
    free(values);
    if (changed) {
        dxl_ini_remove(sys, "Core.System", "Paths");
        for (size_t i = 0; i < nfirst; i++) dxl_ini_append(sys, "Core.System", "Paths", first[i]);
        for (size_t i = 0; i < kept; i++) dxl_ini_append(sys, "Core.System", "Paths", copies[i]);
    }
    for (size_t i = 0; i < kept; i++) free(copies[i]);
    free(copies);
}

static void set_or_remove(dxl_ini *ini, const char *section, const char *key, const char *value) {
    if (value) {
        const char *now = dxl_ini_get(ini, section, key);
        if (!now || strcmp(now, value) != 0) dxl_ini_set(ini, section, key, value);
    } else if (dxl_ini_get(ini, section, key)) {
        dxl_ini_remove(ini, section, key);
    }
}

static char *settings_json(const dxl_dxc_place *w, char **def) {
    *def = dxl_path_join(w->app_data, "engine-settings.json.default");
    return dxl_path_join(w->profile->engine_dir, "Settings.json");
}

/* The engine's pad in the mod's mode, or not: Settings.json written where
 * it changes. */
static int pad_mode(const dxl_dxc_place *w, int on, dxl_err *err) {
    char *def, *json = settings_json(w, &def);
    dxl_engine_settings *es = dxl_es_open(json, def);
    int rc = 0;
    if (dxl_es_bool(es, DXL_ES_PAD_DXCONTROLLER) != on || (on && !dxl_es_existed(es))) {
        dxl_es_set_bool(es, DXL_ES_PAD_DXCONTROLLER, on);
        rc = dxl_es_save(es, err);
    }
    dxl_es_free(es);
    free(json);
    free(def);
    return rc;
}

static int recorded(const dxl_profile *p) { return dxl_ini_get(p->ini, UNDO, "Made") != NULL; }

int dxl_dxc_rest(const dxl_dxc_place *w, dxl_err *err) {
    dxl_ini *sys = dxl_config_ini(w->config), *usr = dxl_config_user_ini(w->config);
    dxl_ini *pini = w->profile->ini;
    int had = recorded(w->profile);
    /* Not the stock DeusEx.u's console and root with our packages gone: the
     * player's where kept, else the game's own. */
    const char *names[2] = { "Console", "Root" }, *ours_now[2] = { DXL_DXC_CONSOLE, DXL_DXC_ROOT };
    const char *stock[2] = { STOCK_CONSOLE, STOCK_ROOT };
    int changed = 0;
    for (int i = 0; i < 2; i++) {
        const char *now = dxl_ini_get(sys, ENGINE_SECTION, names[i]);
        if (had) {
            const char *was = dxl_ini_get(pini, UNDO, names[i]);
            if (!now || !was || strcmp(now, was) != 0) changed = 1;
            set_or_remove(sys, ENGINE_SECTION, names[i], was);
        } else if (now && !dxl_stricmp(now, ours_now[i])) {
            dxl_ini_set(sys, ENGINE_SECTION, names[i], stock[i]);
            changed = 1;
        }
    }
    size_t before = dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0);
    set_paths(sys, NULL, 0);
    if (dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0) != before) changed = 1;
    for (size_t i = 0; had && i < dxl_dxc_binding_count(); i++) {
        const char *was = dxl_ini_get(pini, INPUT, bindings[i].key);
        const char *now = dxl_ini_get(usr, DXL_INPUT_SECTION, bindings[i].key);
        /* A binding the game wrote while the mod was on, neither the mod's
         * nor the player's kept one: dropped for the player's, and said. */
        int kept = now && was ? !strcmp(now, was) : now == was;
        if (now && dxl_stricmp(now, bindings[i].binding) != 0 && !kept) {
            if (was)
                dxl_log("DXController: %s set back to the player's \"%s\", the \"%s\" a game with it on wrote dropped",
                        bindings[i].key, was, now);
            else
                dxl_log("DXController: %s taken away, the player's none, the \"%s\" a game with it on wrote dropped",
                        bindings[i].key, now);
        }
        set_or_remove(usr, DXL_INPUT_SECTION, bindings[i].key, was);
    }
    if (!had && !changed) {
        /* Nothing of ours in the inis: the pad's mode alone, where a start
         * was cut short before it put it back. */
        return pad_mode(w, 0, err) == 0 ? 0 : -1;
    }
    /* The inis first: the console and the paths in one file, together. */
    if (dxl_config_save(w->config, err) != 0) return -1;
    if (pad_mode(w, 0, err) != 0) return -1;
    if (had) {
        dxl_ini_empty_section(pini, UNDO);
        dxl_ini_empty_section(pini, INPUT);
        if (dxl_profile_save(w->profile, err) != 0) return -1;
    }
    return 1;
}

/* What the start is about to change, kept in profile.ini and written before
 * anything is: once, so a start after one cut short keeps the player's. */
static int record(const dxl_dxc_place *w, dxl_err *err) {
    if (recorded(w->profile)) return 0;
    dxl_ini *sys = dxl_config_ini(w->config), *usr = dxl_config_user_ini(w->config);
    dxl_ini *pini = w->profile->ini;
    dxl_ini_empty_section(pini, UNDO);
    dxl_ini_empty_section(pini, INPUT);
    const char *names[2] = { "Console", "Root" };
    for (int i = 0; i < 2; i++) {
        const char *v = dxl_ini_get(sys, ENGINE_SECTION, names[i]);
        if (v) dxl_ini_set(pini, UNDO, names[i], v);
    }
    for (size_t i = 0; i < dxl_dxc_binding_count(); i++) {
        const char *v = dxl_ini_get(usr, DXL_INPUT_SECTION, bindings[i].key);
        if (v) dxl_ini_set(pini, INPUT, bindings[i].key, v);
    }
    dxl_ini_set(pini, UNDO, "Made", "1");
    return dxl_profile_save(w->profile, err);
}

int dxl_dxc_start(const dxl_dxc_place *w, int multiplayer, char *why, size_t size) {
    dxl_err e;
    why[0] = '\0';
    int wanted = dxl_dxc_option(w->profile);
    char able_why[300];
    int able = wanted && dxl_dxc_able(w, able_why, sizeof able_why);
    char *dir = NULL;
    if (able && !multiplayer) {
        dir = dxl_dxc_make(w, &e);
        if (!dir) snprintf(why, size, "DXController is off for this game: its DeusEx.u could not be made: %s",
                           dxl_err_msg(&e));
    }
    /* A failure is kept for the Controller page until a start that is not
     * one. */
    const char *problem = dxl_profile_get(w->profile, DXL_DXC_PROBLEM);
    if (why[0] || (problem && *problem)) {
        if (why[0]) dxl_profile_set(w->profile, DXL_DXC_PROBLEM, why);
        else dxl_ini_remove(w->profile->ini, "Profile", DXL_DXC_PROBLEM);
    }
    if (!dir) {
        if (dxl_dxc_rest(w, &e) < 0 && !why[0])
            snprintf(why, size, "DXController's changes could not be taken back: %s", dxl_err_msg(&e));
        if (why[0]) return -1;
        return able && multiplayer ? DXL_DXC_MULTIPLAYER : 0;
    }
    if (record(w, &e) != 0) {
        snprintf(why, size, "DXController is off for this game: profile.ini could not keep the player's own: %s",
                 dxl_err_msg(&e));
        free(dir);
        dxl_dxc_rest(w, NULL);
        return -1;
    }
    /* The inis: written by the caller's save, the paths and the console in
     * one file. Settings.json's pad mode after it (dxl_dxc_pad). */
    dxl_ini *sys = dxl_config_ini(w->config), *usr = dxl_config_user_ini(w->config);
    char *made = dxl_path_join(dir, "*.u"), *app = dxl_path_join(w->app_data, DXL_DXC_FOLDER);
    char *shipped = dxl_path_join(app, "*.u");
    const char *first[2] = { made, shipped };
    set_paths(sys, first, 2);
    set_or_remove(sys, ENGINE_SECTION, "Console", DXL_DXC_CONSOLE);
    set_or_remove(sys, ENGINE_SECTION, "Root", DXL_DXC_ROOT);
    for (size_t i = 0; i < dxl_dxc_binding_count(); i++)
        set_or_remove(usr, DXL_INPUT_SECTION, bindings[i].key, bindings[i].binding);
    free(made);
    free(app);
    free(shipped);
    free(dir);
    return 1;
}

int dxl_dxc_pad(const dxl_dxc_place *w, int saved, dxl_err *err) { return pad_mode(w, saved, err); }

int dxl_dxc_multiplayer(const char *url, const char *words) {
    if (url && *url && *url != '?') return 1;
    if (!words) return 0;
    if (dxl_cmd_param(words, "server") || dxl_stristr(words, "?listen")) return 1;
    /* The original's first word, a URL: an address joins. */
    const char *w = words;
    while (*w == ' ' || *w == '\t') w++;
    size_t n = strcspn(w, " \t?");
    if (!n || *w == '-' || *w == '/' || memchr(w, '=', n)) return 0;
    if (memchr(w, ':', n)) return 1;
    if (n > 3 && !dxl_strnicmp(w + n - 3, ".dx", 3)) return 0;
    return memchr(w, '.', n) != NULL;
}
