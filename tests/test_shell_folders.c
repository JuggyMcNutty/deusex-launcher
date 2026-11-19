/* The folder browser (screens/dialogs.c): a folder's folders, and links to
 * them, sorted, the dot folders with hidden alone; with the window's worker
 * (start_list) the folder handed to it and the dialog reading until its
 * answer comes, an answer for a folder left since dropped (bugs.md LUI.17). */
#include "scratch.h"
#include "screens/shell.h"

#ifndef _WIN32
#include <unistd.h>
#endif

static dxl_shell s;
static char asked[1024];
static int asked_hidden;

static int capture(dxl_shell *sh, const char *dir, int hidden) {
    (void)sh;
    snprintf(asked, sizeof asked, "%s", dir);
    asked_hidden = hidden;
    return 0;
}

static void test_read(void) {
    const char *dir = scratch_dir("folders");
    scratch_write(dir, "beta/x.txt", "x");
    scratch_write(dir, "Alpha/x.txt", "x");
    scratch_write(dir, ".dots/x.txt", "x");
    scratch_write(dir, "file.txt", "x");
    char *target = dxl_path_join(dir, "beta");
    int folders = 2;
#ifndef _WIN32
    /* A link to a folder is one. */
    char *link = dxl_path_join(dir, "gamma");
    CHECK_INT(symlink(target, link), 0);
    free(link);
    folders = 3;
#endif
    char *real, **names;
    int count;
    dxl_browser_read(dir, 0, &real, &names, &count);
    CHECK_INT(count, folders);
    if (count >= 2) {
        CHECK_STR(names[0], "Alpha");
        CHECK_STR(names[1], "beta");
    }
    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
    free(real);
    dxl_browser_read(dir, 1, &real, &names, &count);
    CHECK_INT(count, folders + 1);
    for (int i = 0; i < count; i++) free(names[i]);
    free(names);
    free(real);

    /* Offscreen, no worker: read at once. */
    dxl_shell_init(&s, "test");
    dxl_browser_open(&s, dir, DXL_BROWSE_GAME);
    CHECK_INT(s.browser.reading, 0);
    CHECK_INT(s.browser.count, folders);

    /* The window's worker: the folder handed to it, none read here. */
    s.start_list = capture;
    dxl_browser_open(&s, target, DXL_BROWSE_GAME);
    CHECK_STR(asked, target);
    CHECK_INT(s.browser.reading, 1);
    CHECK_INT(s.browser.count, 0);
    /* An answer for a folder left since: dropped. */
    char **late = dxl_xmalloc(sizeof *late);
    late[0] = dxl_xstrdup("late");
    dxl_browser_listed(&s, dir, 0, dxl_xstrdup(dir), late, 1);
    CHECK_INT(s.browser.reading, 1);
    CHECK_INT(s.browser.count, 0);
    /* Its own: taken. */
    dxl_browser_read(target, asked_hidden, &real, &names, &count);
    dxl_browser_listed(&s, target, asked_hidden, real, names, count);
    CHECK_INT(s.browser.reading, 0);
    CHECK_INT(s.browser.count, 0);
    CHECK(s.browser.dir && !strcmp(s.browser.dir, real));
    /* The browser closed: an answer comes to nothing. */
    dxl_browser_open(&s, dir, DXL_BROWSE_GAME);
    s.dialog = DXL_DIALOG_NONE;
    dxl_browser_read(dir, 0, &real, &names, &count);
    dxl_browser_listed(&s, dir, 0, real, names, count);
    CHECK_INT(s.browser.count, 0);

    dxl_shell_free(&s);
    free(target);
    CHECK_INT(dxl_fs_remove_tree(dir), 0);
}

TEST_MAIN_BEGIN
    RUN(test_read);
TEST_MAIN_END
