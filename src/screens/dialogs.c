/* The dialogs: the folder browser, which a pad works as well as a mouse
 * does, and a message. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/fs.h"
#include "ui/osk.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* The tops of the file system (platform/fs.h): "/" alone here, a drive's
 * each on Windows. Over more than one, a root has the list of them above
 * it, a folder of its own by the name "". */
static char *first_root(void) {
    char **roots = dxl_fs_roots();
    char *first = dxl_xstrdup(roots && roots[0] ? roots[0] : "/");
    dxl_fs_roots_free(roots);
    return first;
}

static int is_root(const char *dir) {
    size_t n = dxl_fs_root_len(dir);
    return n && !dir[n];
}

/* Whether dir has a folder above it: not the roots' list, nor the one root. */
static int has_up(const char *dir) {
    if (!dir[0]) return 0;
    if (!is_root(dir)) return 1;
    char **roots = dxl_fs_roots();
    int many = roots && roots[0] && roots[1];
    dxl_fs_roots_free(roots);
    return many;
}

/* The folder above dir: its own, or above a root, the roots' list. */
static char *above(const char *dir) { return is_root(dir) ? dxl_xstrdup("") : dxl_path_dirname(dir); }

/* The folders in dir, sorted; the roots for "". */
static void list(dxl_browser *b, const char *dir) {
    int hidden = b->hidden;
    char *keep = dxl_xstrdup(dir);
    dxl_browser_free(b);
    b->hidden = hidden;
    if (!keep[0]) {
        b->dir = keep;
        char **roots = dxl_fs_roots();
        for (int i = 0; roots && roots[i]; i++) {
            b->names = dxl_xrealloc(b->names, sizeof *b->names * (size_t)(b->count + 1));
            b->names[b->count++] = dxl_xstrdup(roots[i]);
        }
        dxl_fs_roots_free(roots);
        return;
    }
    char *real = dxl_fs_real(keep);
    b->dir = real ? real : dxl_xstrdup(keep);
    free(keep);
    DIR *d = opendir(b->dir);
    if (!d) return;
    int cap = 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        if (e->d_name[0] == '.' && !b->hidden) continue;
        /* A folder, or a link to one. */
        char *p = dxl_path_join(b->dir, e->d_name);
        int folder = dxl_path_is_dir(p);
        free(p);
        if (!folder) continue;
        if (b->count == cap) {
            cap = cap ? cap * 2 : 32;
            b->names = dxl_xrealloc(b->names, sizeof *b->names * cap);
        }
        b->names[b->count++] = dxl_xstrdup(e->d_name);
    }
    closedir(d);
    if (b->count) qsort(b->names, b->count, sizeof *b->names, by_name);
}

void dxl_browser_open(dxl_shell *s, const char *dir, int mode) {
    s->browser.note[0] = '\0';
    s->browser.mode = mode;
    char *top = dir && *dir ? NULL : first_root();
    list(&s->browser, top ? top : dir);
    free(top);
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog = DXL_DIALOG_BROWSER;
}

/* Into another folder: the list from its top. */
static void go_to(dxl_shell *s, dxl_list *l, const char *dir) {
    s->browser.note[0] = '\0';
    list(&s->browser, dir);
    *dxl_ui_value(&s->ui, l->id, 0) = 0;
    l->count = s->browser.count + has_up(s->browser.dir);
    dxl_list_focus(&s->ui, l, 0);
    dxl_ui_forget(&s->ui);
}

/* The folder shown, as the game's: it, or the folder above when this is
 * the game's System; as the engine's; or as a mod's, it or the folder
 * above when this is the mod's System. */
static void use(dxl_shell *s) {
    dxl_browser *b = &s->browser;
    if (b->mode == DXL_BROWSE_ENGINE) {
        char why[800];
        if (dxl_shell_choose_engine(s, b->dir, b->note, sizeof b->note, why, sizeof why)) dxl_shell_close_dialog(s);
        return;
    }
    if (b->mode == DXL_BROWSE_MOD) {
        dxl_mod m;
        dxl_err e;
        int ok = dxl_mod_check(b->dir, &m, &e) == 0;
        if (!ok && dxl_stricmp(dxl_path_basename(b->dir), "System") == 0) {
            char *up = dxl_path_dirname(b->dir);
            ok = dxl_mod_check(up, &m, NULL) == 0;
            free(up);
        }
        if (ok) {
            dxl_shell_close_dialog(s);
            dxl_shell_mod_shown(s, &m);
            dxl_mod_free(&m);
        } else {
            snprintf(b->note, sizeof b->note, "Not a mod's folder: %s.", dxl_err_msg(&e));
        }
        return;
    }
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

    dxl_rect in = dxl_dialog_begin(ui, over, 840, 600, b->mode == DXL_BROWSE_ENGINE ? "The engine's folder"
                                                       : b->mode == DXL_BROWSE_MOD  ? "The mod's folder"
                                                                                    : "The game's folder");
    dxl_label(ui, dxl_cut_top(&in, 28), DXL_FONT_BODY,
              b->dir[0] ? dxl_shell_tilde(b->dir, shown, sizeof shown) : "The drives", t->text_focus,
              DXL_W_LEFT | DXL_W_TAIL | DXL_W_MAY_CUT);
    dxl_cut_top(&in, 8);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_cut_bottom(&in, 12);
    if (b->note[0]) {
        dxl_label(ui, dxl_cut_bottom(&in, 24), DXL_FONT_SMALL, b->note, t->warn, DXL_W_LEFT | DXL_W_MAY_CUT);
        dxl_cut_bottom(&in, 6);
    }

    dxl_well(ui, in);
    int up = has_up(b->dir);
    int rows = b->count + up;
    dxl_list l;
    dxl_list_begin(ui, &l, "browser", dxl_inset(in, 4), rows, 36, 0);
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
        char *to = up && open == 0 ? above(b->dir)
                 : b->dir[0]         ? dxl_path_join(b->dir, b->names[open - up])
                                     : dxl_xstrdup(b->names[open - up]);
        go_to(s, &l, to);
        free(to);
    } else if (ok) {
        use(s);
    } else if (home) {
        const char *h = dxl_fs_home();
        char *top = h && *h ? NULL : first_root();
        go_to(s, &l, top ? top : h);
        free(top);
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
    dxl_paragraph_typed(ui, in, DXL_FONT_BODY, s->message, t->text, s->dialog_since);
    int ok = dxl_button(ui, "message ok", dxl_cut_right(&buttons, 150), "OK", DXL_W_PRIMARY);
    dxl_dialog_end(ui);
    if (ok || back) dxl_shell_close_dialog(s);
}

/* ---- the asking dialogs ------------------------------------------------ */

static void ask(dxl_shell *s, dxl_dialog d, uint32_t id) {
    if (s->dialog == DXL_DIALOG_NONE) s->dialog_return = s->ui.focus;
    s->dialog_since = s->ui.now;
    s->dialog = d;
    s->asked_for = id;
    s->answered = 0;
    s->answer = -1;
}

/* The answer kept for the control that asked, which reads it as it is
 * drawn: in the next frame, which comes at once, whatever input does; one
 * that frame does not draw, gone with its page or its list, never reads
 * it, and it is dropped as the frame ends (dxl_shell_frame). */
static void answer(dxl_shell *s, int a) {
    s->answer = a;
    s->answered = 1;
    s->answered_in = s->ui.frame;
    dxl_shell_close_dialog(s);
    dxl_ui_animate(&s->ui);
}

void dxl_shell_pick_with(dxl_shell *s, uint32_t id, const char *title, const char *const *labels,
                         const char *const *details, int n, int current) {
    ask(s, DXL_DIALOG_PICK, id);
    snprintf(s->ask_title, sizeof s->ask_title, "%s", title);
    if (n > DXL_PICK_MAX) n = DXL_PICK_MAX;
    for (int i = 0; i < n; i++) {
        snprintf(s->pick_text[i], sizeof s->pick_text[i], "%s", labels && labels[i] ? labels[i] : "");
        snprintf(s->pick_detail[i], sizeof s->pick_detail[i], "%s", details && details[i] ? details[i] : "");
        s->pick_labels[i] = s->pick_text[i];
    }
    s->pick_count = n;
    s->pick_current = current;
}

void dxl_shell_pick(dxl_shell *s, uint32_t id, const char *title, const char *const *labels, int n, int current) {
    dxl_shell_pick_with(s, id, title, labels, NULL, n, current);
}

const char *dxl_shell_picked(const dxl_shell *s, int answer) {
    return answer >= 0 && answer < s->pick_count ? s->pick_text[answer] : NULL;
}

int dxl_shell_picked_at(const dxl_shell *s, int answer, const char *const *labels, int n) {
    const char *picked = dxl_shell_picked(s, answer);
    if (!picked) return -1;
    if (answer < n && labels[answer] && !strcmp(labels[answer], picked)) return answer;
    for (int i = 0; i < n; i++)
        if (labels[i] && !strcmp(labels[i], picked)) return i;
    return -1;
}

void dxl_shell_osk(dxl_shell *s, uint32_t id, const char *title, char *buf, size_t size) {
    ask(s, DXL_DIALOG_OSK, id);
    snprintf(s->ask_title, sizeof s->ask_title, "%s", title);
    s->osk_buf = buf;
    s->osk_size = size;
    dxl_osk_open(&s->osk, buf);
}

void dxl_shell_confirm(dxl_shell *s, uint32_t id, const char *title, const char *text, const char *yes,
                       const char *no) {
    ask(s, DXL_DIALOG_CONFIRM, id);
    snprintf(s->ask_title, sizeof s->ask_title, "%s", title);
    snprintf(s->message, sizeof s->message, "%s", text);
    snprintf(s->ask_yes, sizeof s->ask_yes, "%s", yes);
    snprintf(s->ask_no, sizeof s->ask_no, "%s", no);
    s->ask_third[0] = '\0';
}

void dxl_shell_confirm3(dxl_shell *s, uint32_t id, const char *title, const char *text, const char *yes,
                        const char *third, const char *no) {
    dxl_shell_confirm(s, id, title, text, yes, no);
    snprintf(s->ask_third, sizeof s->ask_third, "%s", third);
}

int dxl_shell_answer(dxl_shell *s, uint32_t id) {
    if (!s->answered || s->asked_for != id) return -1;
    s->answered = 0;
    s->asked_for = 0;
    return s->answer;
}

void dxl_shell_answer_expire(dxl_shell *s) {
    if (!s->answered || s->ui.frame <= s->answered_in) return;
    dxl_log("a dialog's answer for a control no longer drawn: dropped");
    s->answered = 0;
    s->asked_for = 0;
}

static void pick(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    int grouped = 0;
    for (int i = 0; i < s->pick_count; i++) grouped |= s->pick_detail[i][0] != '\0';
    float row_h = grouped ? 50 : 40, rows = (float)(s->pick_count < 9 ? s->pick_count : 9);
    dxl_rect in = dxl_dialog_begin(ui, over, 520, 5 + 32 + 16 + rows * row_h + 8 + 20, s->ask_title);
    dxl_well(ui, in);
    dxl_list l;
    dxl_list_begin(ui, &l, "pick", dxl_inset(in, 4), s->pick_count, row_h, DXL_LIST_WHOLE_ROWS);
    if (ui->focus == 0 || s->pick_current >= 0) {
        dxl_list_focus(ui, &l, s->pick_current);
        s->pick_current = -1;
    }
    int chosen = -1;
    for (int i = 0; i < s->pick_count; i++)
        if (dxl_list_row(ui, &l, i, s->pick_labels[i], s->pick_detail[i], ui->theme.help, 0) & DXL_HIT_ACTIVATED)
            chosen = i;
    dxl_list_end(ui, &l);
    dxl_dialog_end(ui);
    if (chosen >= 0) answer(s, chosen);
    else if (back) {
        s->answered = 0;
        s->asked_for = 0;
        dxl_shell_close_dialog(s);
    }
}

static void osk(dxl_shell *s, dxl_rect over, int back) {
    int r = dxl_osk(&s->ui, &s->osk, over, s->ask_title, s->osk_buf, s->osk_size, back);
    if (r > 0) answer(s, 1);
    else if (r < 0) {
        s->answered = 0;
        s->asked_for = 0;
        dxl_shell_close_dialog(s);
    }
}

static void confirm(dxl_shell *s, dxl_rect over, int back) {
    dxl_ui *ui = &s->ui;
    float w = 620;
    if (w > over.w - 32) w = over.w - 32;
    float text = dxl_text_size(ui, DXL_FONT_BODY, s->message, w - 44).y;
    dxl_rect in = dxl_dialog_begin(ui, over, w, 5 + 32 + 16 + text + 16 + 46 + 20, s->ask_title);
    dxl_rect buttons = dxl_cut_bottom(&in, 46);
    dxl_paragraph_typed(ui, in, DXL_FONT_BODY, s->message, ui->theme.text, s->dialog_since);
    int no = dxl_button(ui, "confirm no", dxl_cut_right(&buttons, 170), s->ask_no, 0);
    dxl_cut_right(&buttons, 12);
    int third = 0;
    if (s->ask_third[0]) {
        third = dxl_button(ui, "confirm third", dxl_cut_right(&buttons, 170), s->ask_third, 0);
        dxl_cut_right(&buttons, 12);
    }
    int yes = dxl_button(ui, "confirm yes", dxl_cut_right(&buttons, 170), s->ask_yes, DXL_W_PRIMARY);
    dxl_dialog_end(ui);
    if (yes) answer(s, 1);
    else if (third) answer(s, 2);
    else if (no || back) answer(s, 0);
}

void dxl_dialogs(dxl_shell *s, dxl_rect r, int back) {
    switch (s->dialog) {
    case DXL_DIALOG_BROWSER: browser(s, r, back); break;
    case DXL_DIALOG_MESSAGE: message(s, r, back); break;
    case DXL_DIALOG_PICK: pick(s, r, back); break;
    case DXL_DIALOG_OSK: osk(s, r, back); break;
    case DXL_DIALOG_CONFIRM: confirm(s, r, back); break;
    case DXL_DIALOG_PADWIZARD: dxl_padwizard(s, r, back); break;
    case DXL_DIALOG_PADTEST: dxl_padtest(s, r, back); break;
    default: break;
    }
}
