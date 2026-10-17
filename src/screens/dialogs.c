/* The dialogs: the folder browser, which a pad works as well as a mouse
 * does, and a message. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/paths.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- the folder browser ---------------------------------------------- */

void dxl_browser_free(dxl_browser *b) {
    for (int i = 0; i < b->count; i++) free(b->names[i]);
    free(b->names);
    free(b->dir);
    b->names = NULL;
    b->dir = NULL;
    b->count = 0;
}

static int by_name(const void *a, const void *b) {
    return dxl_stricmp(*(char *const *)a, *(char *const *)b);
}

/* The folders in dir, sorted. */
static void list(dxl_browser *b, const char *dir) {
    int hidden = b->hidden;
    char *keep = dxl_xstrdup(dir);
    dxl_browser_free(b);
    b->hidden = hidden;
    char real[PATH_MAX];
    b->dir = dxl_xstrdup(realpath(keep, real) ? real : keep);
    free(keep);
    DIR *d = opendir(b->dir);
    if (!d) return;
    int cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (e->d_name[0] == '.' && !b->hidden) continue;
        if (e->d_type != DT_DIR) {
            if (e->d_type != DT_LNK && e->d_type != DT_UNKNOWN) continue;
            char *p = dxl_path_join(b->dir, e->d_name);
            int dir_too = dxl_path_is_dir(p);
            free(p);
            if (!dir_too) continue;
        }
        if (b->count == cap) {
            cap = cap ? cap * 2 : 32;
            b->names = dxl_xrealloc(b->names, sizeof *b->names * cap);
        }
        b->names[b->count++] = dxl_xstrdup(e->d_name);
    }
    closedir(d);
    if (b->count) qsort(b->names, b->count, sizeof *b->names, by_name);
}

void dxl_browser_open(dxl_shell *s, const char *dir) {
    s->browser.note[0] = '\0';
    list(&s->browser, dir && *dir ? dir : "/");
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog = DXL_DIALOG_BROWSER;
}

/* Into another folder: the list from its top. */
static void go_to(dxl_shell *s, dxl_list *l, const char *dir) {
    s->browser.note[0] = '\0';
    list(&s->browser, dir);
    *dxl_ui_value(&s->ui, l->id, 0) = 0;
    l->count = s->browser.count + (strcmp(s->browser.dir, "/") != 0);
    dxl_list_focus(&s->ui, l, 0);
    dxl_ui_forget(&s->ui);
}

/* The folder shown, as the game's: it, or the folder above when this is
 * the game's System. */
static void use(dxl_shell *s) {
    dxl_browser *b = &s->browser;
    dxl_install in;
    if (dxl_install_check(b->dir, &in) != 0 && !in.found && dxl_stricmp(dxl_path_basename(b->dir), "System") == 0) {
        char *up = dxl_path_dirname(b->dir);
        dxl_install_free(&in);
        dxl_install_check(up, &in);
        free(up);
    }
    if (in.ok) {
        dxl_shell_close_dialog(s);
        dxl_shell_choose(s, &in);
    } else if (in.found) {
        snprintf(b->note, sizeof b->note, "%s", in.problem);
    } else {
        snprintf(b->note, sizeof b->note, "Not the game's folder: there is no System/DeusEx.exe in it.");
    }
    dxl_install_free(&in);
}

static void browser(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_browser *b = &s->browser;
    char shown[PATH_MAX + 2];

    dxl_rect in = dxl_dialog_begin(ui, over, 840, 600, "The game's folder");
    dxl_label(ui, dxl_cut_top(&in, 28), DXL_FONT_BODY, dxl_shell_tilde(b->dir, shown, sizeof shown), t->text_focus,
              DXL_W_LEFT | DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_cut_top(&in, 8);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 12);
    if (b->note[0]) {
        dxl_label(ui, dxl_cut_bottom(&in, 24), DXL_FONT_SMALL, b->note, t->warn, DXL_W_LEFT | DXL_W_MAY_CUT);
        dxl_cut_bottom(&in, 6);
    }

    dxl_well(ui, in);
    int up = strcmp(b->dir, "/") != 0;
    int rows = b->count + up;
    dxl_list l;
    dxl_list_begin(ui, &l, "browser", dxl_inset(in, 4), rows, 36);
    int open = -1;
    for (int i = 0; i < rows; i++) {
        const char *name = up && i == 0 ? ".. (the folder above)" : b->names[i - up];
        if (dxl_list_row(ui, &l, i, name, NULL, t->help, DXL_W_MAY_CUT) & DXL_HIT_ACTIVATED) open = i;
    }
    dxl_list_end(ui, &l);
    if (!b->count) {
        dxl_rect empty = dxl_inset(in, 16);
        empty.y += up ? 40 : 0;
        dxl_label(ui, (dxl_rect){ empty.x, empty.y, empty.w, 24 }, DXL_FONT_SMALL, "No folders in it.", t->help,
                  DXL_W_LEFT);
    }

    float bw = (buttons.w - 3 * 10) / 4;
    int ok = dxl_button(ui, "browser use", dxl_cut_left(&buttons, bw), "Use this folder", DXL_W_PRIMARY);
    dxl_cut_left(&buttons, 10);
    int home = dxl_button(ui, "browser home", dxl_cut_left(&buttons, bw), "Home folder", 0);
    dxl_cut_left(&buttons, 10);
    int dots = dxl_button(ui, "browser hidden", dxl_cut_left(&buttons, bw),
                          b->hidden ? "Hide dot folders" : "Show dot folders", 0);
    dxl_cut_left(&buttons, 10);
    int cancel = dxl_button(ui, "browser cancel", buttons, "Cancel", 0);
    dxl_dialog_end(ui);

    if (open >= 0) {
        char *to = up && open == 0 ? dxl_path_dirname(b->dir) : dxl_path_join(b->dir, b->names[open - up]);
        go_to(s, &l, to);
        free(to);
    } else if (ok) {
        use(s);
    } else if (home) {
        const char *h = getenv("HOME");
        go_to(s, &l, h && *h ? h : "/");
    } else if (dots) {
        b->hidden = !b->hidden;
        char *keep = dxl_xstrdup(b->dir);
        list(b, keep);
        free(keep);
    } else if (cancel || back) {
        dxl_shell_close_dialog(s);
    }
}

/* ---- a message ---------------------------------------------------------- */

static void message(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    float w = 620;
    if (w > over.w - 32) w = over.w - 32;
    float text = dxl_text_size(ui, DXL_FONT_BODY, s->message, w - 44).y;
    float h = 5 + 32 + 16 + text + 16 + 46 + 20;
    dxl_rect in = dxl_dialog_begin(ui, over, w, h, s->message_title);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_paragraph(ui, in, DXL_FONT_BODY, s->message, t->text);
    int ok = dxl_button(ui, "message ok", dxl_cut_right(&buttons, 150), "OK", DXL_W_PRIMARY);
    dxl_dialog_end(ui);
    if (ok || back) dxl_shell_close_dialog(s);
}

void dxl_dialogs(dxl_shell *s, dxl_rect r, int back) {
    switch (s->dialog) {
    case DXL_DIALOG_BROWSER: browser(s, r, back); break;
    case DXL_DIALOG_MESSAGE: message(s, r, back); break;
    default: break;
    }
}
