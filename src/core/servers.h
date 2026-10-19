/* The servers a master lists, or a beacon on the local network answers
 * for, each as its answer to \status\ tells it (core/gamespy.h): what the
 * server browser shows of it, sorts it by and filters it with; the URL that
 * joins it; the profile's favourites.
 */
#ifndef DXL_SERVERS_H
#define DXL_SERVERS_H

#include "core/common.h"

typedef enum {
    DXL_SERVER_LISTED,      /* the master listed it; not asked yet, or no answer yet */
    DXL_SERVER_ANSWERED,    /* its status came back whole */
    DXL_SERVER_SILENT       /* no answer in time */
} dxl_server_state;

typedef struct {
    char  address[64];      /* a.b.c.d */
    int   query_port;       /* the master's port for it, its query answerer's */
    int   port;             /* the game's: its hostport, else the query port's one before */
    int   lan;              /* a beacon on the local network answered for it */
    dxl_server_state state;
    int   ping_ms;
    char  name[128];
    char  map[64];          /* mapname, the file's */
    char  map_title[64];    /* maptitle */
    char  game_type[64];
    int   players, max_players;
    int   password;         /* a password to join */
    int   version, min_version;   /* gamever, minnetver: 1100 */
    char *status;           /* the whole answer, \key\value; NULL before it comes */
} dxl_server;

/* The game's net version: a server of another the game does not join. */
#define DXL_NET_VERSION 1100

void dxl_server_init(dxl_server *s, const char *address, int query_port);
/* Its answer, whole: the fields read from it, the text kept. */
void dxl_server_take_status(dxl_server *s, const char *status);
void dxl_server_free(dxl_server *s);
/* The game takes this one: its version and the game's agree. */
int  dxl_server_joinable(const dxl_server *s);

/* Player i of its answer -- player_i, frags_i, ping_i -- 0 past the last. */
int dxl_server_player(const dxl_server *s, int i, char *name, size_t size, int *frags, int *ping);
/* Rule i: a pair of its answer that is neither its basic information nor
 * a player's, in its order; 0 past the last. */
int dxl_server_rule(const dxl_server *s, int i, char *key, size_t key_size, char *value, size_t value_size);

typedef enum { DXL_SORT_PING, DXL_SORT_PLAYERS, DXL_SORT_NAME, DXL_SORT_COUNT } dxl_server_sort;

/* <0, 0, >0 as a comes before b: the answered before the rest, then by
 * the sort -- ping, players (most first), name -- then by address. */
int dxl_server_compare(const dxl_server *a, const dxl_server *b, dxl_server_sort by);

typedef struct {
    int not_empty, not_full, no_password, favourites, lan;
} dxl_server_filter;

/* Whether the filter shows s; favourites, the profile's list. */
int dxl_server_shown(const dxl_server *s, const dxl_server_filter *f, const char *favourites);

/* The URL that joins host:port, into out: ?lan after it for a server of
 * the local network, as the game's Join LAN screen adds it (the client's
 * LAN speed), then the password, when there is one. */
void dxl_server_join_url(const char *host, int port, int lan, const char *password, char *out, size_t size);

/* A deusex://host[:port][/] link as host:port, its port the game's 7790
 * when it names none, into out; 0 when it is no such link. */
int dxl_server_link(const char *link, char *out, size_t size);

/* The profile's favourites ([Profile] Favourites): "a.b.c.d:port" words,
 * the game's ports. Whether one is there; the list with one put in or
 * taken out, the caller's to free. */
int   dxl_favourite_has(const char *list, const char *address, int port);
char *dxl_favourite_toggle(const char *list, const char *address, int port);

#endif
