/* dxl-cli: the launcher with no window, for scripts and for looking.
 *
 *   dxl-cli --dry-run [--game=<dir>] [--profile=<id>] [words...]
 *   dxl-cli --print-command [...]     the engine's command line, nothing run
 *   dxl-cli --import --game=<dir> [--profile=<id>]
 *   dxl-cli --play [...]              play, no Home (app/run.h)
 *   dxl-cli --probe [--ini]           the GPU this machine has; --ini, for a program
 *   dxl-cli --find                    the installs the first run would list
 *   dxl-cli --mods [--game=<dir>]     the mods a profile can be made of (core/mods.h)
 *   dxl-cli --import --game=<dir> --mod=<dir> --profile=<id>   a mod's profile
 *   dxl-cli --servers[=<master>]      the servers a master lists, as the browser asks them
 *   dxl-cli --lan                     the servers on the local network, as the browser asks them
 *
 * The words are the original's command line (-safe, -changevideo, a map
 * with -hax0r, ...), as deusex-launcher takes them.
 */
#define _GNU_SOURCE
#include "app/browser.h"
#include "app/run.h"
#include "app/session.h"
#include "core/finder.h"
#include "core/servers.h"
#include "platform/gpu_probe.h"
#include "platform/startup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DXL_VERSION
#define DXL_VERSION "unversioned"
#endif

static volatile int stop;

static void usage(FILE *f) {
    fprintf(f,
        "usage: dxl-cli --dry-run | --print-command | --import | --play | --probe | --find | --mods |\n"
        "               --servers | --lan [--game=<dir>] [--mod=<dir>] [--profile=<id>] [the original's command line...]\n"
        "\n"
        "  --dry-run        what the launcher would decide, make and run; nothing is written\n"
        "  --print-command  the engine's command line for this profile; nothing is run\n"
        "  --import         a profile made from --game's install, nothing run\n"
        "  --play           play, with no Home screen; a screen that is due is a message\n"
        "  --probe [--ini]  the Vulkan, OpenGL ES and OpenGL this machine offers; --ini as\n"
        "                   the [Probe] section the launcher's window reads\n"
        "  --find           the installs of Deus Ex the first run would find here, but\n"
        "                   launcher.ini's: each folder, where it was found, its build\n"
        "  --mods           the mods a profile can be made of: the install's folder's (the profile's,\n"
        "                   or --game's), the launcher's mods folder's, the profiles'\n"
        "  --mod=<dir>      with --import, the profile made a mod's: its folders first in its paths\n"
        "  --servers[=<master>]  the servers the master (master.333networks.com) lists, each\n"
        "                   asked as the browser asks it: name, map, players, ping, address\n"
        "  --lan            the servers on the local network, as the game's Join LAN screen finds\n"
        "                   them (REPORTQUERY to ports 8777-8786), each asked so\n"
        "  --game=<dir>     the game's folder: the first run's, or the profile's from now on\n"
        "  --profile=<id>   another profile than launcher.ini's (default: default)\n"
        "\n"
        "Files: the launcher's own under $XDG_CONFIG_HOME, $XDG_DATA_HOME, $XDG_STATE_HOME\n"
        "and $XDG_CACHE_HOME, each in dxlauncher/ (DXL_HOME=<dir>: all four under <dir>);\n"
        "nothing in the game's folder. The engine is the launcher's own, beside it, or\n"
        "launcher.ini's [Launcher] Engine; DXL_ENGINE=<path> runs another over both, and\n"
        "DXL_ENGINE_ARGS adds more of its own options. DXL_LAN=<host>[:<port>] asks that one\n"
        "place's beacons in place of the network's, DXL_LAN=none no beacon.\n");
}

static int probe(int as_ini) {
    dxl_gpu_probe p;
    int rc = dxl_gpu_probe_run(&p, 4000);
    if (as_ini) {
        char *text = dxl_gpu_probe_to_ini(&p);
        fputs(text, stdout);
        free(text);
        return rc != 0;
    }
    if (rc != 0) {
        printf("probe failed: %s\n", p.note);
        return 1;
    }
    printf("vulkan     %s\n", p.vulkan ? "yes" : "no");
    if (p.vulkan) printf("  device   %s\n  api      %s\n", p.vulkan_device, p.vulkan_version);
    printf("opengl es  %s\n", p.gles ? "yes" : "no");
    if (p.gles) {
        if (p.gles_renderer[0]) printf("  renderer %s\n", p.gles_renderer);
        printf("  version  %s\n", p.gles_version);
    }
    printf("opengl     %s\n", p.gl ? "yes" : "no");
    return 0;
}

/* The finder's places, this machine's own, each install on two lines. */
static int find(void) {
    dxl_finder_where where;
    memset(&where, 0, sizeof where);
    dxl_found_list l;
    dxl_finder_find(&l, &where);
    for (int i = 0; i < l.count; i++) {
        const dxl_install *in = &l.items[i].install;
        printf("%s\n    %s: ", in->root, l.items[i].source);
        if (in->ok) printf("Deus Ex %s (%s)%s%s\n", in->build->version, in->build->edition, in->problem[0] ? "; " : "",
                           in->problem);
        else printf("%s\n", in->problem);
    }
    if (!l.count) printf("no Deus Ex found\n");
    dxl_found_list_free(&l);
    return 0;
}

/* The mods a profile can be made of, each with its settings and the
 * profiles that play it: the install's folder's (--game's, else the
 * profile's), the launcher's mods folder's, the profiles'. */
static int list_mods(int argc, char **argv) {
    const char *game = NULL, *profile = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "--game=", 7)) game = argv[i] + 7;
        else if (!strncmp(argv[i], "--profile=", 10)) profile = argv[i] + 10;
    }
    dxl_session s;
    dxl_err e;
    if (dxl_session_open(&s, profile, &e) != 0) {
        fprintf(stderr, "%s\n", dxl_err_msg(&e));
        return 1;
    }
    const char *root = game ? game : dxl_session_install_path(&s, dxl_profile_get(&s.profile, "Install"));
    char *folder = dxl_session_mods_dir(&s);
    printf("the game's folder: %s\nthe launcher's mods folder: %s\n", root ? root : "none: --game=<dir> names it",
           folder);
    free(folder);
    dxl_mod_list l;
    dxl_session_mods(&s, root, &l);
    for (int i = 0; i < l.count; i++) {
        const dxl_mod *m = &l.items[i];
        printf("\n%s\n    %s, %s: %d package%s\n    its settings: %s\n    its controls: %s\n", m->name,
               dxl_mod_where_text(m->where), m->dir, m->packages, m->packages == 1 ? "" : "s",
               m->ini ? m->ini : "the game's", m->user_ini ? m->user_ini : "the game's");
        char ids[8][DXL_PROFILE_ID_MAX + 1];
        size_t n = dxl_session_mod_profiles(&s, m->dir, ids, 8);
        printf("    played by: ");
        for (size_t k = 0; k < n && k < 8; k++) printf("%s%s", k ? ", " : "", ids[k]);
        printf("%s\n", n ? "" : "no profile yet");
    }
    if (!l.count) printf("\nno mods found\n");
    dxl_mod_list_free(&l);
    dxl_session_close(&s);
    return 0;
}

/* The browser's look, each server on a line as it answers, then all of
 * them by their ping. */
typedef struct {
    dxl_server s[DXL_BROWSE_MAX];
    int        count;
} servers;

static void listed(void *ctx, const char *address, int port, int lan) {
    servers *l = ctx;
    for (int i = 0; i < l->count; i++)
        if (l->s[i].query_port == port && !strcmp(l->s[i].address, address)) {
            l->s[i].lan |= lan;
            return;
        }
    if (l->count < DXL_BROWSE_MAX) {
        dxl_server_init(&l->s[l->count], address, port);
        l->s[l->count++].lan = lan;
    }
}

static void answered(void *ctx, const char *address, int port, int ping, const char *status) {
    servers *l = ctx;
    for (int i = 0; i < l->count; i++)
        if (l->s[i].query_port == port && !strcmp(l->s[i].address, address)) {
            dxl_server_take_status(&l->s[i], status);
            l->s[i].ping_ms = ping;
        }
}

static int by_ping(const void *a, const void *b) { return dxl_server_compare(a, b, DXL_SORT_PING); }

/* A server's own text for the terminal: its control characters, escapes
 * among them, as '?'. */
static const char *clean(const char *in, char *out, size_t size) {
    size_t n = 0;
    for (; *in && n + 1 < size; in++) out[n++] = (unsigned char)*in < 0x20 || *in == 0x7f ? '?' : *in;
    out[n] = '\0';
    return out;
}

/* master's servers, or (NULL) the local network's. */
static int list_servers(const char *master) {
    static servers l;
    dxl_browse_fns fns = { listed, answered };
    dxl_err e;
    dxl_browse_lan lan;
    dxl_browse_lan_init(&lan, DXL_BEACON_PORT, DXL_BEACON_PRODUCT);
    if (dxl_browse(master, NULL, master ? NULL : &lan, &fns, &l, NULL, &e) != 0) {
        fprintf(stderr, "%s\n", dxl_err_msg(&e));
        return 1;
    }
    qsort(l.s, (size_t)l.count, sizeof *l.s, by_ping);
    int answers = 0;
    for (int i = 0; i < l.count; i++) {
        const dxl_server *s = &l.s[i];
        if (s->state != DXL_SERVER_ANSWERED) {
            printf("%s:%d    no answer\n", s->address, s->port);
            continue;
        }
        answers++;
        char name[256], map[128], game[128];
        printf("%s\n    %s, %s, %d/%d, %d ms, %s:%d%s%s%s\n", clean(s->name, name, sizeof name), clean(s->map, map, sizeof map),
               clean(s->game_type, game, sizeof game), s->players, s->max_players, s->ping_ms, s->address, s->port,
               s->lan ? ", LAN" : "", s->password ? ", password" : "", dxl_server_joinable(s) ? "" : ", not this version");
    }
    if (master) printf("%d listed by %s, %d answered\n", l.count, master, answers);
    else printf("%d on the local network, %d answered\n", l.count, answers);
    for (int i = 0; i < l.count; i++) dxl_server_free(&l.s[i]);
    return 0;
}

int main(int argc, char **argv) {
    int mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "--version") == 0) { printf("dxl-cli %s\n", DXL_VERSION); return 0; }
        if (strcmp(argv[i], "--find") == 0) return find();
        if (strcmp(argv[i], "--mods") == 0) return list_mods(argc, argv);
        if (strcmp(argv[i], "--servers") == 0) return list_servers("master.333networks.com");
        if (strncmp(argv[i], "--servers=", 10) == 0) return list_servers(argv[i] + 10);
        if (strcmp(argv[i], "--lan") == 0) return list_servers(NULL);
        if (strcmp(argv[i], "--probe") == 0) {
            int as_ini = i + 1 < argc && strcmp(argv[i + 1], "--ini") == 0;
            return probe(as_ini);
        }
        if (!strcmp(argv[i], "--dry-run") || !strcmp(argv[i], "--print-command") ||
            !strcmp(argv[i], "--import") || !strcmp(argv[i], "--play"))
            mode = 1;
    }
    if (!mode) {
        usage(stderr);
        return DXL_RUN_EXIT_USAGE;
    }
    dxl_startup(&stop, NULL);
    int rc = dxl_run_main(argc, argv, NULL, &stop);
    dxl_startup_ended();
    return rc;
}
