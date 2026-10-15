#include "test.h"
#include "core/ini.h"
#include "core/localize.h"
#include "core/renderdev.h"
#include "core/peicon.h"

#include <unistd.h>

/* The checks tests/fixtures can only stand in for, run against a real install
 * in gamefiles/System: the byte-identical round trip, and the values
 * dx-reverse-info/ describes. The game's files are not part of this
 * repository, so without an install there this test is skipped (exit 77). */

#ifndef DXL_GAMEFILES
#define DXL_GAMEFILES "gamefiles/System"
#endif

static char *game_file(const char *name) {
    size_t n = strlen(DXL_GAMEFILES) + strlen(name) + 2;
    char *p = malloc(n);
    snprintf(p, n, "%s/%s", DXL_GAMEFILES, name);
    return p;
}

/* DeusEx.ini and User.ini only exist once the game has run; the defaults it
 * builds them from always do. */
static void test_roundtrip(void) {
    const char *names[] = { "Default.ini", "DefUser.ini", "DeusEx.ini", "User.ini" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        char *path = game_file(names[i]);
        size_t orig_len = 0;
        char *orig = slurp(path, &orig_len);
        if (!orig && i >= 2) { free(path); continue; }
        CHECK(orig != NULL);
        dxl_ini *ini = orig ? dxl_ini_load(path, NULL) : NULL;
        if (ini) {
            size_t out_len = 0;
            char *out = dxl_ini_render(ini, &out_len);
            CHECK_INT(out_len, orig_len);
            CHECK(out_len == orig_len && memcmp(out, orig, orig_len) == 0);
            free(out);
            dxl_ini_free(ini);
        }
        free(orig);
        free(path);
    }
}

static void test_default_ini_matches_the_spec(void) {
    char *path = game_file("Default.ini");
    dxl_ini *ini = dxl_ini_load(path, NULL);
    free(path);
    CHECK(ini != NULL);
    if (!ini) return;

    /* dx-reverse-info/wizard.md: a pristine install ships FirstRun=0, so the
     * first-time flow always runs. */
    CHECK_INT(dxl_ini_get_int(ini, "FirstRun", "FirstRun", -1), 0);
    /* dx-reverse-info/launch-flow.md section 8: CdPath=..\ makes the CD check pass. */
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "CdPath"), "..\\");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameEngine"), "DeusEx.DeusExGameEngine");
    CHECK_STR(dxl_ini_get(ini, "Engine.Engine", "GameRenderDevice"),
              "GlideDrv.GlideRenderDevice");
    CHECK_INT(dxl_ini_get_all(ini, "Core.System", "Paths", NULL, 0), 5);

    /* dx-reverse-info/ini-keys.md: DescFlags and Description are runtime values that
     * appear in no shipped ini. If these ever start existing, the detection
     * story in the docs is wrong. */
    CHECK(dxl_ini_get(ini, "D3DDrv.D3DRenderDevice", "Description") == NULL);
    CHECK(dxl_ini_get(ini, "D3DDrv.D3DRenderDevice", "DescFlags") == NULL);
    dxl_ini_free(ini);
}

/* The words the pages show, from the game's own .int files. */
static void test_strings(void) {
    dxl_loc *l = dxl_loc_open(DXL_GAMEFILES, NULL);
    CHECK_STR(dxl_loc_general(l, "Startup", "FirstTime"), "Deus Ex First-Time Configuration");
    CHECK_STR(dxl_loc_general(l, "Startup", "Run"), "Run!");
    CHECK_STR(dxl_loc_general(l, "Startup", "WorldHigh"), "High detail textures");   /* unquoted */
    CHECK_STR(dxl_loc_general(l, "Window", "NextButton"), "&Next >");
    CHECK_STR(dxl_loc_general(l, "Window", "InsertCdTitle"), "Cd Required At Startup");
    CHECK_STR(dxl_loc_get(l, "Startup", "IDDIALOG_ConfigPageRenderer", "IDC_RenderNote", 0), "");
    dxl_loc_free(l);
}

/* The Renderer page's registry: the five devices the game registers, in the
 * order the original met them under wine (dx-reverse-info/wizard.md, "Observed under wine"). */
static void test_render_devices(void) {
    dxl_loc *l = dxl_loc_open(DXL_GAMEFILES, NULL);
    dxl_renderdev_list r;
    dxl_renderdev_load(&r, DXL_GAMEFILES, l, NULL, NULL);
    CHECK_INT(r.count, 5);
    size_t idx[8];
    int sel;
    size_t n = dxl_renderdev_shown(&r, 1, idx, 8, &sel);
    const char *want[] = { "3dfx Glide for Windows", "Direct3D Support", "OpenGL Support",
                           "S3 MeTaL for Windows", "Software Rendering" };
    CHECK_INT(n, 5);
    for (size_t i = 0; i < n && i < 5; i++) CHECK_STR(r.items[idx[i]].caption, want[i]);
    /* Nothing certified: the software renderer. */
    CHECK_INT(sel, 4);
    dxl_renderdev_free(&r);
    dxl_loc_free(l);
}

/* dx-reverse-info/wizard.md, the frame: the wizard's icon is DeusEx.exe's
 * group 128, the only one it has. For 32 its 32x32 of 256 colours is taken,
 * square and every pixel in, a near-black corner and a blue centre; for 16
 * its 16x16 of 16. */
static void test_exe_icon(void) {
    char *path = game_file("DeusEx.exe");
    dxl_icon icon;
    CHECK_INT(dxl_pe_icon(path, 128, 32, &icon, NULL), 0);
    CHECK_INT(icon.w, 32);
    CHECK_INT(icon.h, 32);
    if (icon.argb) {
        CHECK_INT(icon.argb[0], 0xff040404);
        CHECK_INT(icon.argb[16 * 32 + 16], 0xff3333cc);
    }
    dxl_icon_free(&icon);
    CHECK_INT(dxl_pe_icon(path, 128, 16, &icon, NULL), 0);
    CHECK_INT(icon.w, 16);
    if (icon.argb) CHECK_INT(icon.argb[8 * 16 + 8], 0xff000080);
    dxl_icon_free(&icon);
    CHECK_INT(dxl_pe_icon(path, 129, 32, &icon, NULL), -1);
    CHECK(icon.argb == NULL);
    free(path);
}

TEST_MAIN_BEGIN
    if (access(DXL_GAMEFILES "/Default.ini", R_OK) != 0) {
        printf("skipped: no Deus Ex install in %s\n", DXL_GAMEFILES);
        return 77;
    }
    RUN(test_roundtrip);
    RUN(test_default_ini_matches_the_spec);
    RUN(test_strings);
    RUN(test_render_devices);
    RUN(test_exe_icon);
TEST_MAIN_END
