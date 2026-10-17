#include "test.h"
#include "scratch.h"
#include "core/gamedb.h"
#include "core/sha1.h"

static void hex_of(const char *text, size_t n, char hex[41]) {
    dxl_sha1 s;
    uint8_t d[20];
    dxl_sha1_init(&s);
    dxl_sha1_update(&s, text, n);
    dxl_sha1_final(&s, d);
    dxl_sha1_hex(d, hex);
}

/* FIPS 180's examples, and a block-sized input fed in odd pieces. */
static void test_sha1_vectors(void) {
    char hex[41];
    hex_of("", 0, hex);
    CHECK_STR(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    hex_of("abc", 3, hex);
    CHECK_STR(hex, "a9993e364706816aba3e25717850c26c9cd0d89d");
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    hex_of(two, strlen(two), hex);
    CHECK_STR(hex, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    dxl_sha1 s;
    uint8_t d[20];
    dxl_sha1_init(&s);
    char a[1000];
    memset(a, 'a', sizeof a);
    for (int i = 0; i < 1000; i++) dxl_sha1_update(&s, a, sizeof a);   /* a million 'a's */
    dxl_sha1_final(&s, d);
    dxl_sha1_hex(d, hex);
    CHECK_STR(hex, "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
}

static void test_sha1_file(void) {
    const char *dir = scratch_dir("sha1");
    scratch_write(dir, "f", "abc");
    char *p = scratch_path(dir, "f");
    char hex[41];
    CHECK_INT(dxl_sha1_file(p, hex), 0);
    CHECK_STR(hex, "a9993e364706816aba3e25717850c26c9cd0d89d");
    char *missing = scratch_path(dir, "none");
    CHECK_INT(dxl_sha1_file(missing, hex), -1);
    free(missing);
    free(p);
    scratch_remove(dir);
}

/* The engine's builds, in any case; nothing else. */
static void test_builds(void) {
    const dxl_game_build *b = dxl_gamedb_find("2a933e26aa9cfb33b37f78afe21434caa031f14a");
    CHECK(b != NULL);
    if (b) { CHECK_STR(b->version, "1112fm"); CHECK_INT(b->current, 1); }
    b = dxl_gamedb_find("AF951DDD35B38E8D9CC8501B8A50A02A3AB6CAE7");
    CHECK(b != NULL);
    if (b) CHECK_STR(b->edition, "Steam");
    b = dxl_gamedb_find("9f923d667a396e8243028c14dc3f5e0a6db13d84");
    CHECK(b != NULL);
    if (b) { CHECK_STR(b->version, "1002f"); CHECK_INT(b->current, 0); }
    CHECK(dxl_gamedb_find("0000000000000000000000000000000000000000") == NULL);
    CHECK(dxl_gamedb_find(NULL) == NULL);
    CHECK_INT(dxl_gamedb_count(), 3);
}

TEST_MAIN_BEGIN
    RUN(test_sha1_vectors);
    RUN(test_sha1_file);
    RUN(test_builds);
TEST_MAIN_END
