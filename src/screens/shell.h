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
 *   About       the launcher, the engine, the folders, the app menu's entry
 *
 * Graphics (-changevideo) opens Home: there is no Graphics page.
 *
 * Every page draws itself from the run's state and the shell's own, in
 * dxl_shell_frame; the window around it is window.h's, or dxl-shots' (a
 * software renderer on a surface, and a clock of its own).
 */
#ifndef DXL_SHELL_H
#define DXL_SHELL_H

#include "app/run.h"
#include "core/finder.h"
#include "ui/glyphs.h"
#include "ui/kit.h"

typedef enum {
    DXL_PAGE_HOME,
    DXL_PAGE_WELCOME,
    DXL_PAGE_FIND,
    DXL_PAGE_IMPORT,
    DXL_PAGE_RECOVERY,
    DXL_PAGE_ABOUT,
    DXL_PAGE_COUNT
} dxl_page;

typedef enum { DXL_DIALOG_NONE, DXL_DIALOG_BROWSER, DXL_DIALOG_MESSAGE } dxl_dialog;

typedef struct {
    char  *dir;          /* the folder shown */
    char **names;        /* the folders in it, sorted */
    int    count;
    int    hidden;       /* those whose names start with a dot listed too */
    char   note[400];    /* why the folder last tried is not an install */
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
    uint32_t     focus[DXL_PAGE_COUNT];   /* each page's focus, for coming back to it */

    /* The engine's --version, asked once. */
    int          engine_asked;
    int          engine_ok;
    char         engine_version[160], engine_features[256], engine_problem[512];

    /* The first run, and a profile's install found again. */
    int          repoint;         /* The game is for the profile's install, not a first run */
    int          looked;          /* the finder has run */
    dxl_found_list found;
    dxl_install  chosen;          /* the install the profile is to be made from */
    int          has_chosen;
    dxl_import   plan;            /* what making it will do */
    char         plan_error[320];

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
/* The install chosen: on the first run the profile to be made from it, on
 * Your profile; else the profile pointed at it, and the screen done. */
void        dxl_shell_choose(dxl_shell *s, const dxl_install *in);
/* A folder or file with the desktop's handler. */
void        dxl_shell_open(dxl_shell *s, const char *path);

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

/* The dialogs: over r, the window less its foot; back is the Esc or the
 * pad's back the dialog takes. */
void dxl_browser_open(dxl_shell *s, const char *dir);
void dxl_browser_free(dxl_browser *b);
void dxl_dialogs(dxl_shell *s, dxl_rect r, int back);

#endif
