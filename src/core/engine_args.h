/* The engine's command line, as a vector of words: no shell between the
 * launcher and the engine, so a quoted word arrives as given.
 *
 *   <engine> --no-launcher <game> --ini=<profile>/DeusEx.ini
 *            --userini=<profile>/User.ini --userdir=<profile>/engine
 *            [--url=<url>] [DXL_ENGINE_ARGS's words] --cmdline=<line>
 *
 * --cmdline is the original's command line: the player's words, and this
 * run's flags (-nosound and the rest of safe mode's). A URL to start at --
 * a map, a server's address, a save to load -- goes in --url, not in
 * --cmdline, where the engine takes a start URL only with -hax0r or -server
 * (the engine's NATIVES.md, the command line).
 */
#ifndef DXL_ENGINE_ARGS_H
#define DXL_ENGINE_ARGS_H

#include "core/common.h"

typedef struct {
    const char *engine;     /* the engine's program */
    const char *game;       /* the game's folder */
    const char *ini;        /* the profile's DeusEx.ini */
    const char *user_ini;   /* its User.ini */
    const char *user_dir;   /* its engine folder */
    const char *url;        /* where to start, or NULL */
    const char *extra;      /* more of the engine's own options, split as a shell would; NULL for none */
    const char *cmdline;    /* the original's command line, or NULL */
} dxl_engine_command;

/* NULL-terminated; free with dxl_argv_free (core/argv.h). */
char **dxl_engine_argv(const dxl_engine_command *c);

/* The words as a shell would need them typed, for --print-command and the
 * log. Caller frees. */
char *dxl_argv_quoted(char *const *argv);

/* What the engine's --version says: "VibeEngine <commit> (<date>)", then
 * "features: <word> ...", what a launcher can rely on it to do. */
typedef struct {
    char version[160];
    char features[256];
} dxl_engine_version;

/* 0 when text is a VibeEngine's answer. */
int dxl_engine_version_parse(const char *text, dxl_engine_version *out);
/* The feature named is in the list. */
int dxl_engine_has(const dxl_engine_version *v, const char *feature);

/* What the launcher needs of the engine: its own folder, so that it never
 * touches ~/.config/SurrealEngine, which the harness's runs read. */
#define DXL_ENGINE_NEEDS "userdir"

#endif
