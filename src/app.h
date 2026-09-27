/* The launcher session.
 *
 * Everything the launcher does, in the order InitEngine does it, with no UI
 * dependency. The frontend calls dxl_app_init, looks at app->decision to see
 * where the home screen should open, mutates settings through the core API
 * as the user makes choices, and finishes with dxl_app_commit +
 * dxl_app_launch.
 *
 * Keeping this UI-free is what lets --dry-run exercise the entire contract
 * without a display, which is how the on-device behaviour gets verified
 * before any pixel is drawn.
 */
#ifndef DXL_APP_H
#define DXL_APP_H

#include "core/bindings.h"
#include "core/common.h"
#include "core/config.h"
#include "core/engine_settings.h"
#include "core/install.h"
#include "core/instance.h"
#include "core/policy.h"
#include "core/argv.h"
#include "core/renderers.h"
#include "core/sentinel.h"

typedef struct {
    /* Where things are */
    char *app_dir;        /* the launcher's own directory */
    char *exe_path;
    char *game_dir;       /* where the user put the game files */
    char *system_dir;     /* <game_dir>/System, case-resolved */
    char *package;        /* "DeusEx" -- names the ini and the log */
    char *game_command;   /* what to exec once configuration is settled */
    char *engine_log;     /* the engine's SE-Log-LastRun.txt */
    char *run_log;        /* run-game.sh's log, with the engine's exit codes */

    char *cmdline;        /* argv[1..] joined */

    dxl_ini *launcher_ini;      /* the launcher's own settings (launcher.ini) */
    char    *launcher_ini_path;

    dxl_config          *cfg;       /* DeusEx.ini and the engine's companions */
    dxl_engine_settings *es;        /* Settings.json */
    dxl_renderer_list    renderers;
    dxl_gpu_probe        gpu;
    dxl_install          install;
    dxl_sentinel         sentinel;
    dxl_instance        *instance;  /* NULL when another instance holds it */
    dxl_decision         decision;

    /* The first time this launcher saw the install, the pad layout was
     * switched from the shipped bindings to the default preset. */
    int pad_layout_applied;
    /* A layout from an earlier launcher was replaced by its revision; this
     * says what changed, for the player. NULL otherwise. */
    const char *pad_layout_update;
} dxl_app;

/* Resolves paths, reads launcher.ini, loads config, probes the install,
 * handles the single-instance question and computes the decision. Does not
 * probe the GPU -- see dxl_app_probe_gpu. Returns 0 on success. */
int  dxl_app_init(dxl_app *app, int argc, char **argv, dxl_err *err);
void dxl_app_shutdown(dxl_app *app);

/* Runs the crash-isolated GPU probe and re-resolves which renderers can run.
 * Call before the display comes up. */
void dxl_app_probe_gpu(dxl_app *app);

/* True when the game files are not (yet) where they should be. The frontend
 * shows the install screen and nothing else. */
int  dxl_app_needs_install(const dxl_app *app);
/* Re-probes the install after the user has copied files. */
void dxl_app_recheck_install(dxl_app *app);

/* The renderer entry currently configured in Settings.json, or -1 if the
 * configured Type is not one this launcher knows. */
int  dxl_app_current_renderer(const dxl_app *app);
/* Chooses a renderer by list index. Refused (returns -1) if it cannot run. */
int  dxl_app_choose_renderer(dxl_app *app, int index);

/* Applies a pad preset to the user ini and records it in Settings.json. */
void dxl_app_choose_layout(dxl_app *app, const dxl_pad_preset *p);
/* The preset the bindings currently match, or NULL for "custom". */
const dxl_pad_preset *dxl_app_current_layout(dxl_app *app);
/* The preset last applied -- what "default" means for one button even after
 * others have been remapped. */
const dxl_pad_preset *dxl_app_base_layout(dxl_app *app);
/* Binds one pad button (a Joy key) to a command. */
void dxl_app_bind(dxl_app *app, const char *joy_key, const char *command);

/* CPU mode for the game: one of the device profile's modes
 * (platform/target.h), kept in launcher.ini as CpuMode and applied by the
 * port's run-game hooks just before the engine starts. NULL when the device
 * offers none. */
const char *dxl_app_cpu_mode(const dxl_app *app);
void        dxl_app_set_cpu_mode(dxl_app *app, const char *mode);

/* Deletes and rebuilds DeusEx.ini/User.ini (and the engine's SE- copies)
 * from Default.ini/DefUser.ini, keeping FirstRun. */
int  dxl_app_reset_game_config(dxl_app *app, dxl_err *err);

/* Writes settings without launching: the ini files and Settings.json. Used
 * when leaving the launcher with Quit, so choices are not lost. */
int  dxl_app_save_settings(dxl_app *app, dxl_err *err);

/* Pre-launch: create the sentinel, clamp FirstRun, save everything. Order
 * matters and is the original's. */
int  dxl_app_commit(dxl_app *app, dxl_err *err);

/* Replaces this process with the game. Only returns on failure. */
int  dxl_app_launch(dxl_app *app, dxl_err *err);

/* Prints the whole decision and the pending config without touching
 * anything. */
void dxl_app_dry_run(dxl_app *app);

#endif
