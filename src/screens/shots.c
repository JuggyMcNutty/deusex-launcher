/* Screenshots: the profile's shots (core/shotlist.h), newest first, as a
 * gallery of thumbnails read off the window's thread (ui/thumbs.h); the
 * one chosen -- the tile last focused -- beside them with when it was taken
 * and its size. One is shown whole, the others stepped to from it; their
 * folder opened with the desktop's file manager; one deleted to the
 * desktop's trash. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/trash.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIDE_W  300
#define THUMB_W 384      /* px: a tile's picture, at most */
#define THUMB_H 216
#define WHOLE_W 2560     /* px: one shown whole, at most */
#define WHOLE_H 1600

static void reload(dxl_shell *s) {
    char keep[256];
    snprintf(keep, sizeof keep, "%s", s->shot_sel < s->shot_list.count ? s->shot_list.items[s->shot_sel].name : "");
    dxl_shots_free(&s->shot_list);
    s->shot_sel = 0;
    dxl_err e;
    if (dxl_shots_list(s->run->session.profile.shots_dir, &s->shot_list, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    for (int i = 0; i < s->shot_list.count; i++)
        if (!strcmp(s->shot_list.items[i].name, keep)) s->shot_sel = i;
    if (s->shot_open && !s->shot_list.count) s->shot_open = 0;
}

void dxl_page_shots_enter(dxl_shell *s) {
    if (!s->run) return;
    if (!s->thumbs) s->thumbs = dxl_thumbs_new(THUMB_W, THUMB_H);
    if (!s->shot_whole) s->shot_whole = dxl_thumbs_new(WHOLE_W, WHOLE_H);
    s->shot_open = 0;
    reload(s);
}

/* A picture of set's in r, fitted and centred: its texture's, a word while
 * it is read, another for one that does not read. */
static void picture(dxl_shell *s, dxl_thumbs *set, const char *path, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_thumbs_want(set, path);
    int failed = 0, w = 0, h = 0;
    SDL_Texture *tex = dxl_thumbs_get(set, path, &failed, &w, &h);
    if (!tex || w <= 0 || h <= 0) {
        dxl_fill(ui, r, dxl_rgba_fade(t->panel_shadow, 0.6f));
        dxl_label(ui, r, DXL_FONT_SMALL, failed ? "Not a picture SDL reads" : "...", t->help, DXL_W_MAY_CUT);
        return;
    }
    float k = r.w / (float)w < r.h / (float)h ? r.w / (float)w : r.h / (float)h;
    dxl_rect at = { r.x + (r.w - w * k) / 2, r.y + (r.h - h * k) / 2, w * k, h * k };
    dxl_rect px = dxl_px(ui, at);
    SDL_RenderTexture(ui->renderer, tex, NULL, &px);
}

static void size_text(long bytes, char *out, size_t n) {
    if (bytes >= 1024L * 1024) snprintf(out, n, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    else snprintf(out, n, "%ld KB", (bytes + 1023) / 1024);
}

/* The one chosen, whole: the page's but for a row of buttons under it. */
static void whole(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_shots *l = &s->shot_list;
    int i = s->shot_sel;
    const dxl_shot *sh = &l->items[i];
    dxl_rect bar = dxl_cut_bottom(&r, 48);
    dxl_cut_bottom(&r, 8);
    dxl_rect caption = dxl_cut_bottom(&r, 26);
    dxl_cut_bottom(&r, 6);
    dxl_thumbs_update(s->shot_whole, ui->renderer);
    picture(s, s->shot_whole, sh->path, r);
    char when[64], text[400];
    dxl_shot_when(sh, when, sizeof when);
    snprintf(text, sizeof text, "%s  \xc2\xb7  %s  \xc2\xb7  %d of %d", sh->name, when, i + 1, l->count);
    dxl_label(ui, caption, DXL_FONT_SMALL, text, t->help, DXL_W_LEFT | DXL_W_MAY_CUT);

    float bw = bar.w / 4 - 8 < 200 ? bar.w / 4 - 8 : 200;
    int prev = dxl_button(ui, "shot prev", dxl_cut_left(&bar, bw), "Previous", i > 0 ? 0 : DXL_W_DISABLED);
    dxl_cut_left(&bar, 10);
    int next = dxl_button(ui, "shot next", dxl_cut_left(&bar, bw), "Next", i + 1 < l->count ? 0 : DXL_W_DISABLED);
    dxl_cut_left(&bar, 10);
    uint32_t del_id = dxl_ui_id(ui, "shot delete");
    int del = dxl_button_id(ui, del_id, dxl_cut_left(&bar, bw), "Delete...", 0);
    dxl_cut_left(&bar, 10);
    int close = dxl_button(ui, "shot close", dxl_cut_left(&bar, bw), "Close", 0);
    /* The focus on Next as it opens, Previous at the last; on one of them
     * again when its button goes (Previous at the first). A dialog's
     * buttons are drawn after the page: its focus is its own. */
    if (s->dialog == DXL_DIALOG_NONE && (!ui->focus || !dxl_focus_has(&ui->cur, ui->focus))) {
        uint32_t want = dxl_ui_id(ui, i + 1 < l->count ? "shot next" : "shot prev");
        if (i + 1 >= l->count && i == 0) want = dxl_ui_id(ui, "shot close");
        dxl_ui_focus(ui, want);
    }

    if (dxl_shell_answer(s, del_id) == 1) {
        dxl_err e;
        char name[256];
        snprintf(name, sizeof name, "%s", sh->name);
        if (dxl_trash(sh->path, s->run->session.xdg.data_home, NULL, 0, &e) != 0) {
            dxl_shell_message(s, "Not deleted", "%s", dxl_err_msg(&e));
        } else {
            dxl_log("shots: %s to the trash", sh->path);
            dxl_shell_toast(s, "%s is in the trash.", name);
            /* The next in its place, or the one before the last. */
            reload(s);
            if (i < l->count) s->shot_sel = i;
            else if (l->count) s->shot_sel = l->count - 1;
        }
        return;
    }
    /* Each change a frame of its own at once: the picture to show is asked
     * for as it is drawn. */
    if (prev && i > 0) {
        s->shot_sel = i - 1;
        dxl_ui_animate(ui);
    } else if (next && i + 1 < l->count) {
        s->shot_sel = i + 1;
        dxl_ui_animate(ui);
    }
    else if (del) {
        char title[320];
        snprintf(title, sizeof title, "Delete %s?", sh->name);
        dxl_shell_confirm(s, del_id, title, "The picture goes to the trash, and can be put back from there.", "Delete",
                          "Keep it");
    } else if (close || ui->in.back) {
        s->shot_open = 0;
        s->shot_refocus = 1;
        dxl_thumbs_clear(s->shot_whole);
        dxl_ui_animate(ui);
    }
}

/* Tile i's id in a gallery of cols: its row's cell. */
static uint32_t tile_id(const dxl_ui *ui, int i, int cols) {
    dxl_list grid;
    memset(&grid, 0, sizeof grid);
    grid.id = dxl_ui_id(ui, "shots grid");
    return dxl_list_cell_id(&grid, i / cols, i % cols);
}

void dxl_page_shots(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    dxl_shots *l = &s->shot_list;
    s->thumbs_pending = dxl_thumbs_update(s->thumbs, ui->renderer);
    if (s->shot_open && s->shot_sel < l->count) {
        whole(s, r);
        s->thumbs_pending |= dxl_thumbs_update(s->shot_whole, ui->renderer);
        return;
    }

    float w = r.w < 1360 ? r.w : 1360;
    float side_w = w * 0.3f < SIDE_W ? w * 0.3f : SIDE_W;
    dxl_rect area = { r.x, r.y + 6, w - side_w - 20, r.h - 6 };
    dxl_rect side = { r.x + w - side_w, r.y + 6, side_w, r.h - 6 };
    dxl_rect help_r = dxl_cut_bottom(&area, 36);
    dxl_cut_bottom(&area, 6);
    const char *why = NULL;

    /* The gallery: rows of two to four tiles, a list of them. */
    dxl_well(ui, area);
    dxl_rect in = dxl_inset(area, 4);
    int cols = (int)(in.w / 230);
    if (cols < 2) cols = 2;
    if (cols > 4) cols = 4;
    float tile_w = (in.w - 14) / cols, pic_h = (tile_w - 16) * 9 / 16, tile_h = pic_h + 16 + 40;
    int rows = (l->count + cols - 1) / cols, show = -1;
    if (!l->count) {
        dxl_paragraph(ui, dxl_inset(area, 18), DXL_FONT_BODY,
                      "No screenshots yet. The game's Shot key (F9) puts them here, the profile's own.", t->help);
    } else {
        /* Back from the one shown whole: on its tile, scrolled to it. */
        if (s->shot_refocus && s->shot_sel < l->count) {
            ui->focus = tile_id(ui, s->shot_sel, cols);
            ui->focus_moved = 1;
        }
        s->shot_refocus = 0;
        dxl_list list;
        dxl_list_begin(ui, &list, "shots grid", in, rows, tile_h, DXL_LIST_WHOLE_ROWS);
        for (int row = 0; row < rows; row++) {
            dxl_rect rr;
            dxl_list_rect(&list, row, &rr);
            if (rr.y + rr.h <= in.y || rr.y >= in.y + in.h) continue;
            for (int c = 0; c < cols; c++) {
                int i = row * cols + c;
                if (i >= l->count) break;
                const dxl_shot *sh = &l->items[i];
                dxl_rect tile = { rr.x + c * tile_w, rr.y, tile_w, tile_h };
                dxl_rect seen = tile;
                if (seen.y < in.y) { seen.h -= in.y - seen.y; seen.y = in.y; }
                if (seen.y + seen.h > in.y + in.h) seen.h = in.y + in.h - seen.y;
                uint32_t id = dxl_list_cell_id(&list, row, c);
                int hits = dxl_ui_focusable(ui, id, seen, 0);
                dxl_ui_name(ui, id, sh->name);
                if (hits & DXL_HIT_FOCUSED) dxl_fill(ui, dxl_inset(tile, 3), t->row_focus);
                else if (hits & DXL_HIT_HOVERED) dxl_fill(ui, dxl_inset(tile, 3), t->row_highlight);
                if ((hits & DXL_HIT_FOCUSED) && s->shot_sel != i) s->shot_sel = i;
                if (hits & DXL_HIT_FOCUSED) why = "Enter or a click shows it whole.";
                if (hits & DXL_HIT_ACTIVATED) show = i;
                picture(s, s->thumbs, sh->path, (dxl_rect){ tile.x + 8, tile.y + 8, tile_w - 16, pic_h });
                char when[64];
                dxl_shot_when(sh, when, sizeof when);
                dxl_rgba c1 = (hits & DXL_HIT_FOCUSED) ? t->text_focus : t->text;
                dxl_label(ui, (dxl_rect){ tile.x + 10, tile.y + 12 + pic_h, tile_w - 20, 20 }, DXL_FONT_SMALL, sh->name,
                          c1, DXL_W_LEFT | DXL_W_MAY_CUT);
                dxl_label(ui, (dxl_rect){ tile.x + 10, tile.y + 32 + pic_h, tile_w - 20, 20 }, DXL_FONT_SMALL, when,
                          t->help, DXL_W_LEFT | DXL_W_MAY_CUT);
                if (i == s->shot_sel && !(hits & DXL_HIT_FOCUSED))
                    dxl_fill(ui, (dxl_rect){ tile.x + 4, tile.y + 8, 3, pic_h }, t->accent);
            }
        }
        dxl_list_end(ui, &list);
    }
    int sel = s->shot_sel < l->count ? s->shot_sel : -1;
    const dxl_shot *chosen = sel >= 0 ? &l->items[sel] : NULL;

    /* The one chosen, then what acts on it. */
    dxl_rect back_r = dxl_cut_bottom(&side, 48);
    if (chosen) {
        char v[300], when[64];
        dxl_shot_when(chosen, when, sizeof when);
        dxl_label(ui, dxl_cut_top(&side, 26), DXL_FONT_BODY, chosen->name, t->text, DXL_W_LEFT | DXL_W_MAY_CUT);
        dxl_field(ui, &side, 26, 70, "Taken", when, t->text, DXL_W_MAY_CUT);
        size_text(chosen->size, v, sizeof v);
        dxl_field(ui, &side, 26, 70, "Size", v, t->text, DXL_W_MAY_CUT);
        dxl_cut_top(&side, 12);
    }
    int b_show = dxl_button(ui, "shots show", dxl_cut_top(&side, 48), "Show", DXL_W_PRIMARY | DXL_W_LEFT |
                            (chosen ? 0 : DXL_W_DISABLED));
    dxl_cut_top(&side, 10);
    uint32_t folder_id = dxl_ui_id(ui, "shots folder");
    int b_folder = dxl_button_id(ui, folder_id, dxl_cut_top(&side, 48), "Open the folder", DXL_W_LEFT);
    dxl_cut_top(&side, 10);
    uint32_t del_id = dxl_ui_id(ui, "shots delete");
    int b_delete = dxl_button_id(ui, del_id, dxl_cut_top(&side, 48), "Delete...", DXL_W_LEFT | (chosen ? 0 : DXL_W_DISABLED));
    int b_back = dxl_button(ui, "shots back", back_r, "Back", DXL_W_LEFT);
    if (ui->focus == folder_id) why = "The profile's Screenshots folder, in the desktop's file manager.";
    else if (ui->focus == del_id) why = "The picture to the trash, to be put back from there.";
    if (why) dxl_paragraph_typed(ui, help_r, DXL_FONT_SMALL, why, t->help, ui->focus_since);

    if (dxl_shell_answer(s, del_id) == 1 && chosen) {
        dxl_err e;
        char name[256];
        snprintf(name, sizeof name, "%s", chosen->name);
        if (dxl_trash(chosen->path, ss->xdg.data_home, NULL, 0, &e) != 0) {
            dxl_shell_message(s, "Not deleted", "%s", dxl_err_msg(&e));
        } else {
            dxl_log("shots: %s to the trash", chosen->path);
            dxl_shell_toast(s, "%s is in the trash.", name);
            int at = sel;
            reload(s);
            if (at < l->count) s->shot_sel = at;
            else if (l->count) s->shot_sel = l->count - 1;
        }
        return;
    }
    if (show >= 0 || (b_show && chosen)) {
        if (show >= 0) s->shot_sel = show;
        s->shot_open = 1;
        ui->focus = 0;
        dxl_ui_animate(ui);
    } else if (b_folder) {
        dxl_path_mkdirs(ss->profile.shots_dir);
        dxl_shell_open(s, ss->profile.shots_dir);
    } else if (b_delete && chosen) {
        char title[320];
        snprintf(title, sizeof title, "Delete %s?", chosen->name);
        dxl_shell_confirm(s, del_id, title, "The picture goes to the trash, and can be put back from there.", "Delete",
                          "Keep it");
    } else if (b_back || ui->in.back) {
        dxl_shell_go(s, DXL_PAGE_HOME);
    }
}
