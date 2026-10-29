/* What the player sees of one start (app/run.h): the pages and dialogs of
 * the launcher's window, drawn frame by frame with the kit (ui/kit.h).
 *
 * run.c asks for a screen -- Home, the first run, Recovery, Graphics --
 * through dxl_run_ui; the shell shows it as pages until the player plays
 * or quits:
 *
 *   Home        Continue (the latest save), Play, Saves, Screenshots,
 *               Settings, Profiles, About, Quit; the profile, its game, the
 *               engine, its latest save, the last run
 *   Welcome     the first run's first page
 *   The game    the installs the finder found (core/finder.h), and the
 *               folder browser; also for a profile whose install is not
 *               there any more, which is pointed at the one chosen
 *   Your profile   what the profile will be made from (core/profile.h's
 *               import, shown before it runs), and Make the profile
 *   Recovery    how the last run ended, the end of its log, Play again
 *   Profiles    every profile, its game, saves and last run: use one, make
 *               a new one (The game, Your profile), duplicate, rename,
 *               delete to the trash
 *   Saves       the profile's saves (core/saves.h), newest first, the one
 *               chosen with its snapshot: load it, delete it to the trash
 *   Screenshots the profile's shots (core/shotlist.h) as a gallery of
 *               thumbnails (ui/thumbs.h): one shown whole, its folder
 *               opened, one deleted to the trash
 *   Multiplayer the servers the profile's master server lists, and its
 *               favourites, each as it answers (screens/servers.c): join
 *               one, or an address
 *   About       the launcher, the engine, the folders, the app menu's entry
 *   Settings    the settings (core/settings.h), a tab a page, kept as the
 *               page is left
 *   The log     the last run's output, the run's before it, the engine's own
 *               log
 *
 * Graphics (-changevideo) opens Settings on its Graphics tab.
 *
 * Every page draws itself from the run's state and the shell's own, in
 * dxl_shell_frame; the window around it is window.h's, or dxl-shots' (a
 * software renderer on a surface, and a clock of its own).
 */
#ifndef DXL_SHELL_H
#define DXL_SHELL_H

#include "app/browser.h"
#include "app/run.h"
#include "core/finder.h"
#include "core/servers.h"
#include "core/renderers.h"
#include "core/padmap.h"
#include "core/saves.h"
#include "core/shotlist.h"
#include "core/settings.h"
#include "core/theme_colors.h"
#include "ui/art.h"
#include "ui/audio.h"
#include "ui/glyphs.h"
#include "ui/kit.h"
#include "ui/osk.h"
#include "ui/padmesh.h"
#include "ui/thumbs.h"

typedef enum {
    DXL_PAGE_HOME,
    DXL_PAGE_WELCOME,
    DXL_PAGE_FIND,
    DXL_PAGE_IMPORT,
    DXL_PAGE_RECOVERY,
    DXL_PAGE_ABOUT,
    DXL_PAGE_SETTINGS,
    DXL_PAGE_LOG,
    DXL_PAGE_PROFILES,
    DXL_PAGE_SAVES,
    DXL_PAGE_SHOTS,
    DXL_PAGE_SERVERS,
    DXL_PAGE_MODS,
    DXL_PAGE_COUNT
} dxl_page;

/* The most profiles the Profiles page lists. */
#define DXL_PROFILES_MAX 32

/* No save's snapshot: the slot of none (a save's is -1, the quick save, or
 * more). */
#define DXL_SNAP_NONE (-2)

/* The most choices a pick dialog lists: the actions a pad's button takes. */
#define DXL_PICK_MAX 96

typedef enum {
    DXL_DIALOG_NONE,
    DXL_DIALOG_BROWSER,
    DXL_DIALOG_MESSAGE,
    DXL_DIALOG_PICK,      /* one of a setting's choices */
    DXL_DIALOG_OSK,       /* the on-screen keyboard */
    DXL_DIALOG_CONFIRM,   /* a question, two answers */
    DXL_DIALOG_PADWIZARD, /* a controller's mapping made (screens/padwizard.c) */
    DXL_DIALOG_PADTEST    /* a controller tried, its input the model's alone (screens/controller.c) */
} dxl_dialog;

/* The mapping wizard's state: the controller, the step, where it rests,
 * the mapping so far. */
typedef struct {
    int            open;
    SDL_JoystickID id;
    SDL_Joystick  *joy;
    int            step;
    int            learnt;        /* where it rests known */
    int            armed;         /* at rest since the last control taken: the next may come */
    int            primed;        /* armed once: its rest is known for good */
    double         opened;        /* when, by the window's clock */
    double         still_since;   /* before the first step: since when it has not moved */
    Sint16         still_axis[16];
    Uint8          still_hat[4];
    int            nbuttons, naxes, nhats;
    Sint16         axis_rest[16];
    Uint8          hat_rest[4];
    dxl_padmap     map;
    dxl_padmesh    mesh;
} dxl_padwizard_state;

/* What a folder is chosen for, by the browser or the system's dialog. */
#define DXL_BROWSE_GAME   0   /* an install: the first run's, or the profile's from now on */
#define DXL_BROWSE_ENGINE 1   /* the engine's: DXEngine in it */
#define DXL_BROWSE_MOD    2   /* a mod's: a folder with a System of its own, of packages (core/mods.h) */

typedef struct {
    char  *dir;          /* the folder shown */
    char **names;        /* the folders in it, sorted */
    int    count;
    int    hidden;       /* those whose names start with a dot listed too */
    int    mode;         /* DXL_BROWSE_* */
    char   note[400];    /* why the folder last tried is not what is wanted */
} dxl_browser;

#define DXL_TOASTS 4

typedef struct dxl_shell {
    dxl_ui       ui;
    dxl_run     *run;
    dxl_view     view;
    int          result;          /* -1 while the screen is up; 0 quit; 1 play */
    const char  *version;         /* the launcher's */

    dxl_page     page, from;      /* the page, and the one before it */
    double       page_since;      /* when it came up; -1 until the next frame */
    /* A page asked for while one draws: taken as it is drawn, so what is
     * left of it draws on its own state. */
    int          in_page;
    int          go_pending;
    dxl_page     go_to;
    uint32_t     focus[DXL_PAGE_COUNT];   /* each page's focus, for coming back to it */

    /* The engine's --version, asked once: what is wrong in a few words, and
     * the whole of it. */
    int          engine_asked;
    int          engine_ok;
    char         engine_version[160], engine_features[256], engine_brief[160], engine_problem[700];

    /* The first run, and a profile's install found again. */
    int          repoint;         /* The game is for the profile's install, not a first run */
    int          repoint_back;    /* ...asked for from a page, which its Back goes to: */
    dxl_page     repoint_to;
    int          pick_for;        /* what the system's folder dialog is for: DXL_BROWSE_* */
    /* The system's folder dialog up: from which page, in which state; its
     * answer is dropped once any of them has changed. */
    int          pick_open;
    dxl_page     pick_page;
    int          pick_repoint, pick_making;
    int          looked;          /* the finder has been asked */
    int          finding;         /* ...and still looks: the list grows as it finds */
    int          find_focus;      /* the first found to take the page's focus as it comes */
    dxl_found_list found;
    dxl_install  chosen;          /* the install the profile is to be made from */
    int          has_chosen;
    dxl_import   plan;            /* what making it will do */
    char         plan_error[320];
    /* How it begins: a graphics preset's index + 1, 0 the engine's defaults;
     * with a pad connected, a layout's + 1, 0 the controls as imported. Its
     * name, as Your profile has it. */
    int          begin_graphics, begin_layout;
    char         begin_name[96];
    /* A profile made from the Profiles page, not a first run: the active
     * profile before it, gone back to if it is not made; the mod it is
     * made of, from the Mods page (dir NULL for none). */
    int          making;
    char         making_before[DXL_PROFILE_ID_MAX + 1];
    dxl_mod      making_mod;

    /* The Mods page: the mods found as it opened and those shown, what
     * each row says, the profile that plays each, the one chosen; the mods
     * shown while the launcher runs. */
    dxl_mod_list mods, mods_shown;
    struct {
        char detail[600];
        char profile[DXL_PROFILE_ID_MAX + 1];   /* the first that plays it; "" for none */
        char profile_name[96];
        int  profiles;
    } mod_rows[DXL_MODS_MAX];
    int          mod_sel;
    /* The profile in use's mod, as Home opened: 0 none, 1 there, -1 gone;
     * what Home says of it. */
    int          profile_mod;
    char         profile_mod_text[600];

    /* The Profiles page: each profile as the page opened, the one chosen,
     * its name as it is edited. */
    struct {
        char id[DXL_PROFILE_ID_MAX + 1];
        char name[96];
        char detail[300];
        int  active, ok;
    } profiles[DXL_PROFILES_MAX];
    int          profile_count, profile_sel;
    char         profile_name[96];

    /* Home and Recovery. */
    int          saves;           /* the profile's saves, counted as Home opens */
    int          unclean;         /* its Running.ini there as Home opens: the last run did not end cleanly */
    int          played;          /* that, or a run's log there: the profile has played */
    /* Home's and the Saves page's: the profile's saves, read as either
     * opens, the one chosen; its snapshot, a texture of the window's
     * renderer, of the slot in save_snap_slot (DXL_SNAP_NONE for none). */
    dxl_saves    save_list;
    int          save_sel;
    SDL_Texture *save_snap;
    int          save_snap_slot;
    int          save_snap_read;  /* that slot's SaveInfo read for it: none to read again */
    /* The Screenshots page: the profile's shots, read as it opens, the one
     * chosen, shown whole or not; their thumbnails and the one shown, read
     * off the window's thread. */
    dxl_shots    shot_list;
    int          shot_sel, shot_open;
    int          shot_refocus;    /* the chosen's tile to take the focus, as the one shown closes */
    dxl_thumbs  *thumbs, *shot_whole;
    int          thumbs_pending;  /* some still being read: frames kept coming */
    char         note[400];       /* a line under Home's panel: what the first run did */
    char        *log_tail;        /* Recovery's: the end of the last run's log */

    /* Dialogs. */
    dxl_dialog   dialog;
    uint32_t     dialog_return;   /* the focus it took, given back as it closes */
    double       dialog_since;    /* when it came up: its message types itself in from then */
    dxl_browser  browser;
    char         message_title[96];
    char         message[800];
    /* The asking dialogs: whose control asked, and the answer, once. */
    uint32_t     asked_for;
    int          answer;          /* the choice, 1 kept or yes, 0 no; -1 none yet */
    int          answered;
    const char  *pick_labels[DXL_PICK_MAX];  /* into pick_text: the dialog's own copy */
    char         pick_text[DXL_PICK_MAX][96];
    char         pick_detail[DXL_PICK_MAX][96];   /* each one's group, or a word on it; "" for none */
    int          pick_count, pick_current;
    char         ask_title[96], ask_yes[48], ask_no[48], ask_third[48];
    char        *osk_buf;
    size_t       osk_size;
    dxl_osk_state osk;

    /* Words for the next run alone (Recovery's, Settings' next-run rows):
     * added to the original's command line as Play is chosen. */
    char         next_run[512];

    /* Settings, while the page is up: its stores, open, and the text rows'
     * values as they are edited. */
    int          settings_tab;
    int          settings_plays;   /* Settings as -changevideo's Graphics opened it: its Done plays */
    int          settings_open;
    dxl_stores   stores;
    dxl_engine_settings *engine_settings;
    const dxl_setting *text_rows[8];
    char         texts[8][256];
    int          text_count;
    /* The Controller tab: the controller shown (its SDL id), its model,
     * the view's turn and the pointer's drag, a control to find, the one
     * chosen; the wizard. */
    SDL_JoystickID pad_id;
    dxl_padmesh  pad_mesh;
    int          pad_mesh_family;     /* -1 for none built */
    float        pad_yaw, pad_pitch, pad_drag_x, pad_drag_y;
    int          pad_find, pad_chosen;
    char        *pad_db;              /* gamecontrollerdb.txt's text as last read; NULL, to read */
    dxl_padwizard_state wizard;
    /* The controller's test: the one tried, since when its back button is
     * held (-1 not held), whether its triggers rumble it. */
    struct {
        SDL_JoystickID id;
        double         held_since;
        int            rumbling;
    } pad_test;
    /* A key captured for an action, waiting on the answer to whose it is. */
    int          bind_action, bind_slot;
    char         bind_key[32];

    /* The log page: which log, read as it opened, its lines, the first
     * shown. */
    int          log_which;
    char        *log_text;
    long        *log_starts;
    int          log_lines;
    int          log_top;

    /* The install's menu themes, read from its DeusEx.u once (themes_from
     * the System folder they are of): the window wears the player's. */
    dxl_menu_theme *themes;
    int          theme_count;
    char        *themes_from;
    /* The install's art the window wears (ui/art.h): its renderer's
     * textures, let go with the window. */
    dxl_art      art;
    /* The install's title music and menu sounds, read once for it
     * (sounds_from its folder), and the window's sound that plays them
     * (ui/audio.h; NULL offscreen or with no output), given them once. */
    uint8_t     *music;
    size_t       music_len;
    uint8_t     *sounds[DXL_SOUND_COUNT];
    size_t       sound_len[DXL_SOUND_COUNT];
    char        *sounds_from;
    dxl_audio   *audio;
    int          audio_given;

    /* The GPU this machine has, as dxl-cli --probe --ini answers in the
     * window's worker (start_probe): 0 not asked, 1 asked, 2 known; and
     * renderers.ini's list, resolved by it. */
    int          probe_state;
    int          make_waits;     /* Make the profile pressed before the probe answered: made once it has, on its page */
    dxl_gpu_probe probe;
    dxl_renderer_list renderers;
    void       (*start_probe)(struct dxl_shell *s);
    /* The window's: the finder in its worker, each install it finds handed
     * in with dxl_shell_found, its end with dxl_shell_found_all. NULL
     * offscreen, where it looks at once. */
    void       (*start_find)(struct dxl_shell *s, const dxl_finder_where *where);
    /* The window's: the server browser's look in its worker, each server
     * handed in with dxl_shell_server_listed and _answered, its end with
     * dxl_shell_browsed. NULL offscreen, where nothing is asked. */
    void       (*start_browse)(struct dxl_shell *s, const char *master, const char *const *also,
                               const dxl_browse_lan *lan);

    struct { char text[200]; double at; } toasts[DXL_TOASTS];
    int          toast_count;

    /* Multiplayer: the servers the master lists and the favourites, each
     * as it answers, in the order they came until the look ends, then by
     * the page's sort; the one chosen, by its address; those the filter
     * shows; what the page's fields hold, the password for the server it
     * was typed for; a deusex:// link's server waiting for the page, and
     * the one it asks about. */
    dxl_server  *servers;          /* DXL_SERVERS_MAX of them, made as the page first opens */
    int          server_count, servers_shown;
    char         server_chosen[96];   /* "a.b.c.d:query port" */
    int          browsing, browsed, servers_sorted;
    int          rebrowse;   /* the profile changed under the look: asked again as the page comes */
    char         browse_error[300];
    int          server_sort, server_show;
    char         join_address[96], join_password[64], join_password_for[96];
    char         join_link[256], link_asked[256];

    /* The big screen (core/bigscreen.h): the window's now, and whether it
     * has been offered in this run. */
    int          big;
    int          big_offered;
    /* The window's: full screen or not, s->big and the kit's with it. NULL
     * offscreen, where the two are set alone. */
    void       (*set_big)(struct dxl_shell *s, int on);

    /* The window's: the system's folder dialog, and the app menu's entry.
     * NULL offscreen, where the buttons do nothing. */
    void       (*pick_folder)(struct dxl_shell *s);
    void       (*desktop)(struct dxl_shell *s, int install);
    int          desktop_entry;   /* the app menu has the launcher's entry */
    void        *window;
} dxl_shell;

void dxl_shell_init(dxl_shell *s, const char *version);
void dxl_shell_free(dxl_shell *s);
/* A screen run.c asks for: its first page. The kit must be up. */
void dxl_shell_enter(dxl_shell *s, dxl_run *r, dxl_view v);
/* One frame, at now (seconds): the kit's input since the last acted on,
 * every page and dialog drawn, s->result set when the screen is done. The
 * caller clears and presents. */
void dxl_shell_frame(dxl_shell *s, double now);
/* Seconds until something changes with no input (a toast goes), or -1. */
double dxl_shell_idle_for(const dxl_shell *s);

/* For the pages. */
void        dxl_shell_go(dxl_shell *s, dxl_page p);
/* The textures of the window's renderer let go, before it goes: the art,
 * a save's snapshot, the thumbnails. */
void        dxl_shell_release(dxl_shell *s);
/* Whether the engine's --version names the feature (core/engine_args.h). */
int         dxl_shell_engine_has(const dxl_shell *s, const char *feature);
/* The game started on a save's slot: the screen done, to play. */
void        dxl_shell_play_save(dxl_shell *s, int slot);
/* Home's Continue: the latest save, NULL for none to start on -- no save,
 * no install or engine to play, or an engine that starts on none. */
const dxl_save *dxl_shell_continue(const dxl_shell *s);
void        dxl_shell_toast(dxl_shell *s, const char *fmt, ...);
void        dxl_shell_message(dxl_shell *s, const char *title, const char *fmt, ...);
void        dxl_shell_close_dialog(dxl_shell *s);
/* path for showing: the home folder as "~". */
const char *dxl_shell_tilde(const char *path, char *buf, size_t size);
/* The engine's --version, into s->engine_*: once. */
void        dxl_shell_ask_engine(dxl_shell *s);
/* The window in the player's menu theme -- User.ini's MenuThemeName, the
 * install's colours of it -- else the Default's, the kit's own. */
void        dxl_shell_theme(dxl_shell *s);
/* The install's title music and menu sounds read, and given the window's
 * sound when it has one, the music begun, at launcher.ini's volumes. */
void        dxl_shell_sound(dxl_shell *s);
/* launcher.ini's volumes on the window's sound. */
void        dxl_shell_volumes(dxl_shell *s);
/* The menu sound of a frame's cues (the kit's DXL_CUE_*), the most telling. */
void        dxl_shell_cue(dxl_shell *s, int cues);
/* The window on the big screen, or off it. */
void        dxl_shell_big(dxl_shell *s, int on);
/* launcher.ini's BigScreen as it is now, its value value: on or off at
 * once, automatic as --big-screen and the environment say. */
void        dxl_shell_big_setting(dxl_shell *s, const char *value);
/* The question the first press of a pad's asks, in a window that is not
 * the big screen: whether it should be. */
void        dxl_shell_offer_big(dxl_shell *s);
/* The install chosen: on the first run the profile to be made from it, on
 * Your profile; else the profile pointed at it, and the screen done -- or,
 * asked for from a page but Recovery, that page again. */
void        dxl_shell_choose(dxl_shell *s, const dxl_install *in);
/* The game page for the profile's install from now on, its Back to page. */
void        dxl_shell_repoint(dxl_shell *s, dxl_page back_to);
/* An engine from path, a folder with it or its program: checked and taken,
 * launcher.ini's from now on, 1; else 0, and brief and why say why not. */
int         dxl_shell_choose_engine(dxl_shell *s, const char *path, char *brief, size_t brief_size, char *why,
                                    size_t why_size);
/* The engine's choices, a pick for the control id -- the launcher's own,
 * another by the browser, by the system's dialog -- and, read on id's
 * frames, its answer done. */
void        dxl_shell_engine_pick(dxl_shell *s, uint32_t id);
void        dxl_shell_engine_answer(dxl_shell *s, uint32_t id);
/* The system's folder dialog shown for what (DXL_BROWSE_*), from the page
 * as it is now. */
void        dxl_shell_pick_folder(dxl_shell *s, int what);
/* A folder it gave: for what it was shown for, if the page it was shown
 * from is still up as it was; else dropped. */
void        dxl_shell_folder_picked(dxl_shell *s, const char *path);
/* A folder or file with the desktop's handler. */
void        dxl_shell_open(dxl_shell *s, const char *path);
/* The GPU probe asked for, once: renderers.ini's list loaded now, resolved
 * as dxl_shell_probed hands the answer in. */
void        dxl_shell_probe(dxl_shell *s);
void        dxl_shell_probed(dxl_shell *s, const dxl_gpu_probe *p);
/* The probe's answer in, with no GPU driver the GPU renderers can use: a
 * profile made here begins on Software, and so do its defaults. */
int         dxl_shell_no_gpu(const dxl_shell *s);

/* The pages, each in r: the window less its head and foot. */
void dxl_page_home(dxl_shell *s, dxl_rect r);
void dxl_page_home_enter(dxl_shell *s);
void dxl_page_welcome(dxl_shell *s, dxl_rect r);
void dxl_page_find(dxl_shell *s, dxl_rect r);
void dxl_page_find_enter(dxl_shell *s);
/* The finder asked for, the game page's list emptied for what it finds:
 * the places core/finder.h has, with --game's folder and launcher.ini's
 * installs. */
void dxl_shell_find(dxl_shell *s);
void dxl_shell_found(dxl_shell *s, const dxl_found *f);
void dxl_shell_found_all(dxl_shell *s);
void dxl_page_import(dxl_shell *s, dxl_rect r);
void dxl_page_recovery(dxl_shell *s, dxl_rect r);
void dxl_page_recovery_enter(dxl_shell *s);
void dxl_page_about(dxl_shell *s, dxl_rect r);
void dxl_page_profiles(dxl_shell *s, dxl_rect r);
void dxl_page_profiles_enter(dxl_shell *s);
void dxl_page_saves(dxl_shell *s, dxl_rect r);
void dxl_page_saves_enter(dxl_shell *s);
/* The profile's saves read again into s->save_list, the chosen kept by
 * its slot where it is still there. */
void dxl_saves_reload(dxl_shell *s);
/* The chosen save's snapshot drawn fitted in r, its picture's part (the
 * game draws 160 x 120 of its 256 x 128); 0 for none to draw. */
int  dxl_save_snapshot(dxl_shell *s, const dxl_save *sv, dxl_rect r);
void dxl_page_shots(dxl_shell *s, dxl_rect r);
void dxl_page_shots_enter(dxl_shell *s);
void dxl_page_servers(dxl_shell *s, dxl_rect r);
void dxl_page_servers_enter(dxl_shell *s);
void dxl_page_mods(dxl_shell *s, dxl_rect r);
void dxl_page_mods_enter(dxl_shell *s);
/* How a profile to be made begins, from its plan: the engine's drawing,
 * and the Modern layout unless its controls are the player's own. */
void dxl_shell_begin_defaults(dxl_shell *s);
/* The profile of id, named name, played from now on, as the Profiles page
 * uses one: Home, or Recovery when its last run left its Running.ini. */
void dxl_shell_use_profile(dxl_shell *s, const char *id, const char *name);
/* A mod's folder shown, by the browser or the system's dialog: the Mods
 * page's, chosen. */
void dxl_shell_mod_shown(dxl_shell *s, const dxl_mod *m);
/* The profile in use's mod looked at again (profile_mod). */
void dxl_shell_profile_mod(dxl_shell *s);
/* The servers asked for again: the profile's master's, and its favourites. */
#define DXL_SERVERS_MAX 256
void dxl_shell_browse(dxl_shell *s);
void dxl_shell_server_listed(dxl_shell *s, const char *address, int query_port, int lan);
void dxl_shell_server_answered(dxl_shell *s, const char *address, int query_port, int ping_ms, const char *status);
/* The look's end: error, the master's failure, or NULL. */
void dxl_shell_browsed(dxl_shell *s, const char *error);
/* The profile changed (another used, one made, a making cancelled): its
 * master and favourites are the look's, asked again as the page comes. */
void dxl_shell_rebrowse(dxl_shell *s);
/* The game started joining host:port, with a password for it or none: the
 * screen done, to play. */
void dxl_shell_join(dxl_shell *s, const char *host, int port, int lan, const char *password);
/* A deusex:// link's server, host:port: Multiplayer, once a page is up
 * between flows -- no dialog, no game or profile being chosen -- asking
 * whether to join it. */
void dxl_shell_join_link(dxl_shell *s, const char *host_port);
/* A new profile not made after all: the one in use before it again, and
 * the Profiles page. */
void dxl_shell_making_cancel(dxl_shell *s);
void dxl_page_settings(dxl_shell *s, dxl_rect r);
void dxl_page_log(dxl_shell *s, dxl_rect r);
void dxl_page_log_enter(dxl_shell *s);
void dxl_page_log_free(dxl_shell *s);
void dxl_page_settings_enter(dxl_shell *s);
/* Settings' Controller tab in area: why and focused, its help line's. */
void dxl_page_controller(dxl_shell *s, dxl_rect area, const char **why, const dxl_setting **focused);
/* A page's registry rows in the order shown; one drawn as its kind. */
int  dxl_settings_rows(const char *page, const dxl_setting **out, int max);
void dxl_settings_row(dxl_shell *s, const dxl_setting *st, uint32_t id, dxl_rect r);
/* The mapping wizard for the controller id, and its dialog. */
void dxl_padwizard_open(dxl_shell *s, SDL_JoystickID id);
void dxl_padwizard(dxl_shell *s, dxl_rect over, int back);
/* The controller id tried, and its dialog: every pad's input drives the
 * model alone, until the controller's back button is held a second, Esc,
 * or Stop. */
void dxl_padtest_open(dxl_shell *s, SDL_JoystickID id);
void dxl_padtest(dxl_shell *s, dxl_rect over, int back);
/* SDL's mappings as the launcher has them: SDL_GAMECONTROLLERCONFIG's, as
 * Steam set it, but for the controllers the launcher maps, and then those
 * of its gamecontrollerdb.txt (core/padmap.h). start: SDL's gamepads
 * started with them, 0 when they cannot be; else its mappings made again. */
int  dxl_pad_mappings(const dxl_run *r, int start);
/* The settings kept: their files written, the stores let go. */
void dxl_settings_keep(dxl_shell *s);
/* A page's settings (NULL: every page's) as a new profile has them, the
 * keys too on the keyboard's: the open stores', kept as the page is. */
void dxl_settings_defaults(dxl_shell *s, const char *page);
/* The screen is done (the window closes): what a page holds, kept. */
void dxl_shell_leave(dxl_shell *s);

/* The asking dialogs, each for the control id that asks; its answer,
 * read once: dxl_shell_answer gives the choice (pick), 1 for a text kept
 * (osk) or yes (confirm), 0 for no, and -1 while there is none. A pick or
 * the keyboard put back gives none. */
void dxl_shell_pick(dxl_shell *s, uint32_t id, const char *title, const char *const *labels, int n, int current);
/* The same, each choice's group under its label (details NULL for none). */
void dxl_shell_pick_with(dxl_shell *s, uint32_t id, const char *title, const char *const *labels,
                         const char *const *details, int n, int current);
/* A pick's answer as the label it chose, the dialog's copy: a list made
 * again may have changed under it (the renderers, as the probe answers).
 * NULL for no answer. */
const char *dxl_shell_picked(const dxl_shell *s, int answer);
/* The answer's place in labels, the list as it is now: its own where its
 * label is there still, two of a list's labels alike (two controllers of
 * one model), else the first of its label; -1 for no answer, or none of
 * its label left. */
int         dxl_shell_picked_at(const dxl_shell *s, int answer, const char *const *labels, int n);
void dxl_shell_osk(dxl_shell *s, uint32_t id, const char *title, char *buf, size_t size);
void dxl_shell_confirm(dxl_shell *s, uint32_t id, const char *title, const char *text, const char *yes,
                       const char *no);
/* The same with a third answer between them, 2. */
void dxl_shell_confirm3(dxl_shell *s, uint32_t id, const char *title, const char *text, const char *yes,
                        const char *third, const char *no);
int  dxl_shell_answer(dxl_shell *s, uint32_t id);

/* The dialogs: over r, the window less its foot; back is the Esc or the
 * pad's back the dialog takes. */
void dxl_browser_open(dxl_shell *s, const char *dir, int mode);
void dxl_browser_free(dxl_browser *b);
void dxl_dialogs(dxl_shell *s, dxl_rect r, int back);

#endif
