/* dxl-shots: the launcher's window, every page and dialog of it, drawn
 * offscreen -- SDL's software renderer on a surface, a clock of its own --
 * through the code the window runs, at the sizes it meets: 1280x720,
 * 1280x800, 1920x1080, 2560x1440 at 1.5 px a dp, 3840x2160 at 2, and
 * 960x600, the smallest; 1920x1080 and 1280x800 on the big screen, a
 * television's and a Steam Deck's; with the keyboard, the mouse, and virtual pads
 * with an Xbox's, a DualSense's, a Switch Pro's and an unknown pad's ids.
 * A kit diagnostic in any frame -- a label cut short, focusables on one
 * another or off the window -- fails it.
 *
 *   dxl-shots [--out=<dir>] [--all] [--only=<shot>] [--fonts=<dir>]
 *
 * With --out it writes PNGs for review: every page at 1280x720 with the
 * keyboard, Home at each size and with each device; --all writes them all.
 * The states are made up, in memory and in a scratch home it makes under
 * $TMPDIR and removes after (its HOME, so the paths shown read ~/...):
 * nothing of the game's is read, and nothing is written anywhere else.
 */
#define _GNU_SOURCE
#include "core/bindings.h"
#include "core/gamedb.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/settings.h"
#include "screens/shell.h"
#include "ui/text.h"
#include "ui/widgets.h"
#include "upkg_writer.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef DXL_SHOTS_FONTS
#define DXL_SHOTS_FONTS "data/fonts"
#endif

typedef struct {
    int w, h;
    float scale;
    int big;     /* the big screen's: its scale the screen's */
} size;

static const size sizes[] = {
    { 1280, 720, 1, 0 }, { 1280, 800, 1, 0 }, { 1920, 1080, 1, 0 }, { 2560, 1440, 1.5f, 0 }, { 3840, 2160, 2, 0 },
    { 960, 600, 1, 0 }, { 1920, 1080, 1, 1 }, { 1280, 800, 1, 1 },
};

typedef enum { MODE_KEYBOARD, MODE_MOUSE, MODE_XBOX, MODE_PS, MODE_SWITCH, MODE_PAD, MODE_COUNT } mode;

static const char *const mode_names[MODE_COUNT] = { "keyboard", "mouse", "xbox", "dualsense", "switchpro", "pad" };

static const struct {
    Uint16 vendor, product;
    const char *name;
} pads[] = {
    { 0x045e, 0x0b12, "Xbox Series X Controller" },
    { 0x054c, 0x0ce6, "DualSense Wireless Controller" },
    { 0x057e, 0x2009, "Nintendo Switch Pro Controller" },
    { 0x1234, 0x5678, "Unknown Pad" },
};
static SDL_JoystickID pad_ids[4];
/* A joystick SDL has no mapping for, for the mapping wizard. */
static SDL_JoystickID raw_id;

typedef enum {
    SCENE_WELCOME,
    SCENE_FIND,
    SCENE_FIND_NONE,
    SCENE_FIND_LOOKING,     /* the finder still looking: nothing yet, */
    SCENE_FIND_MORE,        /* and two found so far */
    SCENE_BROWSER,
    SCENE_MESSAGE,
    SCENE_IMPORT,
    SCENE_HOME,
    SCENE_HOME_MADE,
    SCENE_HOME_ENGINE,      /* an engine this system cannot start */
    SCENE_THEME_PASTEL,     /* Home in two of the game's menu themes: a light one, */
    SCENE_THEME_RED,        /* and Settings in a strong one */
    SCENE_REPOINT,
    SCENE_RECOVERY,
    SCENE_RECOVERY_SAVE,    /* a run started on a save that did not load */
    SCENE_SAFE,
    SCENE_ABOUT,
    SCENE_PROFILES,         /* three: the one in use, a mod's, one whose game is gone */
    SCENE_HOME_NEW,         /* a profile with no saves yet: Play first */
    SCENE_BIG_OFFER,        /* Home, a pad's first press asking whether the big screen should be */
    SCENE_SAVES,            /* thirteen, the quick save the newest, one with no snapshot */
    SCENE_SAVES_NONE,
    SCENE_SAVES_OLD,        /* an engine that starts on no save */
    SCENE_SHOTS,            /* seven, and one that does not read */
    SCENE_SHOTS_NONE,
    SCENE_SHOT_WHOLE,       /* the newest shown whole */
    SCENE_SERVERS,          /* nine answered -- one with a password, one of another version, a favourite -- and one silent */
    SCENE_SERVERS_ASKING,   /* the look on: three listed, one answered */
    SCENE_SERVERS_FAILED,   /* the master gave no list; a favourite answered */
    SCENE_SERVERS_PASSWORD, /* the chosen asks for a password */
    SCENE_MODS,             /* two in the game's folder, one played by a profile; one in the launcher's mods folder */
    SCENE_MODS_NONE,
    SCENE_MOD_PROFILE,      /* Your profile, a mod's */
    SCENE_SETTINGS,
    SCENE_PICK,
    SCENE_OSK,
    SCENE_CONFIRM,
    SCENE_LOG,
    SCENE_KEYS,
    /* The other settings pages, by their names in page_scenes. */
    SCENE_CONTROLLER,
    SCENE_CONTROLLER_END,   /* its rows' list paged to its end */
    SCENE_PADWIZARD,
    SCENE_PADTEST,          /* the controller tried: its north button and right trigger held */
    SCENE_GRAPHICS,
    SCENE_AUDIO,
    SCENE_GAMEPLAY,
    SCENE_MULTIPLAYER,
    SCENE_LAUNCHER,
    SCENE_COUNT
} scene;

static const char *const scene_names[SCENE_COUNT] = {
    "welcome", "find", "find-none", "find-looking", "find-more", "browser", "message", "import", "home", "home-made", "home-engine", "theme-pastel", "theme-red", "repoint",
    "recovery", "recovery-save", "safe", "about", "profiles", "home-new", "big-offer", "saves", "saves-none", "saves-old-engine", "shots",
    "shots-none", "shot-whole", "servers", "servers-asking", "servers-failed", "servers-password", "mods", "mods-none",
    "mod-profile", "settings", "pick", "osk", "confirm", "log", "keys",
    "controller", "controller-end", "padwizard", "padtest", "graphics", "audio", "gameplay", "multiplayer",
    "launcher",
};

/* The settings page each of those scenes shows. */
static const struct { scene sc; const char *page; } page_scenes[] = {
    { SCENE_KEYS, "Keyboard & mouse" }, { SCENE_CONTROLLER, "Controller" }, { SCENE_CONTROLLER_END, "Controller" },
    { SCENE_GRAPHICS, "Graphics" },
    { SCENE_AUDIO, "Audio" },
    { SCENE_GAMEPLAY, "Gameplay & HUD" }, { SCENE_MULTIPLAYER, "Multiplayer" }, { SCENE_LAUNCHER, "Launcher" },
};

static char home[512];
static const char *fonts_dir = DXL_SHOTS_FONTS;
static const char *out_dir;
static const char *only;
static int all;
static int failures, shots, saved;

/* ---- the made-up states ----------------------------------------------- */

static void write_file(const char *path, const char *text) {
    char *dir = dxl_path_dirname(path);
    dxl_path_mkdirs(dir);
    free(dir);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static char *at_home(const char *rel) { return dxl_path_join(home, rel); }

/* An install the engine plays, as the screens see it: no files behind it. */
static void good_install(dxl_install *in, const char *rel, int build) {
    memset(in, 0, sizeof *in);
    in->root = at_home(rel);
    in->system_dir = dxl_path_join(in->root, "System");
    in->build = dxl_gamedb_at((size_t)build);
    snprintf(in->sha1, sizeof in->sha1, "%s", in->build->sha1);
    in->found = 1;
    in->ok = 1;
}

/* The scratch home: its folders for the browser, a copy of the game's
 * folder the engine would not take (files of a few bytes), and a profile
 * with saves and a log. */
static void make_home(void) {
    static const char *dirs[] = { "Desktop", "Documents", "Downloads", "Games/Heroic", "Music", "Pictures",
                                  "Videos", ".local/share", ".steam" };
    for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++) {
        char *p = at_home(dirs[i]);
        dxl_path_mkdirs(p);
        free(p);
    }
    static const char *files[] = { "System/DeusEx.exe", "System/Core.u", "System/Engine.u", "System/Extension.u",
                                   "System/DeusEx.u", "System/DeusExUI.u", "System/Default.ini",
                                   "System/DefUser.ini", "Maps/DX.dx" };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        char rel[256];
        snprintf(rel, sizeof rel, "Games/Heroic/Deus Ex GOTY/%s", files[i]);
        char *p = at_home(rel);
        write_file(p, "x");
        free(p);
    }
}

/* Twelve saves at the game's places and a quick save after them, each with
 * a snapshot but the seventh's, made by a renderer that takes none. */
static void make_saves(const char *dir) {
    static const char *const where[12][2] = {
        { "Before the statue", "NYC - Liberty Island" }, { "UNATCO Headquarters", "UNATCO Headquarters" },
        { "The subway", "NYC - Battery Park" }, { "NYC - Hell's Kitchen", "NYC - Hell's Kitchen" },
        { "Paul's apartment", "NYC - Hell's Kitchen" }, { "LaGuardia Airport", "LaGuardia Airport" },
        { "UNATCO Headquarters", "UNATCO Headquarters" }, { "The tong's temple", "Hong Kong - Wan Chai Market" },
        { "Hong Kong - VersaLife", "Hong Kong - VersaLife" }, { "The catacombs", "Paris - Denfert-Rochereau" },
        { "Vandenberg Air Force Base", "Vandenberg Air Force Base" }, { "Area 51", "Area 51" },
    };
    for (int i = 0; i < 12; i++) {
        dxl_test_save sv = { { 2026, 10, 1 + i, 9 + i % 12, 7 * i % 60, 0 }, i + 1, where[i][0], where[i][1], NULL,
                             3600 * i + 1200 + 97 * i, i != 6 };
        char folder[16];
        snprintf(folder, sizeof folder, "Save%04d", i + 1);
        dxl_test_write_save(dir, folder, &sv);
    }
    dxl_test_save quick = { { 2026, 10, 16, 21, 40, 0 }, -1, "Quick Save", "Area 51", NULL, 44 * 3600 + 1800, 1 };
    dxl_test_write_save(dir, "QuickSave", &quick);
}

/* Seven shots of the game's size, made up -- a sky, a horizon, blocks of
 * colour -- a minute apart, and a file that does not read. */
static void make_shots(const char *dir) {
    dxl_path_mkdirs(dir);
    for (int i = 0; i < 7; i++) {
        SDL_Surface *pic = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_XRGB8888);
        if (!pic) continue;
        const SDL_PixelFormatDetails *f = SDL_GetPixelFormatDetails(pic->format);
        for (int y = 0; y < 720; y += 8) {
            SDL_Rect band = { 0, y, 1280, 8 };
            int k = y * 255 / 720;
            SDL_FillSurfaceRect(pic, &band, SDL_MapRGB(f, NULL, (Uint8)(20 + k / 4), (Uint8)(30 + k / 3), (Uint8)(60 + k / 2)));
        }
        for (int b = 0; b < 9; b++) {
            int h = 120 + ((b * 53 + i * 31) % 260);
            SDL_Rect block = { b * 142 + 6, 520 - h, 120, h + 200 };
            SDL_FillSurfaceRect(pic, &block, SDL_MapRGB(f, NULL, (Uint8)(40 + 25 * i), (Uint8)(50 + 18 * b), 70));
        }
        char leaf[32];
        snprintf(leaf, sizeof leaf, "Shot%04d.bmp", i + 1);
        char *path = dxl_path_join(dir, leaf);
        SDL_SaveBMP(pic, path);
        struct timeval tv[2] = { { 1792186000 + 60 * i, 0 }, { 1792186000 + 60 * i, 0 } };
        utimes(path, tv);
        free(path);
        SDL_DestroySurface(pic);
    }
    char *bad = dxl_path_join(dir, "Shot0008.bmp");
    write_file(bad, "not a picture");
    struct timeval tv[2] = { { 1792185000, 0 }, { 1792185000, 0 } };
    utimes(bad, tv);
    free(bad);
}

/* A run as run.c has it: the session in the scratch home, the profile made
 * (profile.ini, DeusEx.ini, thirteen saves), its install the one given. */
static void make_run(dxl_run *r, int profile) {
    memset(r, 0, sizeof *r);
    dxl_err e;
    if (dxl_session_open(&r->session, NULL, &e) != 0) {
        fprintf(stderr, "dxl-shots: %s\n", dxl_err_msg(&e));
        exit(1);
    }
    if (!profile) return;
    dxl_profile *p = &r->session.profile;
    dxl_profile_make_dirs(p, &e);
    dxl_profile_set(p, "Name", "Deus Ex");
    dxl_profile_set(p, "Install", "1112fm-00000000");
    dxl_profile_save(p, &e);
    write_file(p->system_ini, "[Engine.Engine]\n");
    /* A few bindings of the shots' own, one on a key the engine's window
     * does not pass on. */
    write_file(p->user_ini, "[Engine.Input]\nW=MoveForward\nUp=MoveForward\nS=MoveBackward\nA=StrafeLeft\n"
                            "D=StrafeRight\nSpace=Jump\nX=Duck\nShift=Walking\nLeftMouse=ParseLeftClick|Fire\n"
                            "RightMouse=ParseRightClick\nR=ReloadWeapon\nF1=ShowInventoryWindow\n");
    make_saves(p->save_dir);
    good_install(&r->session.install, "GOG Games/Deus Ex GOTY", 0);
    r->config = dxl_config_open_files(r->session.install.system_dir, "DeusEx", p->system_ini, p->user_ini);
    r->last.known = 1;
    r->last.clean = 1;
    r->last.ready = 1;
    r->last.ended = 1792186920;   /* 16 October 2026, 21:42 UTC */
}

/* Mods of a few bytes each: GMDXv10, with its own settings and controls,
 * and TNM in the game's folder; Revision, with none, in the launcher's
 * mods folder. */
static void make_mods(const dxl_run *r) {
    static const char *const files[][2] = {
        { "GMDXv10/System/GMDX.u", "x" },
        { "GMDXv10/System/GMDXv10.ini", "[Core.System]\nPaths=..\\GMDXv10\\System\\*.u\nPaths=..\\System\\*.u\n" },
        { "GMDXv10/System/GMDXv10User.ini", "[Engine.Input]\n" },
        { "GMDXv10/Save/Save0001/SaveInfo.dxs", "x" },
        { "TNM/System/TNMText.u", "x" },
        { "TNM/System/TNM.ini", "[Core.System]\nPaths=..\\TNM\\System\\*.u\nPaths=..\\System\\*.u\n" },
    };
    for (size_t i = 0; i < sizeof files / sizeof *files; i++) {
        char *p = dxl_path_join(r->session.install.root, files[i][0]);
        write_file(p, files[i][1]);
        free(p);
    }
    char *mods = dxl_session_mods_dir(&r->session), *p = dxl_path_join(mods, "Revision/System/Revision.u");
    write_file(p, "x");
    free(p);
    free(mods);
}

static void crashed(dxl_run *r) {
    r->last.clean = 0;
    r->last.ready = 0;
    r->last.status = 139;
    r->last.signal = 11;
    char *log = dxl_path_join(r->session.profile.logs_dir, DXL_LASTRUN_LOG);
    write_file(log,
               "[] Game folder: ~/GOG Games/Deus Ex GOTY\n"
               "[] Renderer: Vulkan\n"
               "[] Loading map DX.dx\n"
               "[GameInfo.InitGame] Base Mutator is DX.Mutator1\n"
               "[GameInfo.Login] Login: Player\n"
               "[GameInfo.AcceptInventory] All inventory from Player is accepted\n"
               "[] Collecting garbage (map DX.dx)\n"
               "[] Purging garbage\n"
               "[] Loading map 01_NYC_UNATCOIsland.dx\n"
               "[] A made-up last line, for this shot: the run ended here, by SIGSEGV\n");
    free(log);
}

static void engine(dxl_shell *s) {
    s->engine_asked = 1;
    s->engine_ok = 1;
    snprintf(s->engine_version, sizeof s->engine_version, "VibeEngine 1e4258f (2026-10-16)");
    snprintf(s->engine_features, sizeof s->engine_features, "userdir loadgame-url int-paths");
}

static void found(dxl_shell *s) {
    dxl_found_list_free(&s->found);
    s->looked = 1;
    s->found.items[0].source = "in Steam's library";
    good_install(&s->found.items[0].install, ".local/share/Steam/steamapps/common/Deus Ex", 1);
    s->found.items[1].source = "in Heroic's games";
    char *bad = at_home("Games/Heroic/Deus Ex GOTY");
    dxl_install_check(bad, &s->found.items[1].install);
    free(bad);
    s->found.count = 2;
}

/* Made-up servers, as their answers would have them. */
static void server(dxl_shell *s, const char *address, int port, int ping, const char *name, const char *map,
                   const char *game, int players, int max, const char *more) {
    char status[1024];
    snprintf(status, sizeof status,
             "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\hostname\\%s\\hostport\\%d\\mapname\\%s"
             "\\maptitle\\Untitled\\gametype\\%s\\numplayers\\%d\\maxplayers\\%d%s",
             name, port - 1, map, game, players, max, more);
    dxl_shell_server_listed(s, address, port);
    if (ping >= 0) dxl_shell_server_answered(s, address, port, ping, status);
}

static void servers(dxl_shell *s, scene sc) {
    dxl_shell_go(s, DXL_PAGE_SERVERS);
    if (sc == SCENE_SERVERS_ASKING) {
        s->browsing = 1;
        s->browsed = 0;
        server(s, "203.0.113.10", 7791, 48, "Wunderland Tactical", "DXMP_Cathedral", "TeamDMGame", 3, 16, "");
        server(s, "203.0.113.11", 7781, -1, "", "", "", 0, 0, "");
        server(s, "203.0.113.12", 7771, -1, "", "", "", 0, 0, "");
        return;
    }
    dxl_profile_set(&s->run->session.profile, "Favourites", "198.51.100.7:7790");
    if (sc == SCENE_SERVERS_FAILED) {
        server(s, "198.51.100.7", 7791, 61, "A Favourite Server", "DXMP_Smuggler", "DeathMatchGame", 1, 8, "");
        dxl_shell_browsed(s, "master.333networks.com:28900: no answer");
        return;
    }
    server(s, "203.0.113.10", 7791, 48, "~[FGS]Wunderland~ DXSG Tactical Server~", "DXSG_Area51Bunker_V3", "SG Team Game",
           3, 10,
           "\\TimeToWin\\15\\player_0\\Cozmo\\frags_0\\15\\ping_0\\ 40\\player_1\\JC Denton\\frags_1\\9\\ping_1\\ 75"
           "\\player_2\\Gunther\\frags_2\\3\\ping_2\\ 130");
    server(s, "198.51.100.7", 7791, 61, "A Favourite Server", "DXMP_Smuggler", "DeathMatchGame", 1, 8, "");
    server(s, "203.0.113.20", 7771, 95, "EZ8 | a Deus Ex community", "DXMP_Area51Bunker", "MTLDeathMatch", 0, 8,
           "\\password\\True");
    server(s, "203.0.113.21", 7791, 120, "Sunny Side", "DXMP_Abandoned", "MTLTeam", 0, 7, "");
    server(s, "203.0.113.22", 17578, 152, "An old one", "DXMP_Silo", "DeathMatchGame", 2, 12,
           "\\gamever\\1000\\minnetver\\1000");
    server(s, "203.0.113.23", 7791, 175, "~[FGS]Wunderland~ DXAG V-Server", "DXAG_Smuggler_V2", "DXAG", 0, 12, "");
    server(s, "203.0.113.24", 7781, 210, "Mixed Deathmatch, every map", "DXMP_DressingRoom", "CDX Zero Aug", 5, 16, "");
    server(s, "203.0.113.25", 7791, 230, "Late Night Hong Kong", "DXMP_Cathedral", "DeathMatchGame", 0, 8, "");
    server(s, "203.0.113.26", 7791, 260, "Paris Streets", "DXMP_Paris", "TeamDMGame", 1, 16, "");
    server(s, "203.0.113.30", 7791, -1, "", "", "", 0, 0, "");
    dxl_shell_browsed(s, NULL);
    if (sc == SCENE_SERVERS_PASSWORD) {
        /* Its row the focus's, the third by ping, once the shot's first
         * move is made: the second's before it. */
        dxl_list l;
        memset(&l, 0, sizeof l);
        l.id = dxl_ui_id(&s->ui, "servers");
        l.count = s->server_count;
        dxl_ui_focus(&s->ui, dxl_list_row_id(&l, 1));
    }
}

/* The scene's run and shell, entered. */
static void set_up(scene sc, dxl_run *r, dxl_shell *s) {
    int profile = sc >= SCENE_HOME && sc != SCENE_REPOINT;
    make_run(r, profile || sc == SCENE_REPOINT);
    engine(s);
    switch (sc) {
    case SCENE_WELCOME:
        dxl_shell_enter(s, r, DXL_VIEW_FIRST_RUN);
        break;
    case SCENE_FIND:
    case SCENE_FIND_NONE:
    case SCENE_FIND_LOOKING:
    case SCENE_FIND_MORE:
    case SCENE_BROWSER:
    case SCENE_MESSAGE:
        dxl_shell_enter(s, r, DXL_VIEW_FIRST_RUN);
        s->looked = 1;
        s->finding = sc == SCENE_FIND_LOOKING || sc == SCENE_FIND_MORE;
        if (sc != SCENE_FIND_NONE && sc != SCENE_FIND_LOOKING) found(s);
        dxl_shell_go(s, DXL_PAGE_FIND);
        if (sc == SCENE_BROWSER) dxl_browser_open(s, home, DXL_BROWSE_GAME);
        if (sc == SCENE_MESSAGE)
            dxl_shell_message(s, "Not one to play", "%s: %s.", s->found.items[1].install.root,
                              s->found.items[1].install.problem);
        break;
    case SCENE_IMPORT: {
        dxl_shell_enter(s, r, DXL_VIEW_FIRST_RUN);
        dxl_install in;
        good_install(&in, ".local/share/Steam/steamapps/common/Deus Ex", 1);
        dxl_shell_choose(s, &in);
        dxl_install_free(&in);
        /* What an install with the game's own inis, twelve saves and three
         * shots would give: none of its files are there to read. */
        s->plan_error[0] = '\0';
        snprintf(s->plan.system_from, sizeof s->plan.system_from, "%s/DeusEx.ini", s->chosen.system_dir);
        snprintf(s->plan.user_from, sizeof s->plan.user_from, "%s/User.ini", s->chosen.system_dir);
        snprintf(s->plan.saves_from, sizeof s->plan.saves_from, "%s/../Save", s->chosen.system_dir);
        s->plan.saves = 12;
        s->plan.shots = 3;
        s->plan.master_replaced = 1;
        break;
    }
    case SCENE_HOME:
    case SCENE_HOME_MADE:
        if (sc == SCENE_HOME_MADE) {
            snprintf(s->note, sizeof s->note,
                     "The profile is made: 12 saves and 3 screenshots copied from the game's folder.");
            r->last.known = 0;
        }
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        break;
    case SCENE_BIG_OFFER:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_offer_big(s);
        break;
    case SCENE_SERVERS:
    case SCENE_SERVERS_ASKING:
    case SCENE_SERVERS_FAILED:
    case SCENE_SERVERS_PASSWORD:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        servers(s, sc);
        break;
    case SCENE_MODS: {
        make_mods(r);
        /* GMDXv10's profile. */
        dxl_profile p;
        dxl_err e;
        char *dir = dxl_path_join(r->session.install.root, "GMDXv10"), *real = realpath(dir, NULL);
        if (dxl_profile_open(&p, &r->session.xdg, "gmdxv10", &e) == 0) {
            dxl_profile_make_dirs(&p, &e);
            dxl_profile_set(&p, "Name", "GMDX v10");
            dxl_profile_set(&p, "Install", "1112fm-00000000");
            dxl_profile_set(&p, "Mod", real ? real : dir);
            dxl_profile_save(&p, &e);
            write_file(p.system_ini, "[Engine.Engine]\n");
            dxl_profile_free(&p);
        }
        free(real);
        free(dir);
    }
        /* fall through */
    case SCENE_MODS_NONE:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_MODS);
        break;
    case SCENE_MOD_PROFILE: {
        make_mods(r);
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        char *dir = dxl_path_join(r->session.install.root, "GMDXv10");
        if (dxl_mod_check(dir, &s->making_mod, NULL) == 0) {
            s->making = 1;
            snprintf(s->making_before, sizeof s->making_before, "%s", r->session.profile.id);
            dxl_install in;
            good_install(&in, "GOG Games/Deus Ex GOTY", 0);
            if (s->has_chosen) dxl_install_free(&s->chosen);
            s->chosen = in;
            s->has_chosen = 1;
            snprintf(s->begin_name, sizeof s->begin_name, "GMDXv10");
            /* What a mod with its own inis and a save of its own gives. */
            memset(&s->plan, 0, sizeof s->plan);
            s->plan_error[0] = '\0';
            snprintf(s->plan.system_from, sizeof s->plan.system_from, "%s", s->making_mod.ini);
            snprintf(s->plan.user_from, sizeof s->plan.user_from, "%s", s->making_mod.user_ini);
            s->plan.saves = 1;
            s->plan.master_replaced = 1;
            dxl_shell_go(s, DXL_PAGE_IMPORT);
        }
        free(dir);
        break;
    }
    case SCENE_HOME_NEW:
    case SCENE_SAVES_NONE: {
        char cmd[1200];
        snprintf(cmd, sizeof cmd, "rm -rf '%s'/*", r->session.profile.save_dir);
        if (system(cmd) != 0) fprintf(stderr, "dxl-shots: could not empty the saves\n");
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        if (sc == SCENE_SAVES_NONE) dxl_shell_go(s, DXL_PAGE_SAVES);
        break;
    }
    case SCENE_SAVES_OLD:
        snprintf(s->engine_features, sizeof s->engine_features, "userdir");
        /* fall through */
    case SCENE_SAVES:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SAVES);
        break;
    case SCENE_SHOTS:
    case SCENE_SHOT_WHOLE:
        make_shots(r->session.profile.shots_dir);
        /* fall through */
    case SCENE_SHOTS_NONE:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SHOTS);
        if (sc == SCENE_SHOT_WHOLE) s->shot_open = 1;
        break;
    case SCENE_HOME_ENGINE:
        s->engine_ok = 0;
        snprintf(s->engine_brief, sizeof s->engine_brief, "needs glibc 2.44, this system has 2.43");
        snprintf(s->engine_problem, sizeof s->engine_problem,
                 "%.200s/Applications/DXLauncher/SurrealEngine does not start here: it needs glibc 2.44, and this system "
                 "has 2.43. It was built on a newer system than this one: choose an engine built for this one, or "
                 "build it here.", home);
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        break;
    case SCENE_THEME_PASTEL:
    case SCENE_THEME_RED: {
        /* Two of DeusEx.u's ColorThemeMenu_* colours, as the install has
         * them: no file of the game's is read here. */
        static const dxl_rgba pastel[DXL_MENU_COLORS] = {
            { 207, 255, 212, 255 }, { 255, 196, 255, 255 }, { 250, 250, 131, 255 }, { 204, 223, 255, 255 },
            { 244, 255, 161, 255 }, { 255, 255, 255, 255 }, { 100, 100, 100, 255 }, { 175, 134, 196, 255 },
            { 175, 113, 175, 255 }, { 255, 255, 145, 255 }, { 100, 100, 124, 255 }, { 150, 80, 150, 255 },
            { 255, 188, 255, 255 },
        };
        static const dxl_rgba red[DXL_MENU_COLORS] = {
            { 255, 0, 0, 255 }, { 135, 0, 0, 255 }, { 255, 255, 255, 255 }, { 255, 27, 0, 255 },
            { 255, 107, 94, 255 }, { 255, 191, 186, 255 }, { 169, 8, 3, 255 }, { 200, 200, 200, 255 },
            { 255, 0, 0, 255 }, { 0, 0, 0, 255 }, { 255, 191, 186, 255 }, { 255, 0, 0, 255 }, { 255, 0, 0, 255 },
        };
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_theme t;
        dxl_theme_from_menu(&t, sc == SCENE_THEME_RED ? "Red" : "Pastel", sc == SCENE_THEME_RED ? red : pastel);
        dxl_ui_set_theme(&s->ui, &t);
        if (sc == SCENE_THEME_RED) dxl_shell_go(s, DXL_PAGE_SETTINGS);
        break;
    }
    case SCENE_REPOINT: {
        dxl_install_free(&r->session.install);
        char *gone = at_home("Games/Deus Ex");
        dxl_install_check(gone, &r->session.install);
        free(gone);
        dxl_shell_enter(s, r, DXL_VIEW_INSTALL);
        found(s);
        break;
    }
    case SCENE_RECOVERY:
        crashed(r);
        dxl_shell_enter(s, r, DXL_VIEW_RECOVERY);
        break;
    case SCENE_RECOVERY_SAVE: {
        r->last.clean = r->last.ready = 0;
        r->last.status = 1;
        snprintf(r->last.error, sizeof r->last.error, "Expected an import table entry: 01_NYC_UNATCOIsland");
        snprintf(r->last.start, sizeof r->last.start, "?loadgame=10");
        char *log = dxl_path_join(r->session.profile.logs_dir, DXL_LASTRUN_LOG);
        write_file(log, "[] Game folder: ~/GOG Games/Deus Ex GOTY\n[] Renderer: Vulkan\n"
                        "[GameInfo.InitGame] InitGame: ?Name=Player?Class=DeusEx.JCDentonMale\n"
                        "SurrealEngine error: Expected an import table entry: 01_NYC_UNATCOIsland\n");
        free(log);
        dxl_shell_enter(s, r, DXL_VIEW_RECOVERY);
        break;
    }
    case SCENE_SAFE:
        r->decision.screen = DXL_SCREEN_MAIN_SAFE;
        dxl_shell_enter(s, r, DXL_VIEW_RECOVERY);
        break;
    case SCENE_ABOUT:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_ABOUT);
        break;
    case SCENE_PROFILES: {
        /* Two more, made up: a mod's, and one whose install has gone. */
        static const char *const others[][2] = { { "gmdx", "GMDX" }, { "old-copy", "The old copy" } };
        for (int i = 0; i < 2; i++) {
            dxl_profile p;
            dxl_err e;
            if (dxl_profile_open(&p, &r->session.xdg, others[i][0], &e) != 0) continue;
            dxl_profile_make_dirs(&p, &e);
            dxl_profile_set(&p, "Name", others[i][1]);
            dxl_profile_set(&p, "Install", i ? "1112fm-11111111" : "1112fm-00000000");
            dxl_profile_save(&p, &e);
            write_file(p.system_ini, "[Engine.Engine]\n");
            dxl_profile_free(&p);
        }
        /* The install they have, by launcher.ini's list. */
        dxl_ini_set(r->session.ini, "Install.1112fm-00000000", "Path", r->session.install.root);
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_PROFILES);
        break;
    }
    case SCENE_SETTINGS:
    case SCENE_PICK:
    case SCENE_OSK:
    case SCENE_CONFIRM: {
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SETTINGS);
        if (sc == SCENE_SETTINGS) break;
        static const char *labels[16];
        const dxl_setting *row = NULL;
        for (size_t i = 0; i < dxl_settings_count() && !row; i++)
            if (dxl_setting_at(i)->kind == DXL_SET_CHOICE) row = dxl_setting_at(i);
        int n = 0;
        for (; row && row->choices[n].value && n < 16; n++) labels[n] = row->choices[n].label;
        for (size_t i = 0; i < dxl_settings_page_count(); i++)
            if (row && !strcmp(dxl_settings_page(i), row->page)) s->settings_tab = (int)i;
        if (sc == SCENE_PICK && row) dxl_shell_pick(s, 1, row->label, labels, n, 1);
        if (sc == SCENE_OSK && s->text_count) {
            snprintf(s->texts[0], sizeof s->texts[0], "-nosound");
            dxl_shell_osk(s, 2, s->text_rows[0]->label, s->texts[0], sizeof s->texts[0]);
        }
        if (sc == SCENE_CONFIRM)
            dxl_shell_confirm(s, 3, "Display", "Every setting on this page back to its default?", "Reset", "Keep them");
        break;
    }
    case SCENE_KEYS:
    case SCENE_CONTROLLER:
    case SCENE_CONTROLLER_END:
    case SCENE_GRAPHICS:
    case SCENE_AUDIO:
    case SCENE_GAMEPLAY:
    case SCENE_MULTIPLAYER:
    case SCENE_LAUNCHER:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SETTINGS);
        for (size_t p = 0; p < sizeof page_scenes / sizeof *page_scenes; p++)
            for (size_t i = 0; page_scenes[p].sc == sc && i < dxl_settings_page_count(); i++)
                if (!strcmp(dxl_settings_page(i), page_scenes[p].page)) s->settings_tab = (int)i;
        /* The default layout on the pad, as a player who chose it has it. */
        if ((sc == SCENE_CONTROLLER || sc == SCENE_CONTROLLER_END) && s->stores.user)
            dxl_bindings_apply(s->stores.user, dxl_pad_preset_default());
        break;
    case SCENE_PADWIZARD:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SETTINGS);
        for (size_t i = 0; i < dxl_settings_page_count(); i++)
            if (!strcmp(dxl_settings_page(i), "Controller")) s->settings_tab = (int)i;
        dxl_page_settings_enter(s);
        if (raw_id) dxl_padwizard_open(s, raw_id);
        break;
    case SCENE_PADTEST:
        dxl_shell_enter(s, r, DXL_VIEW_HOME);
        dxl_shell_go(s, DXL_PAGE_SETTINGS);
        for (size_t i = 0; i < dxl_settings_page_count(); i++)
            if (!strcmp(dxl_settings_page(i), "Controller")) s->settings_tab = (int)i;
        dxl_page_settings_enter(s);
        if (s->stores.user) dxl_bindings_apply(s->stores.user, dxl_pad_preset_default());
        dxl_padtest_open(s, pad_ids[0]);
        break;
    case SCENE_LOG:
        crashed(r);
        dxl_shell_enter(s, r, DXL_VIEW_RECOVERY);
        dxl_shell_go(s, DXL_PAGE_LOG);
        break;
    default:
        break;
    }
}

static void tear_down(dxl_run *r) {
    dxl_config_free(r->config);
    r->config = NULL;
    dxl_session_close(&r->session);
    /* The next scene's profile from nothing. */
    char *cfg = at_home(".config"), *data = at_home(".local/share/dxlauncher"), *state = at_home(".local/state");
    char *mods = at_home("GOG Games/Deus Ex GOTY");
    char cmd[2600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s' '%s' '%s' '%s'/GMDXv10 '%s'/TNM", cfg, data, state, mods, mods);
    free(mods);
    if (system(cmd) != 0) fprintf(stderr, "dxl-shots: could not clear the profile\n");
    free(cfg);
    free(data);
    free(state);
}

/* ---- the frames -------------------------------------------------------- */

static void frame(dxl_shell *s, double now) {
    SDL_SetRenderDrawColor(s->ui.renderer, 0, 0, 0, 255);
    SDL_RenderClear(s->ui.renderer);
    dxl_shell_frame(s, now);
    SDL_RenderPresent(s->ui.renderer);
}

static void events(dxl_shell *s) {
    SDL_UpdateJoysticks();
    SDL_Event e;
    while (SDL_PollEvent(&e)) dxl_ui_event(&s->ui, &e);
}

/* The device's first move: the focus to the next, as a player would. */
static void act(dxl_shell *s, mode m) {
    dxl_ui *ui = &s->ui;
    SDL_Event e;
    SDL_zero(e);
    if (m == MODE_KEYBOARD) {
        e.type = SDL_EVENT_KEY_DOWN;
        e.key.key = SDLK_DOWN;
        dxl_ui_event(ui, &e);
    } else if (m == MODE_MOUSE) {
        /* The pointer on the second focusable the last frame drew. */
        int top = dxl_focus_top_scope(&ui->cur), seen = 0;
        for (int i = 0; i < ui->cur.count; i++) {
            const dxl_focusable *f = &ui->cur.items[i];
            if (f->scope != top || seen++ != 1) continue;
            e.type = SDL_EVENT_MOUSE_MOTION;
            e.motion.x = (f->box.x + f->box.w / 2) * ui->scale;
            e.motion.y = (f->box.y + f->box.h / 2) * ui->scale;
            dxl_ui_event(ui, &e);
            break;
        }
    } else {
        SDL_Joystick *j = SDL_GetJoystickFromID(pad_ids[m - MODE_XBOX]);
        if (!j) return;
        SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
        events(s);
        SDL_SetJoystickVirtualButton(j, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
        events(s);
    }
}

static int wanted(scene sc, int si, mode m) {
    if (all) return 1;
    if (si == 0 && m == MODE_KEYBOARD) return 1;
    if (sc == SCENE_HOME && (m == MODE_KEYBOARD || si == 0)) return 1;
    if (sc == SCENE_FIND && si == 5 && m == MODE_XBOX) return 1;
    return 0;
}

/* The kit for a size, its fonts open: made once, reset for each shot. */
static SDL_Surface *surface;
static SDL_Renderer *renderer;
static dxl_ui kit;

static int kit_for(int si) {
    static int now = -1;
    if (now == si) return 0;
    if (now >= 0) {
        dxl_ui_free(&kit);
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    }
    now = -1;
    const size *z = &sizes[si];
    surface = SDL_CreateSurface(z->w, z->h, SDL_PIXELFORMAT_XRGB8888);
    renderer = surface ? SDL_CreateSoftwareRenderer(surface) : NULL;
    if (!renderer || dxl_ui_init(&kit, NULL, renderer, fonts_dir, z->scale) != 0) {
        fprintf(stderr, "dxl-shots: %dx%d: %s\n", z->w, z->h, SDL_GetError());
        return -1;
    }
    kit.big = z->big;
    now = si;
    return 0;
}

static void shoot(scene sc, int si, mode m) {
    const size *z = &sizes[si];
    char name[128];
    if (z->big)
        snprintf(name, sizeof name, "%s-%dx%dbig-%s", scene_names[sc], z->w, z->h, mode_names[m]);
    else if (z->scale != 1)
        snprintf(name, sizeof name, "%s-%dx%d@%.1f-%s", scene_names[sc], z->w, z->h, z->scale, mode_names[m]);
    else
        snprintf(name, sizeof name, "%s-%dx%d-%s", scene_names[sc], z->w, z->h, mode_names[m]);
    if (only && !strstr(name, only)) return;

    if (kit_for(si) != 0) exit(1);
    static dxl_shell s;
    dxl_shell_init(&s, "shots");
    dxl_ui_reset(&kit);
    s.ui = kit;
    s.big = kit.big;
    dxl_run r;
    set_up(sc, &r, &s);
    /* The Controller page shows the pad the shot's mode uses; the test
     * tries it, its north button (no pad's back) and right trigger held. */
    if (m >= MODE_XBOX) s.pad_id = pad_ids[m - MODE_XBOX];
    SDL_Joystick *tried = NULL;
    if (sc == SCENE_PADTEST) {
        if (m >= MODE_XBOX) s.pad_test.id = pad_ids[m - MODE_XBOX];
        tried = SDL_GetJoystickFromID(s.pad_test.id);
        if (tried) {
            SDL_SetJoystickVirtualButton(tried, SDL_GAMEPAD_BUTTON_NORTH, true);
            SDL_SetJoystickVirtualAxis(tried, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
        }
    }
    s.page_since = 0;   /* the page up long since: no slide in the frames below */
    double t = 100;
    events(&s);
    frame(&s, t);
    /* The thumbnails the first frame asked for, all read. */
    if (s.thumbs || s.shot_whole) {
        dxl_thumbs_wait(s.thumbs, s.ui.renderer);
        dxl_thumbs_wait(s.shot_whole, s.ui.renderer);
        frame(&s, t += 0.05);
        dxl_thumbs_wait(s.thumbs, s.ui.renderer);
        dxl_thumbs_wait(s.shot_whole, s.ui.renderer);
    }
    /* A list's end in view: the focus on its first row, paged to its last. */
    if (sc == SCENE_CONTROLLER_END) {
        uint32_t first = dxl_ui_named(&s.ui, "Layout");
        if (first) dxl_ui_focus(&s.ui, first);
        dxl_ui_input_begin(&s.ui);
        frame(&s, t += 0.05);
        dxl_ui_input_begin(&s.ui);
        s.ui.in.page_down = 10;
        frame(&s, t += 0.05);
    }
    dxl_ui_input_begin(&s.ui);
    act(&s, m);
    frame(&s, t += 0.05);
    /* And those the move brought into view. */
    if (s.thumbs || s.shot_whole) {
        dxl_thumbs_wait(s.thumbs, s.ui.renderer);
        dxl_thumbs_wait(s.shot_whole, s.ui.renderer);
    }
    /* Every animation run out. */
    dxl_ui_input_begin(&s.ui);
    frame(&s, t += 2);

    shots++;
    if (s.ui.diag_count) {
        failures++;
        fprintf(stderr, "%s: %d diagnostics\n", name, s.ui.diag_count);
        for (int i = 0; i < s.ui.diag_count && i < 8; i++) fprintf(stderr, "    %s\n", s.ui.diag[i]);
    }
    if (out_dir && wanted(sc, si, m)) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.png", out_dir, name);
        if (SDL_SavePNG(surface, path)) saved++;
        else fprintf(stderr, "dxl-shots: %s: %s\n", path, SDL_GetError());
    }
    if (tried) {
        SDL_SetJoystickVirtualButton(tried, SDL_GAMEPAD_BUTTON_NORTH, false);
        SDL_SetJoystickVirtualAxis(tried, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
        events(&s);
    }
    /* The kit goes on to the next shot, its texts and textures with it, in
     * the Default's colours. */
    s.ui.pads_off = 0;
    if (strcmp(s.ui.theme.name, "Default") != 0) {
        dxl_theme def;
        dxl_theme_default(&def);
        dxl_ui_set_theme(&s.ui, &def);
    }
    kit = s.ui;
    if (s.settings_open) {
        dxl_es_free(s.engine_settings);
        s.settings_open = 0;
    }
    dxl_shell_free(&s);
    tear_down(&r);
}

static void attach_pads(void) {
    for (int i = 0; i < 4; i++) {
        SDL_VirtualJoystickDesc d;
        SDL_INIT_INTERFACE(&d);
        d.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        d.vendor_id = pads[i].vendor;
        d.product_id = pads[i].product;
        d.naxes = SDL_GAMEPAD_AXIS_COUNT;
        d.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        d.name = pads[i].name;
        pad_ids[i] = SDL_AttachVirtualJoystick(&d);
        /* Held open here: the kit's own let go with each shot's. */
        if (pad_ids[i]) SDL_OpenGamepad(pad_ids[i]);
        else fprintf(stderr, "dxl-shots: no virtual %s: %s\n", pads[i].name, SDL_GetError());
    }
    SDL_VirtualJoystickDesc d;
    SDL_INIT_INTERFACE(&d);
    d.type = SDL_JOYSTICK_TYPE_UNKNOWN;
    d.vendor_id = 0x1209;
    d.product_id = 0xd0d1;
    d.naxes = 6;
    d.nbuttons = 13;
    d.nhats = 1;
    d.name = "A pad of no maker";
    raw_id = SDL_AttachVirtualJoystick(&d);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--out=", 6) == 0) out_dir = argv[i] + 6;
        else if (strcmp(argv[i], "--all") == 0) all = 1;
        else if (strncmp(argv[i], "--only=", 7) == 0) only = argv[i] + 7;
        else if (strncmp(argv[i], "--fonts=", 8) == 0) fonts_dir = argv[i] + 8;
        else {
            fprintf(stderr, "usage: dxl-shots [--out=<dir>] [--all] [--only=<shot>] [--fonts=<dir>]\n");
            return 2;
        }
    }
    if (out_dir && dxl_path_mkdirs(out_dir) != 0) {
        fprintf(stderr, "dxl-shots: cannot make %s\n", out_dir);
        return 1;
    }
    const char *tmp = getenv("TMPDIR");
    snprintf(home, sizeof home, "%s/dxl-shots-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(home)) {
        perror("dxl-shots: mkdtemp");
        return 1;
    }
    setenv("HOME", home, 1);
    setenv("TZ", "UTC", 1);
    static const char *unset[] = { "DXL_HOME", "XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_STATE_HOME", "XDG_CACHE_HOME",
                                   "DXL_ENGINE" };
    for (size_t i = 0; i < sizeof unset / sizeof *unset; i++) unsetenv(unset[i]);
    dxl_log_set_echo(0);
    dxl_log_set_to_file(0);
    make_home();

    /* No real pad in the way of the virtual ones: SDL's controllers of
     * their makes and models alone. */
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,
                "0x045e/0x0b12,0x054c/0x0ce6,0x057e/0x2009,0x1234/0x5678,0x1209/0xd0d1");
    if (!SDL_Init(SDL_INIT_GAMEPAD) || !TTF_Init()) {
        fprintf(stderr, "dxl-shots: %s\n", SDL_GetError());
        return 1;
    }
    attach_pads();

    Uint64 t0 = SDL_GetTicks();
    for (int sc = 0; sc < SCENE_COUNT; sc++)
        for (int si = 0; si < (int)(sizeof sizes / sizeof *sizes); si++)
            for (int m = 0; m < MODE_COUNT; m++) shoot((scene)sc, si, (mode)m);
    printf("dxl-shots: %d shots, %d with diagnostics, %d written%s%s, %.1f s\n", shots, failures, saved,
           out_dir ? " to " : "", out_dir ? out_dir : "", (SDL_GetTicks() - t0) / 1000.0);

    if (renderer) {
        dxl_ui_free(&kit);
        SDL_DestroyRenderer(renderer);
        SDL_DestroySurface(surface);
    }
    TTF_Quit();
    SDL_Quit();
    char cmd[700];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", home);
    if (system(cmd) != 0) fprintf(stderr, "dxl-shots: could not remove %s\n", home);
    return failures ? 1 : 0;
}
