#include "core/servers.h"
#include "core/gamespy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dxl_server_init(dxl_server *s, const char *address, int query_port) {
    memset(s, 0, sizeof *s);
    snprintf(s->address, sizeof s->address, "%s", address);
    s->query_port = query_port;
    s->port = query_port > 1 ? query_port - 1 : query_port;
    s->ping_ms = -1;
}

void dxl_server_free(dxl_server *s) {
    free(s->status);
    s->status = NULL;
}

static int number(const char *status, const char *key, int fallback) {
    char v[32];
    return dxl_gs_value(status, key, v, sizeof v) && v[0] ? atoi(v) : fallback;
}

void dxl_server_take_status(dxl_server *s, const char *status) {
    free(s->status);
    s->status = dxl_xstrdup(status);
    s->state = DXL_SERVER_ANSWERED;
    char v[32];
    dxl_gs_value(status, "hostname", s->name, sizeof s->name);
    dxl_gs_value(status, "mapname", s->map, sizeof s->map);
    dxl_gs_value(status, "maptitle", s->map_title, sizeof s->map_title);
    dxl_gs_value(status, "gametype", s->game_type, sizeof s->game_type);
    int port = number(status, "hostport", 0);
    if (port > 0 && port <= 65535) s->port = port;
    s->players = number(status, "numplayers", 0);
    s->max_players = number(status, "maxplayers", 0);
    s->version = number(status, "gamever", 0);
    s->min_version = number(status, "minnetver", s->version);
    s->password = dxl_gs_value(status, "password", v, sizeof v) && (!dxl_stricmp(v, "True") || !strcmp(v, "1"));
}

int dxl_server_joinable(const dxl_server *s) {
    /* The game joins a server whose version is its own or newer, which
     * takes clients of its version (minnetver). */
    return s->state == DXL_SERVER_ANSWERED && s->version >= DXL_NET_VERSION && s->min_version <= DXL_NET_VERSION;
}

int dxl_server_player(const dxl_server *s, int i, char *name, size_t size, int *frags, int *ping) {
    if (!s->status) return 0;
    char key[32];
    snprintf(key, sizeof key, "player_%d", i);
    if (!dxl_gs_value(s->status, key, name, size)) return 0;
    snprintf(key, sizeof key, "frags_%d", i);
    if (frags) *frags = number(s->status, key, 0);
    snprintf(key, sizeof key, "ping_%d", i);
    if (ping) *ping = number(s->status, key, 0);
    return 1;
}

/* The basic information's keys, and a player's (each ending _<n>). */
static int not_a_rule(const char *key) {
    static const char *const basic[] = {
        "gamename", "gamever", "minnetver", "location", "hostname", "hostport", "maptitle", "mapname",
        "gametype", "numplayers", "maxplayers", "gamemode", "password", "queryid", "final",
    };
    for (size_t i = 0; i < sizeof basic / sizeof *basic; i++)
        if (!dxl_stricmp(key, basic[i])) return 1;
    static const char *const player[] = { "player_", "frags_", "ping_", "team_", "mesh_", "skin_", "face_" };
    for (size_t i = 0; i < sizeof player / sizeof *player; i++)
        if (!dxl_strnicmp(key, player[i], strlen(player[i]))) return 1;
    return 0;
}

int dxl_server_rule(const dxl_server *s, int i, char *key, size_t key_size, char *value, size_t value_size) {
    if (!s->status) return 0;
    const char *p = s->status;
    char k[64], v[256];
    for (int n = 0; dxl_gs_next(&p, k, sizeof k, v, sizeof v);) {
        if (!k[0] || not_a_rule(k)) continue;
        if (n++ == i) {
            snprintf(key, key_size, "%s", k);
            snprintf(value, value_size, "%s", v);
            return 1;
        }
    }
    return 0;
}

/* -1, 0 or 1 as x is less, the same or more: no number a server sends
 * overflows it. */
static int order(int x, int y) { return (x > y) - (x < y); }

int dxl_server_compare(const dxl_server *a, const dxl_server *b, dxl_server_sort by) {
    int aa = a->state == DXL_SERVER_ANSWERED, ba = b->state == DXL_SERVER_ANSWERED;
    if (aa != ba) return ba - aa;
    int c = 0;
    if (aa) {
        switch (by) {
        case DXL_SORT_PLAYERS: c = order(b->players, a->players); break;
        case DXL_SORT_NAME: c = dxl_stricmp(a->name, b->name); break;
        default: c = order(a->ping_ms, b->ping_ms); break;
        }
    }
    if (!c) c = strcmp(a->address, b->address);
    if (!c) c = order(a->query_port, b->query_port);
    return c;
}

int dxl_server_shown(const dxl_server *s, const dxl_server_filter *f, const char *favourites) {
    if (!f) return 1;
    if (f->favourites && !dxl_favourite_has(favourites, s->address, s->port)) return 0;
    if (f->lan && !s->lan) return 0;
    /* A server not answered yet is shown till it answers, but under a
     * filter that asks of its answer. */
    int answered = s->state == DXL_SERVER_ANSWERED;
    if (f->not_empty && (!answered || s->players <= 0)) return 0;
    if (f->not_full && (!answered || (s->max_players > 0 && s->players >= s->max_players))) return 0;
    if (f->no_password && (!answered || s->password)) return 0;
    return 1;
}

void dxl_server_join_url(const char *host, int port, int lan, const char *password, char *out, size_t size) {
    snprintf(out, size, "%s:%d%s%s%s", host, port, lan ? "?lan" : "", password && *password ? "?password=" : "",
             password && *password ? password : "");
}

int dxl_server_link(const char *link, char *out, size_t size) {
    if (!link || dxl_strnicmp(link, "deusex://", 9) != 0) return 0;
    char host[256];
    snprintf(host, sizeof host, "%s", link + 9);
    host[strcspn(host, "/?# ")] = '\0';
    char *colon = strrchr(host, ':');
    int port = 7790;
    if (colon) {
        *colon = '\0';
        port = atoi(colon + 1);
    }
    if (!host[0] || port <= 0 || port > 65535) return 0;
    snprintf(out, size, "%s:%d", host, port);
    return 1;
}

int dxl_favourite_has(const char *list, const char *address, int port) {
    if (!list || !address) return 0;
    char want[96];
    snprintf(want, sizeof want, "%s:%d", address, port);
    size_t n = strlen(want);
    for (const char *p = list; (p = strstr(p, want)); p += n)
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || !p[n])) return 1;
    return 0;
}

char *dxl_favourite_toggle(const char *list, const char *address, int port) {
    char want[96];
    snprintf(want, sizeof want, "%s:%d", address, port);
    int had = dxl_favourite_has(list, address, port);
    dxl_buf b;
    dxl_buf_init(&b);
    /* Every word but this one, then this one when it was not there. */
    char *copy = dxl_xstrdup(list ? list : "");
    char *save = NULL;
    for (char *w = strtok_r(copy, " ", &save); w; w = strtok_r(NULL, " ", &save))
        if (strcmp(w, want) != 0) dxl_buf_word(&b, w);
    free(copy);
    if (!had) dxl_buf_word(&b, want);
    return b.data ? b.data : dxl_xstrdup("");
}
