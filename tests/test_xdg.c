#include "test.h"
#include "core/xdg.h"
#include "platform/fs.h"
#include "platform/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void clear_env(void) {
    dxl_env_set("DXL_HOME", NULL);
    dxl_env_set("XDG_CONFIG_HOME", NULL);
    dxl_env_set("XDG_DATA_HOME", NULL);
    dxl_env_set("XDG_STATE_HOME", NULL);
    dxl_env_set("XDG_CACHE_HOME", NULL);
}

/* Nothing set: the spec's defaults under HOME. */
static void test_defaults(void) {
    clear_env();
    dxl_env_set("HOME", "/home/jc");
    dxl_xdg x;
    CHECK_INT(dxl_xdg_open(&x, NULL), 0);
    CHECK_STR(x.config, "/home/jc/.config/dxlauncher");
    CHECK_STR(x.data, "/home/jc/.local/share/dxlauncher");
    CHECK_STR(x.state, "/home/jc/.local/state/dxlauncher");
    CHECK_STR(x.cache, "/home/jc/.cache/dxlauncher");
    CHECK_STR(x.data_home, "/home/jc/.local/share");
    dxl_xdg_free(&x);
}

/* Each variable, when it is an absolute path; a relative one or an empty
 * one counts as unset, as the spec has it. */
static void test_variables(void) {
    clear_env();
    dxl_env_set("HOME", "/home/jc");
    dxl_env_set("XDG_CONFIG_HOME", "/cfg");
    dxl_env_set("XDG_DATA_HOME", "relative/data");
    dxl_env_set("XDG_STATE_HOME", "");
    dxl_env_set("XDG_CACHE_HOME", "/var/cache/jc");
    dxl_xdg x;
    CHECK_INT(dxl_xdg_open(&x, NULL), 0);
    CHECK_STR(x.config, "/cfg/dxlauncher");
    CHECK_STR(x.data, "/home/jc/.local/share/dxlauncher");
    CHECK_STR(x.state, "/home/jc/.local/state/dxlauncher");
    CHECK_STR(x.cache, "/var/cache/jc/dxlauncher");
    dxl_xdg_free(&x);
}

/* DXL_HOME puts all four under it, whatever else is set. */
static void test_scratch_home(void) {
    clear_env();
    dxl_env_set("HOME", "/home/jc");
    dxl_env_set("XDG_CONFIG_HOME", "/cfg");
    dxl_env_set("DXL_HOME", "/tmp/fresh");
    dxl_xdg x;
    CHECK_INT(dxl_xdg_open(&x, NULL), 0);
    CHECK_STR(x.config, "/tmp/fresh/config/dxlauncher");
    CHECK_STR(x.data, "/tmp/fresh/data/dxlauncher");
    CHECK_STR(x.state, "/tmp/fresh/state/dxlauncher");
    CHECK_STR(x.cache, "/tmp/fresh/cache/dxlauncher");
    dxl_xdg_free(&x);
    /* A relative one, from the working folder. */
    dxl_env_set("DXL_HOME", "scratch/home");
    char *cwd = dxl_fs_cwd();
    CHECK(cwd != NULL);
    CHECK_INT(dxl_xdg_open(&x, NULL), 0);
    if (cwd) {
        char want[4200];
        snprintf(want, sizeof want, "%s%sscratch/home/config/dxlauncher", cwd,
                 cwd[strlen(cwd) - 1] == '/' ? "" : "/");
        CHECK_STR(x.config, want);
        free(cwd);
    }
    dxl_xdg_free(&x);
    clear_env();
}

TEST_MAIN_BEGIN
    RUN(test_defaults);
    RUN(test_variables);
    RUN(test_scratch_home);
TEST_MAIN_END
