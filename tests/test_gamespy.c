#include "test.h"
#include "core/gamespy.h"

/* The original's own answers: IpDrv.dll's Validate, as the harness's
 * NetConsole logged it in the original under Proton (DXEngine's
 * tools/dxcap), each game's key; and two challenges 333networks sent
 * it, after which it listed the original its servers. */
static void test_validate(void) {
    static const struct { const char *challenge, *game, *answer; } vectors[] = {
        { "abcdef", "deusex", "pMqFmqAG" }, { "abcdef", "ut", "QnhL/w4k" },
        { "Q9w3Xk", "unreal", "zZGfacK1" }, { "zzzzzz", "oldver", "ouNSvpg1" },
        { "PABIDN", "deusex", "FbBtVJx6" }, { "XYDSRV", "deusex", "xpM6CYMR" },
    };
    for (size_t i = 0; i < sizeof vectors / sizeof *vectors; i++) {
        char out[9];
        dxl_gs_validate(vectors[i].challenge, vectors[i].game, out);
        CHECK_STR(out, vectors[i].answer);
    }
    /* Any game but the three keys as Deus Ex does. */
    char a[9], b[9];
    dxl_gs_validate("abcdef", "anything", a);
    dxl_gs_validate("abcdef", "deusex", b);
    CHECK_STR(a, b);
}

static void test_values_and_challenge(void) {
    char v[64], c[7];
    CHECK_INT(dxl_gs_value("\\hostname\\A Server\\numplayers\\3\\final\\", "NumPlayers", v, sizeof v), 1);
    CHECK_STR(v, "3");
    CHECK_INT(dxl_gs_value("\\hostname\\A Server\\final\\", "mapname", v, sizeof v), 0);
    CHECK_STR(v, "");
    CHECK_INT(dxl_gs_value("\\final\\", "final", v, sizeof v), 1);
    CHECK_INT(dxl_gs_challenge("\\basic\\\\secure\\ABCDEF", c), 1);
    CHECK_STR(c, "ABCDEF");
    CHECK_INT(dxl_gs_challenge("\\basic\\\\secure\\ABCDEFGH\\final\\", c), 1);
    CHECK_STR(c, "ABCDEF");
    CHECK_INT(dxl_gs_challenge("\\basic\\\\secure\\ABC", c), 0);   /* not all of it yet */
    CHECK_INT(dxl_gs_challenge("\\error\\no\\final\\", c), 0);
    const char *p = "\\a\\1\\b\\\\c\\3";
    char k[8], val[8];
    CHECK_INT(dxl_gs_next(&p, k, sizeof k, val, sizeof val), 1);
    CHECK_STR(k, "a");
    CHECK_STR(val, "1");
    CHECK_INT(dxl_gs_next(&p, k, sizeof k, val, sizeof val), 1);
    CHECK_STR(k, "b");
    CHECK_STR(val, "");
    CHECK_INT(dxl_gs_next(&p, k, sizeof k, val, sizeof val), 1);
    CHECK_STR(k, "c");
    CHECK_INT(dxl_gs_next(&p, k, sizeof k, val, sizeof val), 0);
}

typedef struct {
    char at[8][64];
    int  port[8];
    int  n;
} ips;

static void got_ip(void *ctx, const char *address, int port) {
    ips *l = ctx;
    if (l->n >= 8) return;
    snprintf(l->at[l->n], sizeof l->at[0], "%s", address);
    l->port[l->n++] = port;
}

/* The list once it is whole; the entries that are none left out. */
static void test_list(void) {
    ips l = { .n = 0 };
    CHECK_INT(dxl_gs_list("\\ip\\1.2.3.4:7791\\ip\\5.6.7.8:7771", got_ip, &l), -1);
    CHECK_INT(l.n, 0);
    CHECK_INT(dxl_gs_list("\\ip\\1.2.3.4:7791\\ip\\no-port\\ip\\5.6.7.8:0\\ip\\9.9.9.9:17578\\final\\", got_ip, &l), 2);
    CHECK_INT(l.n, 2);
    CHECK_STR(l.at[0], "1.2.3.4");
    CHECK_INT(l.port[0], 7791);
    CHECK_STR(l.at[1], "9.9.9.9");
    CHECK_INT(l.port[1], 17578);
    CHECK_INT(dxl_gs_list("\\final\\", got_ip, &l), 0);
}

/* Packet n of an answer as Deus Ex's UdpServerQuery sends it (IpServer.u):
 * its basic, its info, its rules, then a player's each; the last \final\
 * then its \queryid\. */
static void packet(char *out, size_t size, int id, int n, int players) {
    int last = n == 3 + players;
    const char *fin = last ? "\\final\\" : "";
    if (n == 1) snprintf(out, size, "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100%s\\queryid\\%d.%d", fin, id, n);
    else if (n == 2) snprintf(out, size, "\\hostname\\Busy\\hostport\\7790\\numplayers\\%d\\maxplayers\\16%s\\queryid\\%d.%d",
                              players, fin, id, n);
    else if (n == 3) snprintf(out, size, "\\TimeToWin\\15%s\\queryid\\%d.%d", fin, id, n);
    else snprintf(out, size, "\\player_%d\\P%d\\frags_%d\\%d%s\\queryid\\%d.%d", n - 4, n - 4, n - 4, n, fin, id, n);
}

/* An answer in the game's own packets, in whatever order they come: whole
 * once each is in, its pairs in the packets' order, their queryids and
 * \final\ out; a full server's, sixteen players in nineteen packets, too. */
static void test_status(void) {
    for (int players = 0; players <= 16; players += 8) {
        dxl_gs_status st;
        dxl_gs_status_init(&st);
        int total = 3 + players;
        char pkt[256];
        /* The last first, then the rest backwards. */
        for (int n = total; n >= 1; n--) {
            CHECK_INT(dxl_gs_status_whole(&st), 0);
            packet(pkt, sizeof pkt, 7, n, players);
            CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
            if (n == total) CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);   /* again */
        }
        CHECK_INT(dxl_gs_status_whole(&st), 1);
        char *text = dxl_gs_status_text(&st);
        CHECK(strncmp(text, "\\gamename\\deusex\\gamever\\1100\\minnetver\\1100\\hostname\\Busy", 56) == 0);
        CHECK(strstr(text, "queryid") == NULL && strstr(text, "final") == NULL);
        char last[32];
        snprintf(last, sizeof last, "\\player_%d\\P%d", players - 1, players - 1);
        if (players) CHECK(strstr(text, last) != NULL);
        free(text);
        dxl_gs_status_free(&st);
    }
    dxl_gs_status st;
    dxl_gs_status_init(&st);
    CHECK_INT(dxl_gs_status_add(&st, "\\hostname\\x", 11), -1);           /* no queryid */
    CHECK_INT(dxl_gs_status_add(&st, "\\queryid\\1.200", 14), -1);        /* past the packets kept */
    CHECK_INT(dxl_gs_status_add(&st, "\\queryid\\1.0", 12), -1);
    /* One packet, whole. */
    const char *one = "\\hostname\\One\\final\\\\queryid\\5.1";
    CHECK_INT(dxl_gs_status_add(&st, one, strlen(one)), 0);
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    dxl_gs_status_free(&st);
}

/* One answer's packets: another's not taken with them; asked over, the
 * first's still coming not taken for the next one's. */
static void test_answers_apart(void) {
    dxl_gs_status st;
    dxl_gs_status_init(&st);
    char pkt[256];
    packet(pkt, sizeof pkt, 41, 1, 2);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
    packet(pkt, sizeof pkt, 42, 2, 2);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);   /* another answer's */
    dxl_gs_status_restart(&st);
    CHECK_INT(st.count, 0);
    packet(pkt, sizeof pkt, 41, 2, 2);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);   /* the answer asked over's, late */
    for (int n = 1; n <= 5; n++) {
        packet(pkt, sizeof pkt, 43, n, 2);
        CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
    }
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    dxl_gs_status_free(&st);
}

/* Asked over with no packet of the first answer in, that answer's late
 * packets are taken until the later ask's come, the newer answer: the late
 * one is let go for it, and its packets after that too. */
static void test_newer_answer_wins(void) {
    dxl_gs_status st;
    dxl_gs_status_init(&st);
    dxl_gs_status_restart(&st);   /* asked over: nothing had come */
    char pkt[256];
    packet(pkt, sizeof pkt, 41, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);    /* the first ask's, late: taken */
    packet(pkt, sizeof pkt, 42, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 1);    /* the second ask's: newer */
    CHECK_INT(st.id, 42);
    CHECK_INT(st.count, 1);
    packet(pkt, sizeof pkt, 41, 2, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);   /* the earlier one's: let go */
    for (int n = 2; n <= 3; n++) {
        packet(pkt, sizeof pkt, 42, n, 0);
        CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
    }
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    dxl_gs_status_free(&st);
}

/* The server numbers its answers from 1 to 100 and begins again
 * (IpServer.u's UdpServerQuery): past the turn the later ask's answer, 1,
 * is still the newer beside the earlier one's late 100, and the late one's
 * packets after it are let go (bugs.md LCORE.17). */
static void test_newer_answer_past_the_turn(void) {
    dxl_gs_status st;
    dxl_gs_status_init(&st);
    dxl_gs_status_restart(&st);   /* asked over: nothing had come */
    char pkt[256];
    packet(pkt, sizeof pkt, 100, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);    /* the first ask's, late: taken */
    packet(pkt, sizeof pkt, 1, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 1);    /* the second ask's, the next after 100: newer */
    CHECK_INT(st.id, 1);
    packet(pkt, sizeof pkt, 100, 2, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);   /* the earlier one's: let go */
    for (int n = 2; n <= 3; n++) {
        packet(pkt, sizeof pkt, 1, n, 0);
        CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
    }
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    dxl_gs_status_free(&st);

    /* The later ask's first, the earlier one's late 99 after it: no newer
     * answer, another's. */
    dxl_gs_status_init(&st);
    dxl_gs_status_restart(&st);
    packet(pkt, sizeof pkt, 2, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), 0);
    packet(pkt, sizeof pkt, 99, 1, 0);
    CHECK_INT(dxl_gs_status_add(&st, pkt, strlen(pkt)), -1);
    CHECK_INT(st.id, 2);
    dxl_gs_status_free(&st);
}

TEST_MAIN_BEGIN
    RUN(test_validate);
    RUN(test_values_and_challenge);
    RUN(test_list);
    RUN(test_status);
    RUN(test_answers_apart);
    RUN(test_newer_answer_wins);
    RUN(test_newer_answer_past_the_turn);
TEST_MAIN_END
