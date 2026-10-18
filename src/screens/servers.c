/* Multiplayer: the servers the profile's master server lists
 * ([DeusEx.MenuScreenJoinGame] MasterServerAddress; app/browser.h) and the
 * profile's favourites, each as it answers -- its name, map, game, players
 * and ping -- in the order they came while the look is on, then by the
 * sort chosen; the filter chosen. The one chosen beside them: its players,
 * Join, with its password when it asks for one, and favourite or not. An
 * address typed is joined as it is. */
#define _GNU_SOURCE
#include "screens/shell.h"
#include "core/log.h"
#include "ui/controls.h"
#include "ui/draw.h"
#include "ui/text.h"
#include "ui/widgets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROW_H  54
#define SIDE_W 360
#define LIVE_MASTER "master.333networks.com"
/* A server's game port, when an address names none: the game's own. */
#define GAME_PORT 7790

static const char *const sorts[DXL_SORT_COUNT] = { "By ping", "By players", "By name" };
enum { SHOW_ALL, SHOW_PLAYERS, SHOW_NOT_FULL, SHOW_OPEN, SHOW_FAVOURITES, SHOW_COUNT };
static const char *const shows[SHOW_COUNT] = { "All servers", "With players", "Not full", "No password", "Favourites" };

static dxl_server_filter filter_of(int show) {
    dxl_server_filter f = { 0 };
    f.not_empty = show == SHOW_PLAYERS;
    f.not_full = show == SHOW_NOT_FULL;
    f.no_password = show == SHOW_OPEN;
    f.favourites = show == SHOW_FAVOURITES;
    return f;
}

static const char *favourites_of(const dxl_shell *s) {
    const char *f = s->run ? dxl_profile_get(&s->run->session.profile, "Favourites") : NULL;
    return f ? f : "";
}

static void key_of(const dxl_server *v, char *out, size_t size) {
    snprintf(out, size, "%s:%d", v->address, v->query_port);
}

static dxl_server *find(dxl_shell *s, const char *address, int port) {
    for (int i = 0; s->servers && i < s->server_count; i++)
        if (s->servers[i].query_port == port && !strcmp(s->servers[i].address, address)) return &s->servers[i];
    return NULL;
}

/* ---- the look ------------------------------------------------------------ */

void dxl_shell_server_listed(dxl_shell *s, const char *address, int port) {
    if (!s->servers || s->server_count >= DXL_SERVERS_MAX || find(s, address, port)) return;
    dxl_server_init(&s->servers[s->server_count++], address, port);
}

void dxl_shell_server_answered(dxl_shell *s, const char *address, int port, int ping, const char *status) {
    dxl_server *v = find(s, address, port);
    if (!v) {
        dxl_shell_server_listed(s, address, port);
        v = find(s, address, port);
    }
    if (!v) return;
    dxl_server_take_status(v, status);
    v->ping_ms = ping;
}

void dxl_shell_browsed(dxl_shell *s, const char *error) {
    int answers = 0;
    for (int i = 0; i < s->server_count; i++) {
        if (s->servers[i].state == DXL_SERVER_ANSWERED) answers++;
        else s->servers[i].state = DXL_SERVER_SILENT;
    }
    s->browsing = 0;
    s->browsed = 1;
    s->servers_sorted = 0;
    snprintf(s->browse_error, sizeof s->browse_error, "%s", error ? error : "");
    dxl_log("multiplayer: %d server(s), %d answered%s%s", s->server_count, answers, error ? "; " : "",
            error ? error : "");
}

/* The profile's master server, as the game takes it. */
static const char *master_of(dxl_shell *s) {
    dxl_ini *sys = s->run && s->run->config ? dxl_config_ini(s->run->config) : NULL;
    const char *m = sys ? dxl_ini_get(sys, "DeusEx.MenuScreenJoinGame", "MasterServerAddress") : NULL;
    return m && *m ? m : LIVE_MASTER;
}

void dxl_shell_browse(dxl_shell *s) {
    if (!s->servers) s->servers = dxl_xmalloc(sizeof *s->servers * DXL_SERVERS_MAX);
    for (int i = 0; i < s->server_count; i++) dxl_server_free(&s->servers[i]);
    s->server_count = 0;
    s->browse_error[0] = '\0';
    s->servers_sorted = 0;
    s->browsed = 0;
    if (!s->start_browse || !s->run) {
        /* Offscreen: nothing asked. */
        s->browsing = 0;
        s->browsed = 1;
        return;
    }
    /* The favourites by their query ports, the next after the game's. */
    char *list = dxl_xstrdup(favourites_of(s));
    const char *also[65];
    char ports[64][96];
    int n = 0;
    char *save = NULL;
    for (char *w = strtok_r(list, " ", &save); w && n < 64; w = strtok_r(NULL, " ", &save)) {
        char *colon = strrchr(w, ':');
        if (!colon) continue;
        *colon = '\0';
        snprintf(ports[n], sizeof ports[n], "%s:%d", w, atoi(colon + 1) + 1);
        also[n] = ports[n];
        n++;
    }
    also[n] = NULL;
    const char *master = master_of(s);
    s->browsing = 1;
    dxl_log("multiplayer: asking %s, and %d favourite(s)", master, n);
    s->start_browse(s, master, also);
    free(list);
}

void dxl_page_servers_enter(dxl_shell *s) {
    if (!s->servers) s->servers = dxl_xmalloc(sizeof *s->servers * DXL_SERVERS_MAX);
    if (!s->browsed && !s->browsing) dxl_shell_browse(s);
}

void dxl_shell_join(dxl_shell *s, const char *host, int port, const char *password) {
    if (!s->run) return;
    if (password && *password)
        snprintf(s->run->url, sizeof s->run->url, "%s:%d?password=%s", host, port, password);
    else
        snprintf(s->run->url, sizeof s->run->url, "%s:%d", host, port);
    dxl_log("play: joining %s:%d%s", host, port, password && *password ? ", with its password" : "");
    s->result = 1;
}

void dxl_shell_join_link(dxl_shell *s, const char *host_port) {
    snprintf(s->join_link, sizeof s->join_link, "%s", host_port);
    dxl_shell_go(s, DXL_PAGE_SERVERS);
}

/* host or host:port, a game's port given or the game's own, into host. */
static int address_of(const char *text, char *host, size_t size, int *port) {
    char t[128];
    snprintf(t, sizeof t, "%s", text);
    char *a = dxl_trim(t);
    if (!strncmp(a, "deusex://", 9)) a += 9;
    a[strcspn(a, "/ ")] = '\0';
    *port = GAME_PORT;
    char *colon = strrchr(a, ':');
    if (colon) {
        *colon = '\0';
        *port = atoi(colon + 1);
    }
    snprintf(host, size, "%s", a);
    return host[0] && *port > 0 && *port <= 65535;
}

/* ---- the page ------------------------------------------------------------- */

static int sort_key;

static int by_sort(const void *a, const void *b) { return dxl_server_compare(a, b, (dxl_server_sort)sort_key); }

static void sort(dxl_shell *s) {
    sort_key = s->server_sort;
    if (s->server_count > 1) qsort(s->servers, (size_t)s->server_count, sizeof *s->servers, by_sort);
}

/* A server's row: its name, then map, game, players, ping. */
static dxl_rgba describe(const dxl_shell *s, const dxl_server *v, int favourite, char *title, size_t tsize,
                         char *detail, size_t dsize) {
    const dxl_theme *t = &s->ui.theme;
    if (v->state != DXL_SERVER_ANSWERED) {
        snprintf(title, tsize, "%s:%d", v->address, v->port);
        snprintf(detail, dsize, "%s%s", v->state == DXL_SERVER_SILENT ? "no answer" : "asking...",
                 favourite ? "  \xc2\xb7  a favourite" : "");
        return v->state == DXL_SERVER_SILENT ? t->warn : t->help;
    }
    snprintf(title, tsize, "%s", v->name[0] ? v->name : v->address);
    snprintf(detail, dsize, "%.60s  \xc2\xb7  %.40s  \xc2\xb7  %d/%d  \xc2\xb7  %d ms%s%s%s", v->map, v->game_type, v->players,
             v->max_players, v->ping_ms, v->password ? "  \xc2\xb7  password" : "",
             favourite ? "  \xc2\xb7  a favourite" : "", dxl_server_joinable(v) ? "" : "  \xc2\xb7  another version");
    return dxl_server_joinable(v) ? t->help : t->warn;
}

/* What the list's foot says: what the look is doing, or found. */
static void status_line(const dxl_shell *s, int shown, char *out, size_t size) {
    int answers = 0;
    for (int i = 0; i < s->server_count; i++) answers += s->servers[i].state == DXL_SERVER_ANSWERED;
    if (s->browsing)
        snprintf(out, size, "Asking the master server and each server it lists: %d listed, %d answered...",
                 s->server_count, answers);
    else if (s->browse_error[0])
        snprintf(out, size, "The master server gave no list: %s.%s", s->browse_error,
                 s->server_count ? " The favourites were asked all the same." : "");
    else
        snprintf(out, size, "%d listed, %d answered%s.", s->server_count, answers,
                 shown < s->server_count ? ", the rest left out by the filter" : "");
}

/* One of n choices on a button, "Sort: By ping": left and right step
 * through them, choosing it lists them. The choice as it is now. */
static int choice(dxl_shell *s, uint32_t id, dxl_rect r, const char *what, const char *const *labels, int n, int now) {
    dxl_ui *ui = &s->ui;
    int a = dxl_shell_answer(s, id);
    if (a >= 0 && a < n) now = a;
    now = (now + dxl_ui_take_lr(ui, id) % n + n) % n;
    char label[96];
    snprintf(label, sizeof label, "%s: %s", what, labels[now]);
    if (dxl_button_id(ui, id, r, label, DXL_W_LEFT)) dxl_shell_pick(s, id, what, labels, n, now);
    return now;
}

/* v a favourite, or not one any more: the profile's list, kept. */
static void toggle_favourite(dxl_shell *s, const dxl_server *v, const char *fav, int favourite) {
    dxl_session *ss = &s->run->session;
    char *list = dxl_favourite_toggle(fav, v->address, v->port);
    dxl_profile_set(&ss->profile, "Favourites", list);
    dxl_err e;
    if (dxl_profile_save(&ss->profile, &e) != 0) dxl_log("warning: %s", dxl_err_msg(&e));
    dxl_log("multiplayer: %s:%d %s the favourites", v->address, v->port, favourite ? "off" : "on");
    dxl_shell_toast(s, favourite ? "%s is not a favourite now." : "%s is a favourite now.",
                    v->name[0] ? v->name : v->address);
    free(list);
}

void dxl_page_servers(dxl_shell *s, dxl_rect r) {
    dxl_ui *ui = &s->ui;
    const dxl_theme *t = &ui->theme;
    dxl_session *ss = &s->run->session;
    int playable = ss->install.ok && s->engine_ok;
    float w = r.w < 1240 ? r.w : 1240;
    float side_w = w * 0.36f < SIDE_W ? w * 0.36f : SIDE_W;
    dxl_rect area = { r.x, r.y + 6, w - side_w - 20, r.h - 6 };
    dxl_rect side = { r.x + w - side_w, r.y + 6, side_w, r.h - 6 };
    const char *fav = favourites_of(s);
    const char *why = NULL;

    /* The look over: the list by the sort, once. */
    int resorted = 0;
    if (!s->browsing && !s->servers_sorted) {
        sort(s);
        s->servers_sorted = resorted = 1;
    }

    /* Its top: Refresh, the sort, the filter. */
    dxl_rect top = dxl_cut_top(&area, 40);
    dxl_cut_top(&area, 10);
    int refresh = dxl_button(ui, "servers refresh", dxl_cut_left(&top, 140), s->browsing ? "Asking..." : "Refresh",
                             s->browsing ? DXL_W_DISABLED : 0);
    dxl_cut_left(&top, 12);
    dxl_rect sort_r = dxl_cut_left(&top, (top.w - 12) / 2);
    dxl_cut_left(&top, 12);
    uint32_t sort_id = dxl_ui_id(ui, "servers sort"), show_id = dxl_ui_id(ui, "servers show");
    int sort_now = choice(s, sort_id, sort_r, "Sort", sorts, DXL_SORT_COUNT, s->server_sort);
    if (sort_now != s->server_sort) {
        s->server_sort = sort_now;
        if (!s->browsing) {
            sort(s);
            resorted = 1;
        }
    }
    int show = choice(s, show_id, top, "Show", shows, SHOW_COUNT, s->server_show);
    s->server_show = show;
    /* Under it what the look is doing, or did. */
    dxl_rect status_r = dxl_cut_top(&area, 24);
    dxl_cut_top(&area, 6);

    /* Its foot: an address to join, and what the look did. */
    dxl_rect help_r = dxl_cut_bottom(&area, 36);
    dxl_cut_bottom(&area, 6);
    dxl_rect join_row = dxl_cut_bottom(&area, 40);
    dxl_cut_bottom(&area, 10);
    uint32_t address_id = dxl_ui_id(ui, "servers address");
    int typed = dxl_shell_answer(s, address_id) == 1;
    dxl_rect join_b = dxl_cut_right(&join_row, 150);
    dxl_cut_right(&join_row, 12);
    int edited = dxl_text_field(ui, address_id, join_row, "Address", s->join_address, sizeof s->join_address,
                                "host:port", 0);
    if (edited == DXL_TEXT_OSK) dxl_shell_osk(s, address_id, "The server's address", s->join_address, sizeof s->join_address);
    int join_address = dxl_button(ui, "servers join address", join_b, "Join it",
                                  s->join_address[0] && playable ? 0 : DXL_W_DISABLED);
    if (ui->focus == address_id || ui->focus == dxl_ui_id(ui, "servers join address"))
        why = "A server the list does not have: its address, and its port when it is not 7790.";

    /* The servers, the filter's. */
    dxl_server_filter f = filter_of(show);
    int shown[DXL_SERVERS_MAX], count = 0, chosen = -1;
    for (int i = 0; i < s->server_count; i++) {
        if (!dxl_server_shown(&s->servers[i], &f, fav)) continue;
        char k[96];
        key_of(&s->servers[i], k, sizeof k);
        if (!strcmp(k, s->server_chosen)) chosen = count;
        shown[count++] = i;
    }
    dxl_well(ui, area);
    int join = -1;
    if (!count) {
        const char *none = s->browsing ? "Asking the master server for the game's servers..."
                           : s->server_count ? "No server the filter shows."
                                             : "No servers: the master server listed none that answered.";
        dxl_paragraph(ui, dxl_inset(area, 18), DXL_FONT_BODY, none, t->help);
    } else {
        dxl_list list;
        dxl_list_begin(ui, &list, "servers", dxl_inset(area, 4), count, ROW_H, 0);
        /* Sorted again, the chosen where it is now, if it had the focus. */
        if (resorted && chosen >= 0)
            for (int i = 0; i < count; i++)
                if (ui->focus == dxl_list_row_id(&list, i)) {
                    dxl_list_focus(ui, &list, chosen);
                    break;
                }
        for (int i = 0; i < count; i++) {
            dxl_server *v = &s->servers[shown[i]];
            char title[160], detail[300];
            int favourite = dxl_favourite_has(fav, v->address, v->port);
            dxl_rgba c = describe(s, v, favourite, title, sizeof title, detail, sizeof detail);
            int hits = dxl_list_row(ui, &list, i, title, detail, c, DXL_W_MAY_CUT);
            if ((hits & DXL_HIT_FOCUSED) && !resorted) {
                char k[96];
                key_of(v, k, sizeof k);
                /* Another server: the password typed was the other's. */
                if (strcmp(k, s->server_chosen) != 0) s->join_password[0] = '\0';
                snprintf(s->server_chosen, sizeof s->server_chosen, "%s", k);
                chosen = i;
            }
            if (hits & DXL_HIT_FOCUSED)
                why = !playable ? "No game or engine to play with: Home sets it right."
                      : v->state == DXL_SERVER_ANSWERED ? "Enter or a click joins it."
                                                        : "It has not answered: joining it may not work.";
            if (hits & DXL_HIT_ACTIVATED) join = shown[i];
            dxl_rect rr;
            if (i == chosen && !(hits & DXL_HIT_FOCUSED) && dxl_list_rect(&list, i, &rr))
                dxl_fill(ui, (dxl_rect){ rr.x + 4, rr.y + 8, 3, rr.h - 16 }, t->accent);
        }
        dxl_list_end(ui, &list);
    }
    if (chosen < 0 && count) {
        key_of(&s->servers[shown[0]], s->server_chosen, sizeof s->server_chosen);
        chosen = 0;
    }
    dxl_server *v = chosen >= 0 ? &s->servers[shown[chosen]] : NULL;

    /* The one chosen: what it is, its players, Join, favourite. */
    dxl_rect back_r = dxl_cut_bottom(&side, 48);
    dxl_cut_bottom(&side, 10);
    int answered = v && v->state == DXL_SERVER_ANSWERED;
    int favourite = v && dxl_favourite_has(fav, v->address, v->port);
    uint32_t password_id = dxl_ui_id(ui, "servers password");
    int b_join = 0, b_fav = 0, password_kept = 0;
    if (v) {
        dxl_label(ui, dxl_cut_top(&side, 30), DXL_FONT_BUTTON, answered && v->name[0] ? v->name : v->address, t->text_focus,
                  DXL_W_LEFT | DXL_W_MAY_CUT);
        dxl_cut_top(&side, 6);
        char val[200];
        snprintf(val, sizeof val, "%s:%d", v->address, v->port);
        dxl_field(ui, &side, 26, 90, "Address", val, t->text, DXL_W_MAY_CUT);
        if (answered) {
            snprintf(val, sizeof val, "%s%s%s", v->map, v->map_title[0] && strcmp(v->map_title, "Untitled") ? ", " : "",
                     v->map_title[0] && strcmp(v->map_title, "Untitled") ? v->map_title : "");
            dxl_field(ui, &side, 26, 90, "Map", val, t->text, DXL_W_MAY_CUT);
            dxl_field(ui, &side, 26, 90, "Game", v->game_type, t->text, DXL_W_MAY_CUT);
            snprintf(val, sizeof val, "%d of %d, %d ms", v->players, v->max_players, v->ping_ms);
            dxl_field(ui, &side, 26, 90, "Players", val, t->text, DXL_W_MAY_CUT);
            if (!dxl_server_joinable(v)) {
                snprintf(val, sizeof val, "%d, not the game's %d", v->version, DXL_NET_VERSION);
                dxl_field(ui, &side, 26, 90, "Version", val, t->warn, DXL_W_MAY_CUT);
            }
        } else {
            dxl_field(ui, &side, 26, 90, "Status", v->state == DXL_SERVER_SILENT ? "no answer" : "asking...", t->help,
                      DXL_W_MAY_CUT);
        }
        /* Its players, as many as there is room for above what acts. */
        float acts = 48 + 10 + 48 + 10 + (v->password ? 40 + 10 : 0);
        char pname[64];
        int frags = 0, ping = 0;
        for (int i = 0; answered && side.h - 22 > acts + 12 && dxl_server_player(v, i, pname, sizeof pname, &frags, &ping); i++) {
            if (i == 0) dxl_cut_top(&side, 4);
            snprintf(val, sizeof val, "%d frags, %d ms", frags, ping);
            dxl_field(ui, &side, 22, 150, pname, val, t->help, DXL_W_MAY_CUT);
        }
        dxl_rect acts_r = dxl_cut_bottom(&side, acts);
        if (v->password) {
            int pw = dxl_text_field(ui, password_id, dxl_cut_top(&acts_r, 40), "Password", s->join_password,
                                    sizeof s->join_password, NULL, 0);
            if (pw == DXL_TEXT_OSK) dxl_shell_osk(s, password_id, "The server's password", s->join_password,
                                                  sizeof s->join_password);
            password_kept = pw == DXL_TEXT_DONE || dxl_shell_answer(s, password_id) == 1;
            dxl_cut_top(&acts_r, 10);
            if (ui->focus == password_id) why = "The server asks for a password to join it.";
        }
        b_join = dxl_button(ui, "servers join", dxl_cut_top(&acts_r, 48), "Join",
                            DXL_W_PRIMARY | DXL_W_LEFT | (playable ? 0 : DXL_W_DISABLED));
        dxl_cut_top(&acts_r, 10);
        b_fav = dxl_button(ui, "servers favourite", dxl_cut_top(&acts_r, 48),
                           favourite ? "Not a favourite" : "A favourite", DXL_W_LEFT);
        if (ui->focus == dxl_ui_id(ui, "servers favourite"))
            why = favourite ? "Asked each time, listed or not; this takes it off the favourites."
                            : "A favourite is asked each time, whether the master lists it or not.";
    }
    int b_back = dxl_button(ui, "servers back", back_r, "Back", DXL_W_LEFT);

    char line[400];
    status_line(s, count, line, sizeof line);
    dxl_label(ui, status_r, DXL_FONT_SMALL, line, s->browse_error[0] ? t->warn : t->help, DXL_W_LEFT | DXL_W_MAY_CUT);
    if (why) dxl_paragraph_typed(ui, help_r, DXL_FONT_SMALL, why, t->help, ui->focus_since);

    /* A deusex:// link's server: joined once the player says so. */
    uint32_t link_id = dxl_ui_id(ui, "servers link");
    if (dxl_shell_answer(s, link_id) == 1) {
        char host[128];
        int port;
        if (address_of(s->join_link, host, sizeof host, &port)) dxl_shell_join(s, host, port, NULL);
        s->join_link[0] = '\0';
        return;
    } else if (s->join_link[0] && s->dialog == DXL_DIALOG_NONE && !s->answered) {
        char text[300];
        snprintf(text, sizeof text, "A link asks to join the server at %s.%s", s->join_link,
                 playable ? "" : " There is no game or engine to play with yet: Home sets it right.");
        if (playable) dxl_shell_confirm(s, link_id, "Join this server?", text, "Join", "Not now");
        else dxl_shell_message(s, "Not joined", "%s", text);
        s->join_link[0] = '\0';
    }

    int to_join = playable && (join >= 0 || b_join || password_kept || (ui->in.start && v && !ui->text_id));
    int address = playable && (typed || edited == DXL_TEXT_DONE || join_address);
    if (to_join) {
        dxl_server *to = join >= 0 ? &s->servers[join] : v;
        if (to && to->password && !s->join_password[0]) {
            /* Its password first: the field, by the keys or the pad. */
            dxl_ui_focus(ui, password_id);
            dxl_shell_toast(s, "%s asks for a password.", to->name[0] ? to->name : to->address);
        } else if (to) {
            dxl_shell_join(s, to->address, to->port, to->password ? s->join_password : NULL);
        }
    } else if (address) {
        char host[128];
        int port;
        if (address_of(s->join_address, host, sizeof host, &port)) dxl_shell_join(s, host, port, NULL);
        else if (s->join_address[0]) dxl_shell_message(s, "Not an address", "%s is no server's address: a name or "
                                                      "a.b.c.d, and :port when it is not 7790.", s->join_address);
    } else if (refresh) {
        dxl_shell_browse(s);
    } else if (b_fav && v) {
        toggle_favourite(s, v, fav, favourite);
    } else if (b_back || (ui->in.back && !ui->text_id)) {
        dxl_shell_go(s, DXL_PAGE_HOME);
    }
}
