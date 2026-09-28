#include "test.h"
#include "scratch.h"
#include "core/renderdev.h"

/* The Renderer page's list: dx-reverse-info/wizard.md "Renderer". The .int
 * files here are stand-ins with the lines that matter. */

static char sys[300];

static void setup(void) {
    const char *dir = scratch_dir("rendev");
    snprintf(sys, sizeof sys, "%s/System", dir);
    scratch_write(dir, "System/D3DDrv.int",
        "[Public]\r\nObject=(Name=D3DDrv.D3DRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=)\r\n"
        "Preferences=(Caption=\"Direct3D support\",Parent=\"Rendering\")\r\n"
        "\r\n[D3DRenderDevice]\r\nClassCaption=\"Direct3D Support\"\r\n");
    scratch_write(dir, "System/GlideDrv.int",
        "[Public]\r\nObject=(Name=GlideDrv.GlideRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=Glide2X.dll)\r\n"
        "[GlideRenderDevice]\r\nClassCaption=\"3dfx Glide for Windows\"\r\n");
    scratch_write(dir, "System/OpenGlDrv.int",
        "[Public]\r\nObject=(Name=OpenGLDrv.OpenGLRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=)\r\n"
        "[OpenGLRenderDevice]\r\nClassCaption=\"OpenGL Support\"\r\n");
    scratch_write(dir, "System/SGLDrv.int",   /* commented out, as the game's is */
        "[Public]\r\n//Object=(Name=SGLDrv.SGLRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=SGL.dll)\r\n"
        "[SGLRenderDevice]\r\nClassCaption=\"PowerVR SGL for Windows\"\r\n");
    scratch_write(dir, "System/SoftDrv.int",
        "[Public]\r\nObject=(Name=SoftDrv.SoftwareRenderDevice,Class=Class,MetaClass=Engine.RenderDevice)\r\n"
        "[SoftwareRenderDevice]\r\nClassCaption=\"Software Rendering\"\r\n");
    scratch_write(dir, "System/Galaxy.int",   /* not a render device */
        "[Public]\r\nObject=(Name=Galaxy.GalaxyAudioSubsystem,Class=Class,MetaClass=Engine.AudioSubsystem)\r\n");
    scratch_write(dir, "System/Startup.int",
        "[Descriptions]\r\nD3DDrv.D3DRenderDevice=Use Direct3D hardware rendering.\r\n");
    scratch_write(dir, "Windows/system/glide2x.dll", "x");
}

static dxl_ini *game(const char *text) { return dxl_ini_parse(text, strlen(text)); }

/* The shown captions, joined by "|", and the chosen one. */
static void shown(const dxl_renderdev_list *l, int all, char *out, size_t n, int *sel) {
    size_t idx[16];
    size_t k = dxl_renderdev_shown(l, all, idx, 16, sel);
    out[0] = '\0';
    for (size_t i = 0; i < k; i++) {
        if (i) strncat(out, "|", n - strlen(out) - 1);
        strncat(out, l->items[idx[i]].caption, n - strlen(out) - 1);
    }
}

static void test_registry(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    dxl_renderdev_list l;
    dxl_renderdev_load(&l, sys, loc, NULL, NULL);
    /* Name order of the files: D3DDrv, GlideDrv, OpenGlDrv, SoftDrv. */
    CHECK_INT(l.count, 4);
    if (l.count == 4) {
        CHECK_STR(l.items[0].path, "D3DDrv.D3DRenderDevice");
        CHECK_STR(l.items[0].caption, "Direct3D Support");
        CHECK_STR(l.items[1].autodetect, "Glide2X.dll");
        CHECK_STR(l.items[3].path, "SoftDrv.SoftwareRenderDevice");
        CHECK_INT(l.items[3].priority, 1);
    }
    dxl_renderdev_free(&l);
    dxl_loc_free(loc);
}

/* Detection certified Direct3D: it and the software renderer are shown, and
 * Direct3D is chosen -- the original under wine did the same. */
static void test_certified_device(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    dxl_ini *ini = game("[D3DDrv.D3DRenderDevice]\r\nDescFlags=1\r\n");
    dxl_renderdev_list l;
    dxl_renderdev_load(&l, sys, loc, ini, NULL);
    char s[256];
    int sel;
    shown(&l, 0, s, sizeof s, &sel);
    CHECK_STR(s, "Direct3D Support|Software Rendering");
    CHECK_INT(sel, 0);
    /* Show all: sorted by caption, the choice unchanged. */
    shown(&l, 1, s, sizeof s, &sel);
    CHECK_STR(s, "3dfx Glide for Windows|Direct3D Support|OpenGL Support|Software Rendering");
    CHECK_INT(sel, 1);
    dxl_renderdev_free(&l);
    dxl_ini_free(ini);
    dxl_loc_free(loc);
}

/* Here: detection leaves every device incompatible, and there is no Windows
 * folder -- the software renderer is chosen. */
static void test_nothing_detected(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    dxl_ini *ini = game("[D3DDrv.D3DRenderDevice]\r\nDescFlags=2\r\n");
    dxl_renderdev_list l;
    dxl_renderdev_load(&l, sys, loc, ini, NULL);
    char s[256];
    int sel;
    shown(&l, 0, s, sizeof s, &sel);
    CHECK_STR(s, "Software Rendering");
    CHECK_INT(sel, 0);
    shown(&l, 1, s, sizeof s, &sel);
    CHECK_INT(sel, 3);
    dxl_renderdev_free(&l);
    dxl_ini_free(ini);
    dxl_loc_free(loc);
}

/* An Autodetect file in a given folder earns 3, the best. */
static void test_autodetect(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    dxl_ini *ini = game("[D3DDrv.D3DRenderDevice]\r\nDescFlags=1\r\n");
    char windows[320];
    snprintf(windows, sizeof windows, "%s/../Windows/system", sys);
    const char *dirs[] = { windows, NULL };
    dxl_renderdev_list l;
    dxl_renderdev_load(&l, sys, loc, ini, dirs);
    char s[256];
    int sel;
    shown(&l, 0, s, sizeof s, &sel);
    CHECK_STR(s, "3dfx Glide for Windows|Direct3D Support|Software Rendering");
    CHECK_INT(sel, 0);
    dxl_renderdev_free(&l);
    dxl_ini_free(ini);
    dxl_loc_free(loc);
}

/* Of two equally good, the one registered last is chosen. */
static void test_tie_goes_to_the_last(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    dxl_ini *ini = game("[D3DDrv.D3DRenderDevice]\r\nDescFlags=1\r\n"
                        "[OpenGLDrv.OpenGLRenderDevice]\r\nDescFlags=1\r\n");
    dxl_renderdev_list l;
    dxl_renderdev_load(&l, sys, loc, ini, NULL);
    char s[256];
    int sel;
    shown(&l, 0, s, sizeof s, &sel);
    CHECK_STR(s, "Direct3D Support|OpenGL Support|Software Rendering");
    CHECK_INT(sel, 1);
    dxl_renderdev_free(&l);
    dxl_ini_free(ini);
    dxl_loc_free(loc);
}

static void test_note(void) {
    dxl_loc *loc = dxl_loc_open(sys, NULL);
    CHECK_STR(dxl_renderdev_note(loc, "D3DDrv.D3DRenderDevice"), "Use Direct3D hardware rendering.");
    CHECK_STR(dxl_renderdev_note(loc, "SoftDrv.SoftwareRenderDevice"), "");
    dxl_loc_free(loc);
}

static void test_testrendev(void) {
    scratch_write(sys, "DeusEx.ini", "[Engine.Engine]\r\nCdPath=..\\\r\n");
    dxl_config *c = dxl_config_open(sys, "DeusEx");
    dxl_err e;
    CHECK_INT(dxl_renderdev_testrendev(c, sys, "D3DDrv.D3DRenderDevice", &e), 0);
    dxl_config_free(c);
    c = dxl_config_open(sys, "DeusEx");
    CHECK_INT(dxl_ini_get_int(dxl_config_ini(c), "D3DDrv.D3DRenderDevice", "DescFlags", 0), 2);
    dxl_config_free(c);
    CHECK_INT(scratch_exists(sys, "Detected.ini"), 1);
}

TEST_MAIN_BEGIN
    setup();
    RUN(test_registry);
    RUN(test_certified_device);
    RUN(test_nothing_detected);
    RUN(test_autodetect);
    RUN(test_tie_goes_to_the_last);
    RUN(test_note);
    RUN(test_testrendev);
    scratch_remove(scratch_dir("rendev"));
TEST_MAIN_END
