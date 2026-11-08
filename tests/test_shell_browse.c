/* The master server Multiplayer asks (screens/servers.c), as the game's
 * Join Internet asks it: MenuScreenJoinInternet.Query hands
 * [DeusEx.MenuScreenJoinGame]'s MasterServerAddress and MasterServerTCPPort
 * to DeusExGSpyLink, whose Start resolves the address whole and opens it at
 * the port, 28900 the class's (bugs.md LCORE.16). */
#include "scratch.h"
#include "core/config.h"
#include "screens/shell.h"

static dxl_shell s;
static char asked[300];

static void capture(dxl_shell *sh, const char *master, const char *const *also, const dxl_browse_lan *lan) {
    (void)sh;
    (void)also;
    (void)lan;
    snprintf(asked, sizeof asked, "%s", master ? master : "(none)");
}

/* The master handed to the look for a profile whose DeusEx.ini's
 * [DeusEx.MenuScreenJoinGame] is section. */
static void asks(const char *tag, const char *section, const char *want) {
    const char *home = scratch_dir(tag);
    char text[300];
    snprintf(text, sizeof text, "[DeusEx.MenuScreenJoinGame]\n%s", section);
    scratch_write(home, "System/DeusEx.ini", text);
    char *sys = dxl_path_join(home, "System");
    char *ini = dxl_path_join(sys, "DeusEx.ini");
    dxl_env_set("DXL_HOME", home);
    dxl_env_set("DXL_LAN", "none");
    static dxl_run r;
    memset(&r, 0, sizeof r);
    dxl_err e;
    CHECK_INT(dxl_session_open(&r.session, NULL, &e), 0);
    r.config = dxl_config_open_files(sys, "DeusEx", ini, NULL);
    dxl_shell_init(&s, "test");
    s.run = &r;
    s.start_browse = capture;
    asked[0] = '\0';
    dxl_shell_browse(&s);
    if (strcmp(asked, want) != 0) fprintf(stderr, "  %s", text);
    CHECK_STR(asked, want);
    free(s.servers);
    s.servers = NULL;
    dxl_config_free(r.config);
    dxl_session_close(&r.session);
    dxl_env_set("DXL_LAN", NULL);
    dxl_env_set("DXL_HOME", NULL);
    CHECK_INT(dxl_fs_remove_tree(home), 0);
    free(ini);
    free(sys);
}

static void test_master_at_its_port(void) {
    asks("browse-port", "MasterServerAddress=master.example.org\nMasterServerTCPPort=28910\n",
         "master.example.org:28910");
    /* The class's port where the ini names none, or none its import reads. */
    asks("browse-no-port", "MasterServerAddress=master.example.org\n", "master.example.org:28900");
    asks("browse-plus-port", "MasterServerAddress=master.example.org\nMasterServerTCPPort=+28910\n",
         "master.example.org:28900");
    asks("browse-port-after", "MasterServerAddress=master.example.org\nMasterServerTCPPort=28910 \n",
         "master.example.org:28910");
    /* An address with a port of its own is resolved whole, as the game's
     * Resolve takes it, and no master is found there. */
    asks("browse-port-in-address", "MasterServerAddress=master.example.org:28910\n",
         "master.example.org:28910:28900");
    /* None at all: 333networks'. */
    asks("browse-no-master", "MasterServerAddress=\n", "master.333networks.com:28900");
}

TEST_MAIN_BEGIN
    RUN(test_master_at_its_port);
TEST_MAIN_END
