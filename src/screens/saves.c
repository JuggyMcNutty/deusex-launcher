/* Saves: the profile's saves (core/saves.h), newest first, each with what
 * it says, where it was made, when, and how long the game had been played;
 * the one chosen -- the row last focused -- beside them with its snapshot.
 * One is loaded: the game started on it (its start URL ?loadgame=N, with
 * an engine that starts on one); one deleted: its folder to the desktop's
 * trash, to be put back from there. */
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

#define ROW_H  58
#define SIDE_W 340

/* The snapshot shown let go: a slot's save may be another now (the quick
 * save's is made again at each quick save). */
static void forget_snapshot(dxl_shell *s) {
    if (s->save_snap) SDL_DestroyTexture(s->save_snap);
    s->save_snap = NULL;
    s->save_snap_slot = DXL_SNAP_NONE;
    s->save_snap_read = 0;
}

void dxl_saves_reload(dxl_shell *s) {
    int keep = s->save_sel < s->save_list.count ? s->save_list.items[s->save_sel].slot : DXL_SNAP_NONE;
    forget_snapshot(s);
    dxl_saves_free(&s->save_list);
    s->save_sel = 0;
    if (!s->run) return;
    dxl_err e;
    if (dxl_saves_list(s->run->session.profile.save_dir, &s->save_list, &e) != 0)
        dxl_log("warning: %s", dxl_err_msg(&e));
    for (int i = 0; i < s->save_list.count; i++)
        if (s->save_list.items[i].slot == keep) s->save_sel = i;
}

void dxl_page_saves_enter(dxl_shell *s) {
    dxl_saves_reload(s);
    /* The focus come back to a row: the chosen save's, kept by its slot,
     * not the row in that place, which may be another save now. */
    dxl_list list;
    memset(&list, 0, sizeof list);
    list.id = dxl_ui_id(&s->ui, "saves");
    list.count = s->save_list.count;
    for (int i = 0; i < 4096; i++)
        if (dxl_list_row_id(&list, i) == s->ui.focus) {
            dxl_list_focus(&s->ui, &list, s->save_sel);
            break;
        }
}

/* The snapshot of sv as a texture of the window's renderer: read once for
 * its slot. */
static SDL_Texture *snapshot_of(dxl_shell *s, const dxl_save *sv) {
    dxl_ui *ui = &s->ui;
    if (s->save_snap_slot != sv->slot) {
        if (s->save_snap) SDL_DestroyTexture(s->save_snap);
        s->save_snap = NULL;
        s->save_snap_slot = sv->slot;
        s->save_snap_read = 0;
    }
    if (s->save_snap_read || !ui->renderer) return s->save_snap;
    s->save_snap_read = 1;
    char *dir = dxl_save_path(s->run->session.profile.save_dir, sv);
    char *path = dxl_path_join(dir, "SaveInfo.dxs");
    dxl_saveinfo info;
    dxl_err e;
    if (path && dxl_saveinfo_read(path, 1, &info, &e) == 0) {
        if (info.snapshot.rgba) {
            s->save_snap = SDL_CreateTexture(ui->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                             info.snapshot.w, info.snapshot.h);
            if (s->save_snap) {
                SDL_UpdateTexture(s->save_snap, NULL, info.snapshot.rgba, info.snapshot.w * 4);
                SDL_SetTextureScaleMode(s->save_snap, SDL_SCALEMODE_LINEAR);
            }
        }
        dxl_saveinfo_free(&info);
    } else if (path) {
        dxl_log("saves: %s", dxl_err_msg(&e));
    }
    free(path);
    free(dir);
    return s->save_snap;
}

int dxl_save_snapshot(dxl_shell *s, const dxl_save *sv, dxl_rect r) {
    if (!sv || !sv->readable || !s->run) return 0;
    SDL_Texture *tex = snapshot_of(s, sv);
    if (!tex) return 0;
    float tw = 0, th = 0;
    SDL_GetTextureSize(tex, &tw, &th);
    /* The game's picture is 160 x 120 of its 256 x 128. */
    SDL_FRect src = { 0, 0, tw, th };
    if (tw == 256 && th == 128) src = (SDL_FRect){ 0, 0, 160, 120 };
    float k = r.w / src.w < r.h / src.h ? r.w / src.w : r.h / src.h;
    dxl_rect at = { r.x, r.y, src.w * k, src.h * k };
    dxl_ui *ui = &s->ui;
    dxl_rect px = dxl_px(ui, at);
    SDL_RenderTexture(ui->renderer, tex, &src, &px);
    dxl_outline(ui, at, 1, ui->theme.panel_light);
    return 1;
}

/* A save's row: what it says, then where, when, how long it had been
 * played. */
static void describe(const dxl_save *sv, char *title, size_t tsize, char *detail, size_t dsize) {
    if (!sv->readable) {
        snprintf(title, tsize, "%s", sv->folder);
        snprintf(detail, dsize, "Its SaveInfo.dxs does not read: a save the game cannot load.");
        return;
    }
    const dxl_saveinfo *i = &sv->info;
    char when[64], played[48];
    dxl_save_when(sv, when, sizeof when);
    dxl_save_played(sv, played, sizeof played);
    snprintf(title, tsize, "%s", i->description[0] ? i->description : i->location[0] ? i->location : sv->folder);
    snprintf(detail, dsize, "%.100s  \xc2\xb7  %s  \xc2\xb7  played %s%s", i->location[0] ? i->location : i->map, when,
             played, sv->slot == DXL_SAVE_QUICK ? "  \xc2\xb7  the quick save" : "");
}

void dxl_page_saves(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    float w = r.w < 1160 ? r.w : 1160;
    float side_w = w * 0.38f < SIDE_W ? w * 0.38f : SIDE_W;
    dxl_rect area = { r.x, r.y + 6, w - side_w - 20, r.h - 6 };
    dxl_rect side = { r.x + w - side_w, r.y + 6, side_w, r.h - 6 };
    dxl_rect help_r = dxl_cut_bottom(&area, 36);
    dxl_cut_bottom(&area, 6);
    const dxl_saves *l = &s->save_list;
    int can_load = ss->install.ok && dxl_shell_engine_has(s, "loadgame-url");
    const char *why = NULL;

    /* The saves. */
    dxl_well(ui, area);
    int load = -1;
    if (!l->count) {
        dxl_paragraph(ui, dxl_inset(area, 18), DXL_FONT_BODY,
                      "No saves yet. The game's saves land here, the profile's own, as it makes them.", t->help);
    } else {
        dxl_list list;
        dxl_list_begin(ui, &list, "saves", dxl_inset(area, 4), l->count, ROW_H, 0);
        for (int i = 0; i < l->count; i++) {
            char title[160], detail[300];
            describe(&l->items[i], title, sizeof title, detail, sizeof detail);
            int hits = dxl_list_row(ui, &list, i, title, detail, l->items[i].readable ? t->help : t->warn,
                                    DXL_W_MAY_CUT);
            if ((hits & DXL_HIT_FOCUSED) && s->save_sel != i) s->save_sel = i;
            if (hits & DXL_HIT_FOCUSED)
                why = !l->items[i].readable ? "This save's folder holds no SaveInfo.dxs the game reads."
                      : can_load ? "Enter or a click starts the game on this save."
                                 : NULL;
            if (hits & DXL_HIT_ACTIVATED) load = i;
            /* The one chosen, as the focus is on what acts on it. */
            dxl_rect rr;
            if (i == s->save_sel && !(hits & DXL_HIT_FOCUSED) && dxl_list_rect(&list, i, &rr))
                dxl_fill(ui, (dxl_rect){ rr.x + 4, rr.y + 8, 3, rr.h - 16 }, t->accent);
        }
        dxl_list_end(ui, &list);
    }
    int sel = s->save_sel < l->count ? s->save_sel : -1;
    const dxl_save *chosen = sel >= 0 ? &l->items[sel] : NULL;
    int chosen_ok = chosen && chosen->readable;

    /* The one chosen: its snapshot, then when, how long, where. */
    dxl_rect back_r = dxl_cut_bottom(&side, 48);
    dxl_cut_bottom(&side, 10);
    float fields = 3 * 26, buttons = 2 * 48 + 10;
    float snap_h = side.h - fields - buttons - 30;
    if (snap_h > side.w * 0.75f) snap_h = side.w * 0.75f;
    if (snap_h < 0) snap_h = 0;
    if (chosen && snap_h >= 60) {
        dxl_rect snap = dxl_cut_top(&side, snap_h);
        if (!dxl_save_snapshot(s, chosen, snap)) {
            dxl_rect box = { snap.x, snap.y, snap_h * 4 / 3, snap_h };
            dxl_fill(ui, box, dxl_rgba_fade(t->panel_shadow, 0.6f));
            dxl_outline(ui, box, 1, t->panel_light);
            dxl_label(ui, dxl_inset(box, 10), DXL_FONT_SMALL,
                      chosen_ok ? "No snapshot: the renderer took none." : "No snapshot.", t->help, DXL_W_MAY_CUT);
        }
        dxl_cut_top(&side, 12);
    }
    if (chosen_ok) {
        char v[200];
        dxl_save_when(chosen, v, sizeof v);
        dxl_field(ui, &side, 26, 90, "Saved", v, t->text, DXL_W_MAY_CUT);
        dxl_save_played(chosen, v, sizeof v);
        dxl_field(ui, &side, 26, 90, "Played", v, t->text, DXL_W_MAY_CUT);
        snprintf(v, sizeof v, "%s, %s", chosen->info.map, chosen->folder);
        dxl_field(ui, &side, 26, 90, "Map", v, t->text, DXL_W_MAY_CUT);
    }
    dxl_cut_top(&side, 10);

    /* What acts on it. */
    int b_load = dxl_button(ui, "saves load", dxl_cut_top(&side, 48), "Load",
                            DXL_W_PRIMARY | DXL_W_LEFT | (chosen_ok && can_load ? 0 : DXL_W_DISABLED));
    dxl_cut_top(&side, 10);
    uint32_t del_id = dxl_ui_id(ui, "saves delete");
    int b_delete = dxl_button_id(ui, del_id, dxl_cut_top(&side, 48), "Delete...", DXL_W_LEFT | (chosen ? 0 : DXL_W_DISABLED));
    int b_back = dxl_button(ui, "saves back", back_r, "Back", DXL_W_LEFT);
    if (ui->focus == del_id)
        why = "The save's folder to the trash, to be put back from there.";
    if (!can_load && l->count)
        why = !ss->install.ok ? "The profile's game is not there to play: Home sets it right."
              : s->engine_ok ? "This engine starts on no save: a VibeEngine with the loadgame-url feature does."
                             : "No engine to play with: Home sets it right.";
    if (why) dxl_paragraph_typed(ui, help_r, DXL_FONT_SMALL, why, t->help, ui->focus_since);

    if (dxl_shell_answer(s, del_id) == 1 && chosen) {
        char *path = dxl_save_path(ss->profile.save_dir, chosen);
        char name[160];
        snprintf(name, sizeof name, "%.150s", chosen->readable && chosen->info.description[0] ? chosen->info.description
                                                                                                : chosen->folder);
        dxl_err e;
        if (dxl_trash(path, ss->xdg.data_home, NULL, 0, &e) != 0) {
            dxl_shell_message(s, "Not deleted", "%s", dxl_err_msg(&e));
        } else {
            dxl_log("saves: %s to the trash", path);
            dxl_shell_toast(s, "%s is in the trash.", name);
            dxl_saves_reload(s);
        }
        free(path);
        return;
    }
    if ((load >= 0 || b_load || (ui->in.start && chosen_ok)) && can_load) {
        const dxl_save *sv = load >= 0 ? &l->items[load] : chosen;
        if (sv && sv->readable) dxl_shell_play_save(s, sv->slot);
    } else if (b_delete && chosen) {
        char title[200], text[400];
        snprintf(title, sizeof title, "Delete %.150s?",
                 chosen->readable && chosen->info.description[0] ? chosen->info.description : chosen->folder);
        snprintf(text, sizeof text, "The save goes to the trash, and can be put back from there.");
        dxl_shell_confirm(s, del_id, title, text, "Delete", "Keep it");
    } else if (b_back || (ui->in.back && !ui->text_id)) {
        dxl_shell_go(s, s->from == DXL_PAGE_RECOVERY ? DXL_PAGE_RECOVERY : DXL_PAGE_HOME);
    }
}
