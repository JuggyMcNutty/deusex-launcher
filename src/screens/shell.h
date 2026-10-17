/* What the player sees of one start (app/run.h): the pages and dialogs of
 * the launcher's window, drawn frame by frame with the kit (ui/kit.h).
 *
 * run.c asks for a screen -- Home, the first run, Recovery, Graphics --
 * through dxl_run_ui; the shell shows it as pages until the player plays
 * or quits:
 *
 *   Home        Play, About, Quit; the profile, its game, the engine, the
 *               last run
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

#include "app/run.h"
#include "core/finder.h"
#include "core/renderers.h"
#include "core/padmap.h"
#include "core/settings.h"
#include "core/theme_colors.h"
#include "ui/art.h"
#include "ui/audio.h"
#include "ui/glyphs.h"
#include "ui/kit.h"
#include "ui/osk.h"
#include "ui/padmesh.h"

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
    DXL_PAGE_COUNT
} dxl_page;

/* The most profiles the Profiles page lists. */
#define DXL_PROFILES_MAX 32

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
#define DXL_BROWSE_ENGINE 1   /* the engine's: VibeEngine or SurrealEngine in it */

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
    int          looked;          /* the finder has run */
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
     * profile before it, gone back to if it is not made. */
    int          making;
    char         making_before[DXL_PROFILE_ID_MAX + 1];

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
    char         note[400];       /* a line under Home's panel: what the first run did */
    char        *log_tail;        /* Recovery's: the end of the last run's log */

    /* Dialogs. */
    dxl_dialog   dialog;
    uint32_t     dialog_return;   /* the focus it took, given back as it closes */
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
    char         themes_from[1024];
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
    char         sounds_from[1024];
    dxl_audio   *audio;
    int          audio_given;

    /* The GPU this machine has, as dxl-cli --probe --ini answers in the
     * window's worker (start_probe): 0 not asked, 1 asked, 2 known; and
     * renderers.ini's list, resolved by it. */
    int          probe_state;
    dxl_gpu_probe probe;
    dxl_renderer_list renderers;
    void       (*start_probe)(struct dxl_shell *s);

    struct { char text[200]; double at; } toasts[DXL_TOASTS];
    int          toast_count;

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

/* The pages, each in r: the window less its head and foot. */
void dxl_page_home(dxl_shell *s, dxl_rect r);
void dxl_page_home_enter(dxl_shell *s);
void dxl_page_welcome(dxl_shell *s, dxl_rect r);
void dxl_page_find(dxl_shell *s, dxl_rect r);
void dxl_page_find_enter(dxl_shell *s);
void dxl_page_import(dxl_shell *s, dxl_rect r);
void dxl_page_recovery(dxl_shell *s, dxl_rect r);
void dxl_page_recovery_enter(dxl_shell *s);
void dxl_page_about(dxl_shell *s, dxl_rect r);
void dxl_page_profiles(dxl_shell *s, dxl_rect r);
void dxl_page_profiles_enter(dxl_shell *s);
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
