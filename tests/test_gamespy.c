#include "test.h"
#include "core/gamespy.h"

/* The original's own answers: IpDrv.dll's Validate, as the harness's
 * NetConsole logged it in the original under Proton (VibeEngine's
 * vibe/tools/dxcap), each game's key; and two challenges 333networks sent
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

/* A status in three packets, in whatever order they come: whole once each
 * is in, its pairs in the packets' order, their queryids and \final\ out. */
static void test_status(void) {
    dxl_gs_status st;
    dxl_gs_status_init(&st);
    const char *p2 = "\\hostname\\Two\\hostport\\7790\\queryid\\82.2";
    const char *p3 = "\\player_0\\Cozmo\\frags_0\\15\\queryid\\82.3\\final\\";
    const char *p1 = "\\queryid\\82.1\\gamename\\deusex\\gamever\\1100";
    CHECK_INT(dxl_gs_status_add(&st, p2, strlen(p2)), 0);
    CHECK_INT(dxl_gs_status_whole(&st), 0);
    CHECK_INT(dxl_gs_status_add(&st, p3, strlen(p3)), 0);
    CHECK_INT(dxl_gs_status_whole(&st), 0);
    CHECK_INT(dxl_gs_status_add(&st, p2, strlen(p2)), -1);   /* again */
    CHECK_INT(dxl_gs_status_add(&st, p1, strlen(p1)), 0);
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    char *text = dxl_gs_status_text(&st);
    CHECK_STR(text, "\\gamename\\deusex\\gamever\\1100\\hostname\\Two\\hostport\\7790\\player_0\\Cozmo\\frags_0\\15");
    free(text);
    CHECK_INT(dxl_gs_status_add(&st, "\\hostname\\x", 11), -1);           /* no queryid */
    CHECK_INT(dxl_gs_status_add(&st, "\\queryid\\1.99", 13), -1);         /* past the packets kept */
    dxl_gs_status_free(&st);
    /* One packet, whole. */
    dxl_gs_status_init(&st);
    const char *one = "\\hostname\\One\\queryid\\5.1\\final\\";
    CHECK_INT(dxl_gs_status_add(&st, one, strlen(one)), 0);
    CHECK_INT(dxl_gs_status_whole(&st), 1);
    dxl_gs_status_free(&st);
}

TEST_MAIN_BEGIN
    RUN(test_validate);
    RUN(test_values_and_challenge);
    RUN(test_list);
    RUN(test_status);
TEST_MAIN_END
