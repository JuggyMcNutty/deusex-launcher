#include "test.h"
#include "core/servers.h"

/* A made-up server's answer, as Deus Ex's query answerer gives one. */
static const char status[] =
    "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\location\\0\\hostname\\A Test Server\\hostport\\7780"
    "\\maptitle\\Untitled\\mapname\\DXMP_Cathedral\\gametype\\DeathMatchGame\\numplayers\\2\\maxplayers\\16"
    "\\TimeToWin\\15\\mutators\\None\\password\\True\\AdminName\\Someone"
    "\\player_0\\JC\\frags_0\\12\\ping_0\\ 48\\team_0\\0\\mesh_0\\JCDenton"
    "\\player_1\\Paul\\frags_1\\7\\ping_1\\ 91\\team_1\\0";

static void test_status(void) {
    dxl_server s;
    dxl_server_init(&s, "1.2.3.4", 7781);
    CHECK_INT(s.port, 7780);
    CHECK_INT(s.state, DXL_SERVER_LISTED);
    CHECK_INT(dxl_server_joinable(&s), 0);
    dxl_server_take_status(&s, status);
    CHECK_INT(s.state, DXL_SERVER_ANSWERED);
    CHECK_STR(s.name, "A Test Server");
    CHECK_STR(s.map, "DXMP_Cathedral");
    CHECK_STR(s.map_title, "Untitled");
    CHECK_STR(s.game_type, "DeathMatchGame");
    CHECK_INT(s.port, 7780);
    CHECK_INT(s.players, 2);
    CHECK_INT(s.max_players, 16);
    CHECK_INT(s.password, 1);
    CHECK_INT(dxl_server_joinable(&s), 1);

    char name[64], key[64], value[64];
    int frags = -1, ping = -1;
    CHECK_INT(dxl_server_player(&s, 1, name, sizeof name, &frags, &ping), 1);
    CHECK_STR(name, "Paul");
    CHECK_INT(frags, 7);
    CHECK_INT(ping, 91);
    CHECK_INT(dxl_server_player(&s, 2, name, sizeof name, &frags, &ping), 0);
    /* The rules: neither the basic pairs nor a player's. */
    int n = 0;
    while (dxl_server_rule(&s, n, key, sizeof key, value, sizeof value)) n++;
    CHECK_INT(n, 3);
    CHECK_INT(dxl_server_rule(&s, 0, key, sizeof key, value, sizeof value), 1);
    CHECK_STR(key, "TimeToWin");
    CHECK_STR(value, "15");
    CHECK_INT(dxl_server_rule(&s, 2, key, sizeof key, value, sizeof value), 1);
    CHECK_STR(key, "AdminName");

    /* The packet's 8-bit text, as a DXEngine server sends it, shown as UTF-8. */
    dxl_server_take_status(&s, "\\hostname\\Caf\xe9\\maptitle\\\xc3\x89t\xc3\xa9\\player_0\\Jos\xe9");
    CHECK_STR(s.name, "Caf\xc3\xa9");
    CHECK_STR(s.map_title, "\xc3\x89t\xc3\xa9");
    char who[64];
    CHECK_INT(dxl_server_player(&s, 0, who, sizeof who, NULL, NULL), 1);
    CHECK_STR(who, "Jos\xc3\xa9");

    /* Another version: not the game's to join. */
    dxl_server_take_status(&s, "\\gamever\\1000\\minnetver\\1000\\hostname\\Old");
    CHECK_INT(dxl_server_joinable(&s), 0);
    CHECK_INT(s.password, 0);
    dxl_server_free(&s);
}

static void answered(dxl_server *s, const char *address, int ping, int players, const char *name) {
    dxl_server_init(s, address, 7791);
    char text[256];
    snprintf(text, sizeof text, "\\hostname\\%s\\numplayers\\%d\\maxplayers\\4\\gamever\\1100\\password\\False", name,
             players);
    dxl_server_take_status(s, text);
    s->ping_ms = ping;
}

/* The answered before the rest; then by ping, players or name. */
static void test_sort_and_filter(void) {
    dxl_server quiet, fast, full, named;
    dxl_server_init(&quiet, "9.9.9.9", 7791);
    answered(&fast, "1.1.1.1", 20, 1, "zulu");
    answered(&full, "2.2.2.2", 80, 4, "Mike");
    answered(&named, "3.3.3.3", 50, 0, "alpha");
    CHECK(dxl_server_compare(&fast, &quiet, DXL_SORT_PING) < 0);
    CHECK(dxl_server_compare(&quiet, &named, DXL_SORT_NAME) > 0);
    CHECK(dxl_server_compare(&fast, &named, DXL_SORT_PING) < 0);
    CHECK(dxl_server_compare(&full, &fast, DXL_SORT_PLAYERS) < 0);
    CHECK(dxl_server_compare(&named, &full, DXL_SORT_NAME) < 0);
    CHECK(dxl_server_compare(&full, &fast, DXL_SORT_NAME) < 0);   /* Mike before zulu, whatever their case */

    /* What a server says of itself, however wild, orders the same both ways. */
    full.players = 2147483647;
    named.players = -2;
    CHECK(dxl_server_compare(&full, &named, DXL_SORT_PLAYERS) < 0);
    CHECK(dxl_server_compare(&named, &full, DXL_SORT_PLAYERS) > 0);
    full.players = 4;
    named.players = 0;

    dxl_server_filter f = { 0 };
    CHECK_INT(dxl_server_shown(&quiet, &f, NULL), 1);
    f.not_empty = 1;
    CHECK_INT(dxl_server_shown(&named, &f, NULL), 0);
    CHECK_INT(dxl_server_shown(&quiet, &f, NULL), 0);
    CHECK_INT(dxl_server_shown(&fast, &f, NULL), 1);
    f.not_empty = 0;
    f.not_full = 1;
    CHECK_INT(dxl_server_shown(&full, &f, NULL), 0);
    f.not_full = 0;
    f.favourites = 1;
    CHECK_INT(dxl_server_shown(&fast, &f, "2.2.2.2:7790 1.1.1.1:7790"), 1);
    CHECK_INT(dxl_server_shown(&named, &f, "2.2.2.2:7790 1.1.1.1:7790"), 0);
    /* On this network: those a beacon there answered for, asked or not. */
    f.favourites = 0;
    f.lan = 1;
    fast.lan = quiet.lan = 1;
    CHECK_INT(dxl_server_shown(&fast, &f, NULL), 1);
    CHECK_INT(dxl_server_shown(&quiet, &f, NULL), 1);
    CHECK_INT(dxl_server_shown(&named, &f, NULL), 0);
    dxl_server_free(&quiet);
    dxl_server_free(&fast);
    dxl_server_free(&full);
    dxl_server_free(&named);
}

static void test_links(void) {
    char out[64];
    CHECK_INT(dxl_server_link("deusex://203.0.113.9:7780/", out, sizeof out), 1);
    CHECK_STR(out, "203.0.113.9:7780");
    CHECK_INT(dxl_server_link("DeusEx://dx.example.org", out, sizeof out), 1);
    CHECK_STR(out, "dx.example.org:7790");
    CHECK_INT(dxl_server_link("deusex://host:0", out, sizeof out), 0);
    CHECK_INT(dxl_server_link("deusex://", out, sizeof out), 0);
    CHECK_INT(dxl_server_link("unreal://host:7777", out, sizeof out), 0);
    CHECK_INT(dxl_server_link("203.0.113.9:7790", out, sizeof out), 0);
}

/* The URL that joins a server: ?lan for the local network's, as the game's
 * Join LAN screen adds it, before the password. */
static void test_join_url(void) {
    char out[128];
    dxl_server_join_url("203.0.113.9", 7790, 0, NULL, out, sizeof out);
    CHECK_STR(out, "203.0.113.9:7790");
    dxl_server_join_url("192.168.1.5", 7790, 1, "", out, sizeof out);
    CHECK_STR(out, "192.168.1.5:7790?lan");
    dxl_server_join_url("192.168.1.5", 7780, 1, "sesame", out, sizeof out);
    CHECK_STR(out, "192.168.1.5:7780?lan?password=sesame");
    dxl_server_join_url("dx.example.org", 7790, 0, "sesame", out, sizeof out);
    CHECK_STR(out, "dx.example.org:7790?password=sesame");
}

static void test_favourites(void) {
    CHECK_INT(dxl_favourite_has(NULL, "1.2.3.4", 7790), 0);
    CHECK_INT(dxl_favourite_has("1.2.3.4:77900 11.2.3.4:7790", "1.2.3.4", 7790), 0);   /* whole words alone */
    char *l = dxl_favourite_toggle("", "1.2.3.4", 7790);
    CHECK_STR(l, "1.2.3.4:7790");
    char *m = dxl_favourite_toggle(l, "5.6.7.8", 7780);
    CHECK_STR(m, "1.2.3.4:7790 5.6.7.8:7780");
    CHECK_INT(dxl_favourite_has(m, "5.6.7.8", 7780), 1);
    char *k = dxl_favourite_toggle(m, "1.2.3.4", 7790);
    CHECK_STR(k, "5.6.7.8:7780");
    free(k);
    free(m);
    free(l);
}

TEST_MAIN_BEGIN
    RUN(test_status);
    RUN(test_sort_and_filter);
    RUN(test_links);
    RUN(test_join_url);
    RUN(test_favourites);
TEST_MAIN_END
