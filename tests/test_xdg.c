#include "test.h"
#include "core/xdg.h"

#include <stdlib.h>

static void clear_env(void) {
    unsetenv("DXL_HOME");
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_STATE_HOME");
    unsetenv("XDG_CACHE_HOME");
}

/* Nothing set: the spec's defaults under HOME. */
static void test_defaults(void) {
    clear_env();
    setenv("HOME", "/home/jc", 1);
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
    setenv("HOME", "/home/jc", 1);
    setenv("XDG_CONFIG_HOME", "/cfg", 1);
    setenv("XDG_DATA_HOME", "relative/data", 1);
    setenv("XDG_STATE_HOME", "", 1);
    setenv("XDG_CACHE_HOME", "/var/cache/jc", 1);
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
    setenv("HOME", "/home/jc", 1);
    setenv("XDG_CONFIG_HOME", "/cfg", 1);
    setenv("DXL_HOME", "/tmp/fresh", 1);
    dxl_xdg x;
    CHECK_INT(dxl_xdg_open(&x, NULL), 0);
    CHECK_STR(x.config, "/tmp/fresh/config/dxlauncher");
    CHECK_STR(x.data, "/tmp/fresh/data/dxlauncher");
    CHECK_STR(x.state, "/tmp/fresh/state/dxlauncher");
    CHECK_STR(x.cache, "/tmp/fresh/cache/dxlauncher");
    dxl_xdg_free(&x);
    clear_env();
}

TEST_MAIN_BEGIN
    RUN(test_defaults);
    RUN(test_variables);
    RUN(test_scratch_home);
TEST_MAIN_END
