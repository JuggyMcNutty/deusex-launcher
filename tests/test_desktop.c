#include "test.h"
#include "scratch.h"
#include "core/paths.h"
#include "platform/desktop.h"

#ifndef DXL_DATA
#define DXL_DATA "data"
#endif

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

/* The entry the app ships, installed: deusex:// links its own, a link
 * handed to the launcher as its first word (%u); with mime_cache, the
 * links' handler in the applications folder's MimeType cache, which
 * xdg-open and GIO read, and gone with the entry. */
static void links(int mime_cache) {
    char *home = dxl_xstrdup(scratch_dir("desktop-links"));
    char *share = dxl_path_join(home, "share");
    char *tmpl = dxl_path_join(DXL_DATA, DXL_APP_ID ".desktop.in");
    dxl_err err;
    CHECK_INT(dxl_desktop_install(share, tmpl, "/a b/deusex-launcher", &err), 0);
    char *path = dxl_desktop_entry_path(share);
    char *text = slurp(path, NULL);
    CHECK(text && strstr(text, "\nMimeType=x-scheme-handler/deusex;\n") != NULL);
    CHECK(text && strstr(text, "\nExec=\"/a b/deusex-launcher\" %u\n") != NULL);
    free(text);
    char *cache = dxl_path_join(share, "applications/mimeinfo.cache");
    if (mime_cache) {
        text = slurp(cache, NULL);
        CHECK(text && strstr(text, "\nx-scheme-handler/deusex=" DXL_APP_ID ".desktop;") != NULL);
        free(text);
        CHECK_INT(dxl_desktop_remove(share, &err), 0);
        text = slurp(cache, NULL);
        CHECK(!text || !strstr(text, "x-scheme-handler/deusex="));
        free(text);
    }
    free(cache);
    free(path);
    free(tmpl);
    free(share);
    scratch_remove(home);
    free(home);
}

static void test_links(void) { links(0); }
static void test_mime_cache(void) { links(1); }

TEST_MAIN_BEGIN
    /* --mime-cache: the MimeType cache, a test of its own, skipped where the
     * desktop has no update-desktop-database to make it. */
    if (argc > 1 && !strcmp(argv[1], "--mime-cache")) {
        if (system("command -v update-desktop-database >/dev/null 2>&1") != 0) {
            printf("skipped: no update-desktop-database here\n");
            return 77;
        }
        RUN(test_mime_cache);
    } else {
        RUN(test_exec);
        RUN(test_install);
        RUN(test_links);
    }
TEST_MAIN_END
