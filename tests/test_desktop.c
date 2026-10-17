#include "test.h"
#include "scratch.h"
#include "core/paths.h"
#include "platform/desktop.h"

/* Exec as the spec has it: quoted when a reserved character is in it, ",
 * `, $ and \ escaped inside the quotes, then every backslash doubled for
 * the string type; % doubled for the field codes. */
static void test_exec(void) {
    char *e = dxl_desktop_exec("/opt/dxlauncher/deusex-launcher");
    CHECK_STR(e, "/opt/dxlauncher/deusex-launcher");
    free(e);
    e = dxl_desktop_exec("/home/a b/DXLauncher/deusex-launcher");
    CHECK_STR(e, "\"/home/a b/DXLauncher/deusex-launcher\"");
    free(e);
    e = dxl_desktop_exec("/home/x/$money/\"q\"/back\\slash/deusex-launcher");
    CHECK_STR(e, "\"/home/x/\\\\$money/\\\\\"q\\\\\"/back\\\\\\\\slash/deusex-launcher\"");
    free(e);
    e = dxl_desktop_exec("/home/x/100%/deusex-launcher");
    CHECK_STR(e, "/home/x/100%%/deusex-launcher");
    free(e);
}

static void test_install(void) {
    char *home = dxl_xstrdup(scratch_dir("desktop"));
    scratch_write(home, "data/entry.desktop.in", "[Desktop Entry]\nName=DXLauncher\nExec=@EXEC@\nTryExec=@EXEC@\n");
    char *tmpl = dxl_path_join(home, "data/entry.desktop.in");
    char *share = dxl_path_join(home, "share");
    dxl_err err;
    CHECK_INT(dxl_desktop_installed(share), 0);
    CHECK_INT(dxl_desktop_install(share, tmpl, "/a b/deusex-launcher", &err), 0);
    CHECK_INT(dxl_desktop_installed(share), 1);
    char *path = dxl_desktop_entry_path(share);
    CHECK(strstr(path, "/applications/io.github.juggymcnutty.DXLauncher.desktop") != NULL);
    FILE *f = fopen(path, "r");
    char text[256] = "";
    if (f) {
        size_t n = fread(text, 1, sizeof text - 1, f);
        text[n] = '\0';
        fclose(f);
    }
    CHECK_STR(text, "[Desktop Entry]\nName=DXLauncher\nExec=\"/a b/deusex-launcher\"\nTryExec=\"/a b/deusex-launcher\"\n");
    char *icon = dxl_desktop_icon_path(share, 64);
    CHECK(strstr(icon, "/icons/hicolor/64x64/apps/io.github.juggymcnutty.DXLauncher.png") != NULL);
    CHECK_INT(dxl_desktop_remove(share, &err), 0);
    CHECK_INT(dxl_desktop_installed(share), 0);
    CHECK_INT(dxl_desktop_remove(share, &err), 0);   /* nothing to remove: fine */
    CHECK(dxl_desktop_install(share, "/nonexistent/template", "/x", &err) != 0);
    free(icon);
    free(path);
    free(share);
    free(tmpl);
    scratch_remove(home);
    free(home);
}

TEST_MAIN_BEGIN
    RUN(test_exec);
    RUN(test_install);
TEST_MAIN_END
