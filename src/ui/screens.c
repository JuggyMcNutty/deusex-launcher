#include "screens_internal.h"
#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const tab_names[DXL_TAB_COUNT] = { "Play", "Video", "Controls", "System" };

#define NOTICE_FRAMES 150    /* about 2.5 s at the panel's 60 Hz */
#define MAX_ROWS 32

/* ---- session helpers --------------------------------------------------- */

void notice(dxl_session *s, int bad, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->notice, sizeof s->notice, fmt, ap);
    va_end(ap);
    s->notice_frames = NOTICE_FRAMES;
    s->notice_bad = bad;
}

static void close_overlay(dxl_session *s) {
    remap_free(s);
    for (int i = 0; i < s->ovl_nlines; i++) free(s->ovl_lines[i]);
    free(s->ovl_lines);
    s->ovl_lines = NULL;
    s->ovl_nlines = 0;
    s->overlay = DXL_OVL_NONE;
    s->ovl_cursor = s->ovl_scroll = 0;
}

void open_confirm(dxl_session *s, const char *title, const char *body,
                  const char *yes, dxl_confirm_fn fn) {
    close_overlay(s);
    s->overlay = DXL_OVL_CONFIRM;
    snprintf(s->ovl_title, sizeof s->ovl_title, "%s", title);
    snprintf(s->confirm_body, sizeof s->confirm_body, "%s", body);
    snprintf(s->confirm_yes, sizeof s->confirm_yes, "%s", yes);
    s->confirm_fn = fn;
    s->ovl_cursor = 0;   /* Cancel: a stray A must not destroy anything */
}

/* The text panel's inner width, shared by wrapping and drawing. */
static SDL_Rect text_panel(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int footer = dxl_ui_line_height(s->ui, DXL_TXT_FOOTER) + m->pad;
    return (SDL_Rect){ m->pad, m->pad / 2, dxl_ui_width(s->ui) - 2 * m->pad,
                       dxl_ui_height(s->ui) - m->pad / 2 - footer };
}

/* Text lines may carry one tab: the part after it is drawn in a second
 * column, so "L2<tab>Use" tables line up in a proportional font. */
static int text_column(dxl_session *s) { return text_panel(s).w / 3; }

static void push_line(dxl_session *s, const char *text, size_t len) {
    s->ovl_lines = dxl_xrealloc(s->ovl_lines, (size_t)(s->ovl_nlines + 1) * sizeof *s->ovl_lines);
    s->ovl_lines[s->ovl_nlines++] = dxl_xstrndup(text, len);
}

static int fits(dxl_session *s, const char *text, size_t n, int max_w) {
    char buf[512];
    if (n >= sizeof buf) return 0;
    memcpy(buf, text, n);
    buf[n] = '\0';
    return dxl_ui_text_width(s->ui, buf, DXL_TXT_FOOTER) <= max_w;
}

/* How much of text[0..len) goes on one line: the longest prefix that fits
 * (binary search -- a few measurements per line rather than one per
 * character), backed off to the last space when there is one. Never splits a
 * UTF-8 sequence, and always takes at least one character. */
static size_t fit_prefix(dxl_session *s, const char *text, size_t len, int max_w) {
    if (memchr(text, '\t', len) || fits(s, text, len, max_w)) return len;
    size_t lo = 1, hi = len < 511 ? len : 511;
    while (lo < hi) {
        size_t mid = (lo + hi + 1) / 2;
        if (fits(s, text, mid, max_w)) lo = mid; else hi = mid - 1;
    }
    size_t take = lo;
    while (take > 1 && ((unsigned char)text[take] & 0xC0) == 0x80) take--;
    for (size_t i = take; i > take / 2; i--)
        if (text[i - 1] == ' ') return i;
    return take;
}

/* Splits text into lines that fit the panel, breaking long lines at spaces
 * where it can and anywhere where it must (log lines carry long paths). */
void open_text(dxl_session *s, const char *title, char *text) {
    close_overlay(s);
    s->overlay = DXL_OVL_TEXT;
    snprintf(s->ovl_title, sizeof s->ovl_title, "%s", title);

    int max_w = text_panel(s).w - dxl_ui_metrics(s->ui)->pad;
    const char *p = text ? text : "";
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        if (len && p[len - 1] == '\r') len--;
        size_t start = 0;
        do {
            size_t take = fit_prefix(s, p + start, len - start, max_w);
            push_line(s, p + start, take);
            start += take;
        } while (start < len);
        if (len == 0) push_line(s, "", 0);
        p += eol ? (size_t)(eol - p) + 1 : len;
    }
    free(text);
    /* Logs are read from the end: start at the bottom. */
    int per = (text_panel(s).h - 3 * dxl_ui_line_height(s->ui, DXL_TXT_ITEM)) /
              dxl_ui_line_height(s->ui, DXL_TXT_FOOTER);
    s->ovl_scroll = s->ovl_nlines > per ? s->ovl_nlines - per : 0;
}

void open_renderer_picker(dxl_session *s) {
    close_overlay(s);
    s->overlay = DXL_OVL_RENDERERS;
    snprintf(s->ovl_title, sizeof s->ovl_title, "Renderer");
    int cur = dxl_app_current_renderer(s->app);
    s->ovl_cursor = cur >= 0 ? cur : 0;
}

void switch_tab(dxl_session *s, dxl_tab t) {
    s->tab = t;
    s->notice_frames = 0;
}

char *read_tail(const char *path, size_t max) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long from = size > (long)max ? size - (long)max : 0;
    fseek(f, from, SEEK_SET);
    char *buf = dxl_xmalloc((size_t)(size - from) + 1);
    size_t got = fread(buf, 1, (size_t)(size - from), f);
    fclose(f);
    buf[got] = '\0';
    /* Started mid-line: drop the fragment. */
    if (from > 0) {
        char *nl = strchr(buf, '\n');
        if (nl) memmove(buf, nl + 1, strlen(nl + 1) + 1);
    }
    return buf;
}

/* run-game.sh writes one "--- <date> ---" block per launch. Only the last
 * block describes the last run. */
static const char *last_block(const char *log) {
    const char *at = log, *p = log;
    while ((p = strstr(p, "\n--- ")) != NULL) at = ++p;
    return at;
}

int last_engine_exit(const dxl_app *app, int *code) {
    char *log = read_tail(app->run_log, 16384);
    if (!log) return 0;
    const char *block = last_block(log), *p = block, *hit = NULL;
    while ((p = strstr(p, "engine exit: ")) != NULL) hit = p++;
    int found = 0;
    if (hit) { *code = atoi(hit + strlen("engine exit: ")); found = 1; }
    free(log);
    return found;
}

int last_engine_error(const dxl_app *app, char *out, size_t n) {
    char *log = read_tail(app->run_log, 16384);
    if (!log) return 0;
    const char *block = last_block(log), *p = block;
    int found = 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        /* The engine reports "SurrealEngine error: <what>"; the launcher's
         * own script says "... missing -- nothing to launch". */
        const char *e = dxl_stristr(p, "error:");
        if (e && e < p + len) {
            const char *what = e + strlen("error:");
            while (*what == ' ') what++;
            snprintf(out, n, "%.*s", (int)(p + len - what), what);
            found = 1;
        } else if (dxl_stristr(p, "binary missing") && dxl_stristr(p, "binary missing") < p + len) {
            snprintf(out, n, "%.*s", (int)len, p);
            found = 1;
        }
        p += len + (eol ? 1 : 0);
    }
    free(log);
    return found;
}

/* ---- Settings.json-backed rows --------------------------------------- */

static const char *choice_label(const dxl_row *r, const char *value) {
    const dxl_es_info *in = dxl_es_describe((dxl_es_field)r->arg);
    if (r->choice_labels)
        for (int i = 0; in->choices[i]; i++)
            if (strcmp(in->choices[i], value) == 0 && r->choice_labels[i])
                return r->choice_labels[i];
    return value;
}

void es_value(dxl_session *s, const dxl_row *r, char *out, size_t n) {
    dxl_es_field f = (dxl_es_field)r->arg;
    const dxl_es_info *in = dxl_es_describe(f);
    switch (in->kind) {
    case DXL_ES_CHOICE:
        snprintf(out, n, "%s", choice_label(r, dxl_es_choice(s->app->es, f)));
        break;
    case DXL_ES_BOOL:
        snprintf(out, n, "%s", dxl_es_bool(s->app->es, f) ? "On" : "Off");
        break;
    case DXL_ES_NUMBER: {
        double v = dxl_es_number(s->app->es, f);
        if (in->step >= 1)       snprintf(out, n, "%d", (int)(v + 0.5));
        else if (in->max <= 1.0) snprintf(out, n, "%d%%", (int)(v * 100 + 0.5));
        else                     snprintf(out, n, "%.2fx", v);
        break;
    }
    }
}

void es_step(dxl_session *s, const dxl_row *r, int dir) {
    dxl_es_field f = (dxl_es_field)r->arg;
    const dxl_es_info *in = dxl_es_describe(f);
    switch (in->kind) {
    case DXL_ES_CHOICE: {
        int count = 0, at = 0;
        const char *cur = dxl_es_choice(s->app->es, f);
        for (; in->choices[count]; count++)
            if (strcmp(in->choices[count], cur) == 0) at = count;
        at = (at + dir + count) % count;
        dxl_es_set_choice(s->app->es, f, in->choices[at]);
        break;
    }
    case DXL_ES_BOOL:
        dxl_es_set_bool(s->app->es, f, !dxl_es_bool(s->app->es, f));
        break;
    case DXL_ES_NUMBER:
        dxl_es_set_number(s->app->es, f, dxl_es_number(s->app->es, f) + dir * in->step);
        break;
    }
}

void es_reset(dxl_session *s, const dxl_row *r) {
    dxl_es_reset(s->app->es, (dxl_es_field)r->arg);
}

double es_slider(dxl_session *s, const dxl_row *r) {
    const dxl_es_info *in = dxl_es_describe((dxl_es_field)r->arg);
    double v = dxl_es_number(s->app->es, (dxl_es_field)r->arg);
    return in->max > in->min ? (v - in->min) / (in->max - in->min) : 0;
}

/* ---- the row list ------------------------------------------------------ */

static dxl_rows rows_for(dxl_tab t) {
    switch (t) {
    case DXL_TAB_VIDEO:    return tab_video_rows();
    case DXL_TAB_CONTROLS: return tab_controls_rows();
    case DXL_TAB_SYSTEM:   return tab_system_rows();
    default:               return (dxl_rows){ NULL, 0 };
    }
}

/* Indexes of the rows currently shown. */
static int visible_rows(dxl_session *s, dxl_rows rs, const dxl_row **out) {
    int n = 0;
    for (int i = 0; i < rs.count && n < MAX_ROWS; i++)
        if (!rs.rows[i].visible || rs.rows[i].visible(s, &rs.rows[i]))
            out[n++] = &rs.rows[i];
    return n;
}

int dxl_session_row_index(dxl_session *s, dxl_tab tab, const char *label) {
    const dxl_row *vis[MAX_ROWS];
    int n = visible_rows(s, rows_for(tab), vis);
    for (int i = 0; i < n; i++)
        if (strcmp(vis[i]->label, label) == 0) return i;
    return -1;
}

static int row_enabled(dxl_session *s, const dxl_row *r, char *why, size_t n) {
    if (why && n) why[0] = '\0';
    return !r->enabled || r->enabled(s, r, why, n);
}

/* The help pane: three lines at the bottom of the tab. */
static int help_top(dxl_session *s, int bottom) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    return bottom - 3 * dxl_ui_line_height(s->ui, DXL_TXT_BODY) - m->pad / 2;
}

static void draw_help(dxl_session *s, int top, const char *text, int warn) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int w = dxl_ui_width(s->ui) - 2 * m->pad;
    dxl_ui_fill(s->ui, (SDL_Rect){ m->pad, top, w, 1 }, DXL_PAL.rule);
    int y = top + m->pad / 4;
    if (s->notice_frames > 0) {
        y = dxl_ui_paragraph_n(s->ui, m->pad, y, w, s->notice, DXL_TXT_BODY,
                               s->notice_bad ? DXL_PAL.bad : DXL_PAL.ok, 1);
        dxl_ui_paragraph_n(s->ui, m->pad, y, w, text, DXL_TXT_BODY,
                           warn ? DXL_PAL.warn : DXL_PAL.text_dim, 2);
        return;
    }
    dxl_ui_paragraph_n(s->ui, m->pad, y, w, text, DXL_TXT_BODY,
                       warn ? DXL_PAL.warn : DXL_PAL.text_dim, 3);
}

static void rows_draw(dxl_session *s, int top, int bottom) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    const dxl_row *vis[MAX_ROWS];
    int n = visible_rows(s, rows_for(s->tab), vis);
    int *cur = &s->cursor[s->tab], *scroll = &s->scroll[s->tab];
    if (n == 0) return;
    if (*cur >= n) *cur = n - 1;
    if (*cur < 0) *cur = 0;

    int htop = help_top(s, bottom);
    int per = (htop - top - m->pad / 4) / m->item_height;
    if (per < 1) per = 1;
    if (*cur < *scroll) *scroll = *cur;
    if (*cur >= *scroll + per) *scroll = *cur - per + 1;
    if (*scroll > n - per) *scroll = n - per > 0 ? n - per : 0;

    for (int i = 0; i < per && *scroll + i < n; i++) {
        const dxl_row *r = vis[*scroll + i];
        int sel = (*scroll + i == *cur);
        int en = row_enabled(s, r, NULL, 0);
        int flags = (en ? 0 : DXL_ROW_DISABLED) | (r->step ? DXL_ROW_ARROWS : 0);
        int y = top + i * m->item_height;
        char value[160] = "";
        if (r->value) r->value(s, r, value, sizeof value);
        if (r->slider)
            dxl_ui_slider_row(s->ui, y, r->label, r->slider(s, r), value, sel, flags);
        else
            dxl_ui_row(s->ui, y, r->label, value[0] ? value : NULL, sel, flags);
    }
    /* More above / below: a dim marker in the gutter. */
    if (*scroll > 0)
        dxl_ui_text(s->ui, m->pad / 4, top, "^", DXL_TXT_FOOTER, DXL_PAL.text_dim);
    if (*scroll + per < n)
        dxl_ui_text(s->ui, m->pad / 4, top + (per - 1) * m->item_height, "v",
                    DXL_TXT_FOOTER, DXL_PAL.text_dim);

    const dxl_row *r = vis[*cur];
    char why[256], text[512];
    if (!row_enabled(s, r, why, sizeof why) && why[0]) {
        draw_help(s, htop, why, 1);
    } else {
        text[0] = '\0';
        if (r->describe) r->describe(s, r, text, sizeof text);
        draw_help(s, htop, text[0] ? text : r->help, 0);
    }
}

static void rows_act(dxl_session *s, dxl_act a) {
    const dxl_row *vis[MAX_ROWS];
    int n = visible_rows(s, rows_for(s->tab), vis);
    int *cur = &s->cursor[s->tab];
    if (n == 0) return;
    if (*cur >= n) *cur = n - 1;
    const dxl_row *r = vis[*cur];
    char why[256];
    int en = row_enabled(s, r, why, sizeof why);

    switch (a) {
    case DXL_ACT_UP:   *cur = (*cur + n - 1) % n; s->notice_frames = 0; break;
    case DXL_ACT_DOWN: *cur = (*cur + 1) % n;     s->notice_frames = 0; break;
    case DXL_ACT_LEFT:
    case DXL_ACT_RIGHT:
        if (en && r->step) r->step(s, r, a == DXL_ACT_LEFT ? -1 : 1);
        break;
    case DXL_ACT_CONFIRM:
        if (!en) { notice(s, 1, "%s", why[0] ? why : "Not available here."); break; }
        if (r->activate) r->activate(s, r);
        else if (r->step) r->step(s, r, +1);
        break;
    case DXL_ACT_ALT:
        if (en && r->reset) {
            r->reset(s, r);
            notice(s, 0, "%s set back to its default.", r->label);
        }
        break;
    case DXL_ACT_BACK:
        switch_tab(s, DXL_TAB_PLAY);
        break;
    default:
        break;
    }
}

/* ---- overlays ---------------------------------------------------------- */

static void renderers_draw(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    const dxl_renderer_list *rl = &s->app->renderers;
    int w = dxl_ui_width(s->ui), h = dxl_ui_height(s->ui);
    SDL_Rect panel = { w / 8, h / 8, w * 3 / 4, h * 3 / 4 };
    dxl_ui_overlay_panel(s->ui, panel);

    int x = panel.x + m->pad / 2, y = panel.y + m->pad / 2;
    dxl_ui_text(s->ui, x, y, s->ovl_title, DXL_TXT_TITLE, DXL_PAL.accent);
    y += dxl_ui_line_height(s->ui, DXL_TXT_TITLE) + m->pad / 4;

    /* Rows use the full width helper, so draw them inside a narrowed frame
     * by offsetting through the panel's own margins. */
    int cur = dxl_app_current_renderer(s->app);
    for (size_t i = 0; i < rl->count; i++) {
        const dxl_renderer *r = &rl->items[i];
        int sel = (int)i == s->ovl_cursor;
        const char *value = (int)i == cur ? "In use" : r->selectable ? "" : "Unavailable";
        int ry = y + (int)i * m->item_height;
        if (sel) {
            dxl_ui_fill(s->ui, (SDL_Rect){ panel.x + 8, ry, panel.w - 16, m->item_height },
                        DXL_PAL.panel_sel);
            dxl_ui_fill(s->ui, (SDL_Rect){ panel.x + 8, ry, 4, m->item_height }, DXL_PAL.accent);
        }
        int lh = dxl_ui_line_height(s->ui, DXL_TXT_ITEM);
        SDL_Color c = !r->selectable ? DXL_PAL.text_dim : sel ? DXL_PAL.accent : DXL_PAL.text;
        dxl_ui_text(s->ui, x + m->pad / 4, ry + (m->item_height - lh) / 2, r->label, DXL_TXT_ITEM, c);
        if (value[0]) {
            int vw = dxl_ui_text_width(s->ui, value, DXL_TXT_ITEM);
            dxl_ui_text(s->ui, panel.x + panel.w - m->pad / 2 - vw,
                        ry + (m->item_height - lh) / 2, value, DXL_TXT_ITEM,
                        (int)i == cur ? DXL_PAL.ok : DXL_PAL.text_dim);
        }
    }
    y += (int)rl->count * m->item_height + m->pad / 2;

    if (s->ovl_cursor >= 0 && (size_t)s->ovl_cursor < rl->count) {
        const dxl_renderer *r = &rl->items[s->ovl_cursor];
        int tw = panel.w - m->pad;
        y = dxl_ui_paragraph_n(s->ui, x, y, tw, r->status, DXL_TXT_BODY,
                               r->selectable ? DXL_PAL.ok : DXL_PAL.warn, 2);
        y += m->pad / 4;
        dxl_ui_paragraph_n(s->ui, x, y, tw, r->description, DXL_TXT_BODY, DXL_PAL.text_dim, 4);
    }
    if (s->notice_frames > 0)
        dxl_ui_paragraph_n(s->ui, x, panel.y + panel.h - m->pad / 2 -
                           dxl_ui_line_height(s->ui, DXL_TXT_BODY),
                           panel.w - m->pad, s->notice, DXL_TXT_BODY,
                           s->notice_bad ? DXL_PAL.bad : DXL_PAL.ok, 1);
}

static void renderers_act(dxl_session *s, dxl_act a) {
    int count = (int)s->app->renderers.count;
    switch (a) {
    case DXL_ACT_UP:   if (count) s->ovl_cursor = (s->ovl_cursor + count - 1) % count; s->notice_frames = 0; break;
    case DXL_ACT_DOWN: if (count) s->ovl_cursor = (s->ovl_cursor + 1) % count;         s->notice_frames = 0; break;
    case DXL_ACT_CONFIRM: {
        if (s->ovl_cursor < 0 || s->ovl_cursor >= count) break;
        const dxl_renderer *r = &s->app->renderers.items[s->ovl_cursor];
        if (dxl_app_choose_renderer(s->app, s->ovl_cursor) != 0) {
            notice(s, 1, "%s can't be used: %s", r->label, r->status);
            break;
        }
        close_overlay(s);
        notice(s, 0, "Renderer set to %s.", r->label);
        break;
    }
    case DXL_ACT_BACK:
    case DXL_ACT_QUIT:
        close_overlay(s);
        break;
    default:
        break;
    }
}

static void confirm_draw(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    int w = dxl_ui_width(s->ui), h = dxl_ui_height(s->ui);
    SDL_Rect panel = { w / 6, h / 5, w * 2 / 3, h * 3 / 5 };
    dxl_ui_overlay_panel(s->ui, panel);
    int x = panel.x + m->pad / 2, y = panel.y + m->pad / 2;
    dxl_ui_text(s->ui, x, y, s->ovl_title, DXL_TXT_TITLE, DXL_PAL.accent);
    y += dxl_ui_line_height(s->ui, DXL_TXT_TITLE) + m->pad / 4;
    dxl_ui_paragraph(s->ui, x, y, panel.w - m->pad, s->confirm_body, DXL_TXT_BODY, DXL_PAL.text);

    const char *opts[2] = { "Cancel", s->confirm_yes };
    int by = panel.y + panel.h - m->pad / 2 - 2 * m->item_height;
    for (int i = 0; i < 2; i++) {
        int sel = i == s->ovl_cursor;
        int ry = by + i * m->item_height;
        if (sel) {
            dxl_ui_fill(s->ui, (SDL_Rect){ panel.x + 8, ry, panel.w - 16, m->item_height },
                        DXL_PAL.panel_sel);
            dxl_ui_fill(s->ui, (SDL_Rect){ panel.x + 8, ry, 4, m->item_height }, DXL_PAL.accent);
        }
        int lh = dxl_ui_line_height(s->ui, DXL_TXT_ITEM);
        dxl_ui_text(s->ui, x + m->pad / 4, ry + (m->item_height - lh) / 2, opts[i], DXL_TXT_ITEM,
                    sel ? (i == 1 ? DXL_PAL.bad : DXL_PAL.accent) : DXL_PAL.text);
    }
}

static void confirm_act(dxl_session *s, dxl_act a) {
    switch (a) {
    case DXL_ACT_UP: case DXL_ACT_DOWN: case DXL_ACT_LEFT: case DXL_ACT_RIGHT:
        s->ovl_cursor = !s->ovl_cursor;
        break;
    case DXL_ACT_CONFIRM: {
        dxl_confirm_fn fn = s->ovl_cursor == 1 ? s->confirm_fn : NULL;
        close_overlay(s);
        if (fn) fn(s);
        break;
    }
    case DXL_ACT_BACK:
    case DXL_ACT_QUIT:
        close_overlay(s);
        break;
    default:
        break;
    }
}

static int text_per_page(dxl_session *s) {
    int lh = dxl_ui_line_height(s->ui, DXL_TXT_FOOTER);
    int per = (text_panel(s).h - 3 * dxl_ui_line_height(s->ui, DXL_TXT_ITEM)) / lh;
    return per > 1 ? per : 1;
}

static void text_draw(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    SDL_Rect panel = text_panel(s);
    dxl_ui_overlay_panel(s->ui, panel);
    int x = panel.x + m->pad / 2, y = panel.y + m->pad / 4;
    dxl_ui_text(s->ui, x, y, s->ovl_title, DXL_TXT_ITEM, DXL_PAL.accent);
    y += dxl_ui_line_height(s->ui, DXL_TXT_ITEM) + m->pad / 4;

    int per = text_per_page(s), lh = dxl_ui_line_height(s->ui, DXL_TXT_FOOTER);
    for (int i = 0; i < per && s->ovl_scroll + i < s->ovl_nlines; i++) {
        const char *line = s->ovl_lines[s->ovl_scroll + i];
        const char *tab = strchr(line, '\t');
        if (!tab) {
            dxl_ui_text(s->ui, x, y + i * lh, line, DXL_TXT_FOOTER, DXL_PAL.text);
            continue;
        }
        char left[256];
        snprintf(left, sizeof left, "%.*s", (int)(tab - line), line);
        dxl_ui_text(s->ui, x, y + i * lh, left, DXL_TXT_FOOTER, DXL_PAL.text_dim);
        dxl_ui_text(s->ui, x + text_column(s), y + i * lh, tab + 1, DXL_TXT_FOOTER, DXL_PAL.text);
    }
    if (s->ovl_nlines == 0)
        dxl_ui_text(s->ui, x, y, "(empty)", DXL_TXT_FOOTER, DXL_PAL.text_dim);

    char pos[64];
    snprintf(pos, sizeof pos, "%d-%d of %d", s->ovl_nlines ? s->ovl_scroll + 1 : 0,
             s->ovl_scroll + per < s->ovl_nlines ? s->ovl_scroll + per : s->ovl_nlines,
             s->ovl_nlines);
    int pw = dxl_ui_text_width(s->ui, pos, DXL_TXT_FOOTER);
    dxl_ui_text(s->ui, panel.x + panel.w - m->pad / 2 - pw, panel.y + m->pad / 4, pos,
                DXL_TXT_FOOTER, DXL_PAL.text_dim);
}

static void text_act(dxl_session *s, dxl_act a) {
    int per = text_per_page(s), max = s->ovl_nlines - per > 0 ? s->ovl_nlines - per : 0;
    switch (a) {
    case DXL_ACT_UP:       s->ovl_scroll--; break;
    case DXL_ACT_DOWN:     s->ovl_scroll++; break;
    case DXL_ACT_LEFT:
    case DXL_ACT_TAB_PREV: s->ovl_scroll -= per; break;
    case DXL_ACT_RIGHT:
    case DXL_ACT_TAB_NEXT: s->ovl_scroll += per; break;
    case DXL_ACT_BACK:
    case DXL_ACT_CONFIRM:
    case DXL_ACT_QUIT:     close_overlay(s); return;
    default: break;
    }
    if (s->ovl_scroll > max) s->ovl_scroll = max;
    if (s->ovl_scroll < 0) s->ovl_scroll = 0;
}

/* ---- the install screen ------------------------------------------------ */

static void install_draw(dxl_session *s) {
    dxl_ui *ui = s->ui;
    const dxl_metrics *m = dxl_ui_metrics(ui);
    int w = dxl_ui_width(ui) - 2 * m->pad;

    int y = dxl_ui_header(ui, "Deus Ex -- game files not found");
    y = dxl_ui_paragraph(ui, m->pad, y, w,
        "Copy your own Deus Ex installation into the folder below, then "
        "choose Check again. The launcher needs these files; it does not "
        "include them.", DXL_TXT_BODY, DXL_PAL.text);
    y += m->line_gap / 2;

    dxl_ui_text(ui, m->pad, y, s->app->game_dir, DXL_TXT_BODY, DXL_PAL.accent);
    y += dxl_ui_line_height(ui, DXL_TXT_BODY) + m->line_gap / 2;

    for (int i = 0; i < s->app->install.item_count; i++) {
        const dxl_install_item *it = &s->app->install.items[i];
        char line[256];
        snprintf(line, sizeof line, "%s  %s", it->found ? "present" : "MISSING",
                 it->relative);
        dxl_ui_text(ui, m->pad, y, line, DXL_TXT_BODY, it->found ? DXL_PAL.ok : DXL_PAL.bad);
        y += dxl_ui_line_height(ui, DXL_TXT_BODY);
    }
    y += m->line_gap / 2;

    static const char *items[] = { "Check again", "Exit" };
    for (int i = 0; i < 2; i++)
        dxl_ui_row(ui, y + i * m->item_height, items[i], NULL, s->install_cursor == i, 0);

    static const char *const hints[] = { "A", "Select", "B", "Exit" };
    dxl_ui_footer(ui, hints, 2);
}

static void enter_home(dxl_session *s);

static void install_act(dxl_session *s, dxl_act a) {
    switch (a) {
    case DXL_ACT_UP: case DXL_ACT_DOWN:
        s->install_cursor = !s->install_cursor;
        break;
    case DXL_ACT_CONFIRM:
        if (s->install_cursor == 1) { s->end = DXL_END_QUIT; break; }
        dxl_app_recheck_install(s->app);
        if (dxl_app_needs_install(s->app)) break;
        /* With the files in place this is a first run: go home. */
        s->installing = 0;
        enter_home(s);
        break;
    case DXL_ACT_BACK:
    case DXL_ACT_QUIT:
        s->end = DXL_END_QUIT;
        break;
    default:
        break;
    }
}

/* ---- session ----------------------------------------------------------- */

static void enter_home(dxl_session *s) {
    dxl_app *app = s->app;
    s->crashed = dxl_sentinel_exists(&app->sentinel);
    s->crash_detail[0] = '\0';
    if (s->crashed) last_engine_error(app, s->crash_detail, sizeof s->crash_detail);

    switch (app->decision.screen) {
    case DXL_SCREEN_MAIN_SAFE:       s->tab = DXL_TAB_SYSTEM; break;
    case DXL_SCREEN_RENDERER_FIRST:
    case DXL_SCREEN_RENDERER_VIDEO:  s->tab = DXL_TAB_VIDEO;  break;
    case DXL_SCREEN_MAIN_RECOVERY:
    case DXL_SCREEN_NONE:
    default:                         s->tab = DXL_TAB_PLAY;   break;
    }
    s->cursor[DXL_TAB_PLAY] = tab_play_initial_cursor(s);
}

void dxl_session_start(dxl_session *s, dxl_app *app, dxl_ui *ui) {
    memset(s, 0, sizeof *s);
    s->app = app;
    s->ui  = ui;
    if (dxl_app_needs_install(app)) { s->installing = 1; return; }
    enter_home(s);
}

void dxl_session_free(dxl_session *s) { close_overlay(s); }

static void footer_for(dxl_session *s) {
    static const char *const f_rend[]    = { "A", "Choose", "B", "Close" };
    static const char *const f_confirm[] = { "A", "Choose", "B", "Cancel" };
    static const char *const f_text[]    = { "Up/Down", "Scroll", "L1/R1", "Page", "B", "Close" };
    static const char *const f_remap[]   = { "A", "Change", "X", "Default", "B", "Done" };
    static const char *const f_actions[] = { "A", "Choose", "L1/R1", "Page", "B", "Back" };
    static const char *const f_play[]    = { "A", "Select", "L1/R1", "Tabs", "START", "Play",
                                             "SELECT", "Quit" };

    switch (s->overlay) {
    case DXL_OVL_RENDERERS: dxl_ui_footer(s->ui, f_rend, 2); return;
    case DXL_OVL_CONFIRM:   dxl_ui_footer(s->ui, f_confirm, 2); return;
    case DXL_OVL_TEXT:      dxl_ui_footer(s->ui, f_text, 3); return;
    case DXL_OVL_REMAP:     dxl_ui_footer(s->ui, f_remap, 3); return;
    case DXL_OVL_ACTIONS:   dxl_ui_footer(s->ui, f_actions, 3); return;
    case DXL_OVL_NONE:      break;
    }
    if (s->tab == DXL_TAB_PLAY) { dxl_ui_footer(s->ui, f_play, 4); return; }

    /* Only offer what the focused row can do. */
    const dxl_row *vis[MAX_ROWS];
    int n = visible_rows(s, rows_for(s->tab), vis), cur = s->cursor[s->tab];
    const dxl_row *r = (n > 0 && cur >= 0 && cur < n) ? vis[cur] : NULL;
    int en = r && row_enabled(s, r, NULL, 0);
    const char *pairs[10];
    int k = 0;
    if (en && (r->activate || r->step)) { pairs[k++] = "A"; pairs[k++] = r->activate ? "Select" : "Change"; }
    if (en && r->step)  { pairs[k++] = "Left/Right"; pairs[k++] = "Change"; }
    if (en && r->reset) { pairs[k++] = "X"; pairs[k++] = "Default"; }
    pairs[k++] = "B";     pairs[k++] = "Home";
    pairs[k++] = "START"; pairs[k++] = "Play";
    dxl_ui_footer(s->ui, pairs, k / 2);
}

static int footer_top(dxl_session *s) {
    const dxl_metrics *m = dxl_ui_metrics(s->ui);
    return dxl_ui_height(s->ui) - m->pad / 2 - dxl_ui_line_height(s->ui, DXL_TXT_FOOTER) - m->pad / 4;
}

void dxl_session_draw(dxl_session *s) {
    dxl_ui_frame_begin(s->ui);
    if (s->installing) {
        install_draw(s);
    } else {
        int top = dxl_ui_tabbar(s->ui, tab_names, DXL_TAB_COUNT, s->tab);
        int bottom = footer_top(s) - dxl_ui_metrics(s->ui)->pad / 4;
        if (s->tab == DXL_TAB_PLAY) tab_play_draw(s, top, bottom);
        else rows_draw(s, top, bottom);

        switch (s->overlay) {
        case DXL_OVL_RENDERERS: renderers_draw(s); break;
        case DXL_OVL_CONFIRM:   confirm_draw(s);   break;
        case DXL_OVL_TEXT:      text_draw(s);      break;
        case DXL_OVL_REMAP:     remap_draw(s);     break;
        case DXL_OVL_ACTIONS:   actions_draw(s);   break;
        case DXL_OVL_NONE:      break;
        }
        footer_for(s);
    }
    dxl_ui_frame_end(s->ui);
    if (s->notice_frames > 0) s->notice_frames--;
}

void dxl_session_act(dxl_session *s, dxl_act a) {
    if (s->installing) { install_act(s, a); return; }

    switch (s->overlay) {
    case DXL_OVL_RENDERERS: renderers_act(s, a); return;
    case DXL_OVL_CONFIRM:   confirm_act(s, a);   return;
    case DXL_OVL_TEXT:      text_act(s, a);      return;
    case DXL_OVL_REMAP:     remap_act(s, a);     return;
    case DXL_OVL_ACTIONS:   actions_act(s, a);   return;
    case DXL_OVL_NONE:      break;
    }

    switch (a) {
    case DXL_ACT_START: s->end = DXL_END_LAUNCH; return;
    case DXL_ACT_QUIT:  s->end = DXL_END_QUIT;   return;
    case DXL_ACT_TAB_PREV: switch_tab(s, (dxl_tab)((s->tab + DXL_TAB_COUNT - 1) % DXL_TAB_COUNT)); return;
    case DXL_ACT_TAB_NEXT: switch_tab(s, (dxl_tab)((s->tab + 1) % DXL_TAB_COUNT)); return;
    default: break;
    }
    if (s->tab == DXL_TAB_PLAY) tab_play_act(s, a);
    else rows_act(s, a);
}

int dxl_session_step(dxl_session *s) {
    if (s->end != DXL_END_NONE) return 1;
    dxl_act a;
    while ((a = dxl_ui_poll(s->ui)) != DXL_ACT_NONE) {
        dxl_session_act(s, a);
        if (s->end != DXL_END_NONE) return 1;
    }
    dxl_session_draw(s);
    return 0;
}
