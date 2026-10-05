#include "test.h"
#include "scratch.h"
#include "wizard/wizard.h"

/* The wizard, clicked through with no display: dx-reverse-info/wizard.md.
 * The .int files here are stand-ins with the strings that matter. Skipped
 * (77) where no font can be found to draw with. */

static char sys[300];
static dxl_gui gui;

static void setup(void) {
    const char *dir = scratch_dir("wizard");
    snprintf(sys, sizeof sys, "%s/System", dir);
    scratch_write(dir, "System/Startup.int",
        "[General]\r\nRun=Run!\r\nFirstTime=First run\r\nSafeMode=Safe mode\r\nVideo=Video\r\n"
        "Detecting=Detecting...\r\nWebPage=http://example.invalid/\r\n"
        "SoundLow=Low sound\r\nSoundHigh=High sound\r\nSkinsLow=Low skins\r\nSkinsHigh=High skins\r\n"
        "WorldLow=Low world\r\nWorldHigh=\"High world\"\r\nResHigh=Standard res\r\n"
        "[Descriptions]\r\nD3DDrv.D3DRenderDevice=The Direct3D note.\r\n"
        "SoftDrv.SoftwareRenderDevice=The software note.\r\n"
        "[IDDIALOG_ConfigPageRenderer]\r\nIDC_RenderPrompt=Pick a device.\r\nIDC_RenderNote=\r\n"
        "IDC_Compatible=Compatible\r\nIDC_All=All\r\n"
        "[IDDIALOG_ConfigPageDetail]\r\nIDC_DetailPrompt=Chosen:\r\nIDC_DetailNote=Change later.\r\n"
        "[IDDIALOG_ConfigPageFirstTime]\r\nIDC_Prompt=Starting.\r\n"
        "[IDDIALOG_ConfigPageSafeMode]\r\nIDC_SafeModePrompt=Not shut down properly.\r\n"
        "IDC_Run=Run\r\nIDC_Video=Video\r\nIDC_SafeMode=Safe\r\nIDC_Web=Web\r\n"
        "[IDDIALOG_ConfigPageSafeOptions]\r\nIDC_SafeOptions=Options\r\nIDC_NoSound=No sound\r\n"
        "IDC_No3DSound=No 3D sound\r\nIDC_No3DVideo=No 3D video\r\nIDC_Window=Window\r\n"
        "IDC_Res=640x480\r\nIDC_ResetConfig=Reset\r\nIDC_NoProcessor=No MMX\r\nIDC_NoJoy=No joystick\r\n"
        "[IDDIALOG_ConfigPageDriver]\r\nIDC_DriverText=Your card:\r\nIDC_Card=Unknown\r\n"
        "IDC_DriverInfo=Line one.\\n\\nLine two.\r\nIDC_Web=Links:\r\nIDC_WebButton=Information && Drivers\r\n");
    scratch_write(dir, "System/Window.int",
        "[General]\r\nBackButton=< &Back\r\nNextButton=&Next >\r\nCancelButton=Cancel\r\n");
    scratch_write(dir, "System/D3DDrv.int",
        "[Public]\r\nObject=(Name=D3DDrv.D3DRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=)\r\n"
        "[D3DRenderDevice]\r\nClassCaption=\"Direct3D Support\"\r\n");
    scratch_write(dir, "System/GlideDrv.int",
        "[Public]\r\nObject=(Name=GlideDrv.GlideRenderDevice,Class=Class,MetaClass=Engine.RenderDevice,Autodetect=Glide2X.dll)\r\n"
        "[GlideRenderDevice]\r\nClassCaption=\"3dfx Glide for Windows\"\r\n");
    scratch_write(dir, "System/SoftDrv.int",
        "[Public]\r\nObject=(Name=SoftDrv.SoftwareRenderDevice,Class=Class,MetaClass=Engine.RenderDevice)\r\n"
        "[SoftwareRenderDevice]\r\nClassCaption=\"Software Rendering\"\r\n");
}

/* The game's ini afresh: Direct3D detected as certified, with a card. */
static void write_ini(void) {
    scratch_write(scratch_dir("wizard"), "System/DeusEx.ini",
        "[Engine.Engine]\r\nGameRenderDevice=SoftDrv.SoftwareRenderDevice\r\n"
        "[D3DDrv.D3DRenderDevice]\r\nDescFlags=1\r\nDescription=Test Card 9000\r\n");
}

typedef struct {
    dxl_config        *cfg;
    dxl_loc           *loc;
    dxl_wizard_request req;
    dxl_wizard        *w;
    dxl_dialog        *d;
} fixture_t;

static void open_wizard(fixture_t *f, dxl_page_id first) {
    write_ini();
    memset(f, 0, sizeof *f);
    f->cfg = dxl_config_open(sys, "DeusEx");
    f->loc = dxl_loc_open(sys, NULL);
    f->req.config = f->cfg;
    f->req.loc = f->loc;
    f->req.system_dir = sys;
    f->req.package = "DeusEx";
    f->req.exe_path = "/nonexistent/DeusEx";
    f->req.cmdline = "";
    f->req.title = "Test";
    f->req.machine.memory = 512ULL << 20;
    f->req.machine.mmx = 1;
    f->req.machine.cpu_mhz = 3000;
    dxl_err e;
    f->w = dxl_wizard_new(&gui, &f->req, 1, &e);
    CHECK(f->w != NULL);
    f->d = dxl_wizard_dialog(f->w);
    dxl_wizard_push(f->w, first);
}

static void close_wizard(fixture_t *f) {
    dxl_wizard_free(f->w);
    dxl_loc_free(f->loc);
    dxl_config_free(f->cfg);
    free(f->req.relaunch_flags);
}

static dxl_ctl *page(fixture_t *f, int id) { return dxl_dialog_find(f->d, id, 1); }
static dxl_ctl *frame(fixture_t *f, int id) { return dxl_dialog_find(f->d, id, 0); }

static const char *items(dxl_ctl *list, char *buf, size_t n) {
    buf[0] = '\0';
    for (int i = 0; i < list->item_count; i++) {
        if (i) strncat(buf, "|", n - strlen(buf) - 1);
        strncat(buf, list->items[i], n - strlen(buf) - 1);
    }
    return buf;
}

/* Where the templates put things, in pixels: as the original's show under
 * wine (dx-reverse-info/wizard.md, "Observed under wine"). */
static void test_layout(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_RENDERER);
    CHECK_INT(f.d->w, 524);
    CHECK_INT(f.d->h, 411);
    dxl_ctl *next = frame(&f, 1004);
    CHECK_INT(next->rc.x, 183); CHECK_INT(next->rc.y, 384);
    CHECK_INT(next->rc.w, 75);  CHECK_INT(next->rc.h, 23);
    dxl_ctl *list = page(&f, 1103);
    CHECK_INT(list->rc.x, 16);  CHECK_INT(list->rc.y, 164);
    CHECK_INT(list->rc.w, 495); CHECK_INT(list->rc.h, 109);
    dxl_ctl *all = page(&f, 1110);
    CHECK_INT(all->rc.x, 290);  CHECK_INT(all->rc.y, 275);
    CHECK_INT(all->rc.w, 221);  CHECK_INT(all->rc.h, 16);
    close_wizard(&f);
}

/* First run: Renderer, detecting, then listed; Direct3D chosen, so the
 * Driver page; Detail; FirstTime, whose Run! ends the wizard with 1. */
static void test_first_run(void) {
    fixture_t f;
    char buf[256];
    open_wizard(&f, DXL_PAGE_RENDERER);
    CHECK(!frame(&f, 3)->visible);                         /* no Back on the first page */
    CHECK_STR(frame(&f, 1004)->text, "&Next >");
    CHECK_STR(frame(&f, 2)->text, "Cancel");
    CHECK(!frame(&f, 1005)->visible);                      /* Finish never shows */
    CHECK_STR(items(page(&f, 1103), buf, sizeof buf), "Detecting...");
    CHECK(page(&f, 1109)->checked);                        /* compatible devices */

    dxl_wizard_first_paint(f.w, 0);
    /* Direct3D is certified; the software renderer always shows; Glide's
     * Autodetect file is nowhere. Sorted, the best chosen. */
    CHECK_STR(items(page(&f, 1103), buf, sizeof buf), "Direct3D Support|Software Rendering");
    CHECK_INT(page(&f, 1103)->selected, 0);
    CHECK_STR(page(&f, 1104)->text, "The Direct3D note.");

    dxl_dialog_click(f.d, 1110);                           /* all devices */
    CHECK_STR(items(page(&f, 1103), buf, sizeof buf),
              "3dfx Glide for Windows|Direct3D Support|Software Rendering");
    dxl_dialog_click(f.d, 1109);
    CHECK_STR(items(page(&f, 1103), buf, sizeof buf), "Direct3D Support|Software Rendering");

    dxl_dialog_choose(f.d, 1103, 1, 0);                    /* the note follows */
    CHECK_STR(page(&f, 1104)->text, "The software note.");
    dxl_dialog_choose(f.d, 1103, 0, 0);

    dxl_dialog_click(f.d, 1004);
    CHECK_STR(dxl_config_render_device(f.cfg), "D3DDrv.D3DRenderDevice");
    CHECK_STR(page(&f, 1111)->text, "Test Card 9000");     /* the Driver page */
    CHECK_STR(page(&f, 1112)->text, "Line one.\n\nLine two.");
    CHECK_STR(page(&f, 1113)->text, "Information && Drivers");
    CHECK(frame(&f, 3)->visible);

    dxl_dialog_click(f.d, 1004);                           /* Detail */
    CHECK_STR(page(&f, 1100)->text, "High sound\r\nHigh skins\r\nHigh world\r\nStandard res\r\n");
    dxl_dialog_click(f.d, 1004);                           /* FirstTime */
    CHECK_STR(page(&f, 1002)->text, "Starting.");
    CHECK_STR(frame(&f, 1004)->text, "Run!");
    CHECK(!f.d->ended);
    dxl_dialog_click(f.d, 1004);
    CHECK(f.d->ended);
    CHECK_INT(f.d->result, 1);
    close_wizard(&f);
}

/* Software rendering goes straight to Detail; Back returns to the page
 * below as it was left; a double click is Next. */
static void test_software_and_back(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_RENDERER);
    dxl_wizard_first_paint(f.w, 0);
    dxl_dialog_choose(f.d, 1103, 1, 1);                    /* double click: Next */
    CHECK_STR(dxl_config_render_device(f.cfg), "SoftDrv.SoftwareRenderDevice");
    CHECK(page(&f, 1100) != NULL);                         /* Detail's edit */
    CHECK(page(&f, 1111) == NULL);
    dxl_dialog_click(f.d, 3);                              /* Back */
    CHECK(page(&f, 1103) != NULL);
    CHECK_INT(page(&f, 1103)->selected, 1);
    CHECK_STR(page(&f, 1104)->text, "The software note.");
    CHECK(!frame(&f, 3)->visible);
    close_wizard(&f);
}

/* Cancel, anywhere, ends it with 0. */
static void test_cancel(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_RENDERER);
    dxl_wizard_first_paint(f.w, 0);
    dxl_dialog_click(f.d, 1004);
    dxl_dialog_click(f.d, 2);
    CHECK(f.d->ended);
    CHECK_INT(f.d->result, 0);
    close_wizard(&f);
}

/* Safe mode: no Next; Safe mode leads to the options, all ticked but
 * Reset; Run! leaves the flags of the boxes ticked -- each its own -- and
 * ends with 0. */
static void test_safe_options(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_SAFEMODE);
    CHECK(!frame(&f, 1004)->visible);
    CHECK_STR(page(&f, 1105)->text, "Not shut down properly.");
    dxl_dialog_click(f.d, 1109);
    CHECK_STR(frame(&f, 1004)->text, "Run!");
    CHECK(frame(&f, 3)->visible);
    for (int id = 1108; id <= 1115; id++) CHECK_INT(page(&f, id)->checked, id != 1113);
    CHECK_STR(page(&f, 1110)->text, "No 3D video");        /* IDC_No3dVideo finds IDC_No3DVideo */

    dxl_dialog_click(f.d, 1109);                           /* 3D sound */
    dxl_dialog_click(f.d, 1112);                           /* window */
    dxl_dialog_click(f.d, 1004);
    CHECK(f.d->ended);
    CHECK_INT(f.d->result, 0);
    CHECK_STR(f.req.relaunch_flags, " -nosound -nohard -defaultres -nommx -nokni -nok6 -nojoy");
    CHECK(scratch_exists(scratch_dir("wizard"), "System/DeusEx.ini"));
    close_wizard(&f);
}

/* Reset deletes <Package>.ini. */
static void test_reset(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_SAFEMODE);
    dxl_dialog_click(f.d, 1109);
    dxl_dialog_click(f.d, 1113);
    dxl_dialog_click(f.d, 1004);
    CHECK(!scratch_exists(scratch_dir("wizard"), "System/DeusEx.ini"));
    close_wizard(&f);
}

/* Safe mode's Run ends with 1; its Video leads to the Renderer page. */
static void test_safe_mode_buttons(void) {
    fixture_t f;
    open_wizard(&f, DXL_PAGE_SAFEMODE);
    dxl_dialog_click(f.d, 1110);
    CHECK(page(&f, 1103) != NULL);
    CHECK(frame(&f, 3)->visible);
    dxl_dialog_click(f.d, 3);
    dxl_dialog_click(f.d, 1108);
    CHECK(f.d->ended);
    CHECK_INT(f.d->result, 1);
    close_wizard(&f);
}

TEST_MAIN_BEGIN
    dxl_err e;
    if (dxl_gui_init(&gui, 0, &e) != 0) {
        printf("skipped: %s\n", dxl_err_msg(&e));
        return 77;
    }
    setup();
    RUN(test_layout);
    RUN(test_first_run);
    RUN(test_software_and_back);
    RUN(test_cancel);
    RUN(test_safe_options);
    RUN(test_reset);
    RUN(test_safe_mode_buttons);
    dxl_gui_quit(&gui);
    scratch_remove(scratch_dir("wizard"));
TEST_MAIN_END
