/* DXController (core/dxcontroller.h) and the patches it is made with
 * (core/patch.h), on made-up files: a source of bytes of its own stands in
 * for the game's DeusEx.u, the result for the mod's; nothing of the game's
 * or the mod's is here. The real patch on the real package is
 * test_gamefiles'. */
#include "test.h"
#include "scratch.h"
#include "core/config.h"
#include "core/dxcontroller.h"
#include "core/engine_settings.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/patch.h"
#include "core/paths.h"
#include "core/profile.h"
#include "core/sha1.h"

#include <stdint.h>
#include <sys/stat.h>

/* ---- a patch made here ---------------------------------------------------- */

typedef struct {
    unsigned char *d;
    size_t n, cap;
} buf;

static void put(buf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) {
        b->cap = (b->n + n) * 2;
        b->d = realloc(b->d, b->cap);
    }
    memcpy(b->d + b->n, p, n);
    b->n += n;
}

static void put_le(buf *b, uint64_t v, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)(v >> (8 * i));
        put(b, &c, 1);
    }
}

static void put_number(buf *b, uint64_t v) {
    do {
        unsigned char c = v & 0x7f;
        v >>= 7;
        if (v) c |= 0x80;
        put(b, &c, 1);
    } while (v);
}

static void put_sha1(buf *b, const unsigned char *p, size_t n) {
    dxl_sha1 s;
    uint8_t d[20];
    dxl_sha1_init(&s);
    dxl_sha1_update(&s, p, n);
    dxl_sha1_final(&s, d);
    put(b, d, 20);
}

static void header(buf *b, const unsigned char *src, size_t sn, const unsigned char *res, size_t rn) {
    put(b, "DXLPATCH", 8);
    put_le(b, 1, 4);
    put_le(b, sn, 8);
    put_sha1(b, src, sn);
    put_le(b, rn, 8);
    put_sha1(b, res, rn);
}

static void op_add(buf *b, const void *p, size_t n) {
    put_number(b, (uint64_t)n << 2);
    put(b, p, n);
}
static void op_copy(buf *b, size_t n) { put_number(b, (uint64_t)n << 2 | 1); }
static void op_copy_at(buf *b, size_t n, int64_t d) {
    put_number(b, (uint64_t)n << 2 | 2);
    put_number(b, d >= 0 ? (uint64_t)d << 1 : ((uint64_t)-d << 1) - 1);
}

#define SRC_N 4000
#define RES_N 2600
static unsigned char src[SRC_N], res[RES_N];

/* The result: the source's first 1000 bytes, three new ones over its next
 * three, its next 997, then 500 from 3000 on and 100 from 100 on. */
static void make_files(buf *patch) {
    for (int i = 0; i < SRC_N; i++) src[i] = (unsigned char)(i * 7 + 3 + (i >> 8));
    memcpy(res, src, 1000);
    memcpy(res + 1000, "XYZ", 3);
    memcpy(res + 1003, src + 1003, 997);
    memcpy(res + 2000, src + 3000, 500);
    memcpy(res + 2500, src + 100, 100);
    memset(patch, 0, sizeof *patch);
    header(patch, src, SRC_N, res, RES_N);
    op_copy(patch, 1000);
    op_add(patch, "XYZ", 3);
    op_copy(patch, 997);              /* the diagonal: 1000 copied and 3 added */
    op_copy_at(patch, 500, 1000);     /* from 2000 on, 1000 further */
    op_copy_at(patch, 100, -3400);    /* from 3500 back to 100 */
}

static void write_bytes(const char *path, const void *p, size_t n) {
    FILE *f = fopen(path, "wb");
    if (f) {
        fwrite(p, 1, n, f);
        fclose(f);
    }
}

/* ---- the patch ------------------------------------------------------------ */

static void test_patch_applies(void) {
    buf p;
    make_files(&p);
    unsigned char *out = NULL;
    size_t n = 0;
    dxl_err e;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, p.d, p.n, &out, &n, &e), 0);
    CHECK_INT(n, RES_N);
    CHECK(out && !memcmp(out, res, RES_N));
    free(out);
    dxl_patch_info info;
    CHECK_INT(dxl_patch_header(p.d, p.n, &info, &e), 0);
    CHECK_INT(info.source_size, SRC_N);
    CHECK_INT(info.result_size, RES_N);
    free(p.d);
}

static void test_patch_wrong_source(void) {
    buf p;
    make_files(&p);
    unsigned char *out = NULL;
    size_t n = 0;
    dxl_err e;
    src[2000] ^= 1;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, p.d, p.n, &out, &n, &e), -1);
    CHECK(out == NULL);
    CHECK(strstr(dxl_err_msg(&e), "not the one the patch is for") != NULL);
    src[2000] ^= 1;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N - 1, p.d, p.n, &out, &n, &e), -1);
    free(p.d);
}

/* A result not the one the header names is never given: a patch whose
 * operations make another. */
static void test_patch_result_checked(void) {
    buf p;
    make_files(&p);
    p.d[DXL_PATCH_HEADER + 2 + 1] ^= 0x20;   /* "XYZ" -> "XyZ" */
    unsigned char *out = NULL;
    size_t n = 0;
    dxl_err e;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, p.d, p.n, &out, &n, &e), -1);
    CHECK(out == NULL);
    CHECK(strstr(dxl_err_msg(&e), "not the one it names") != NULL);
    free(p.d);
}

/* Cut anywhere, a patch gives nothing. */
static void test_patch_truncated(void) {
    buf p;
    make_files(&p);
    int refused = 0, tried = 0;
    for (size_t cut = 0; cut < p.n; cut++) {
        unsigned char *out = NULL;
        size_t n = 0;
        tried++;
        if (dxl_patch_apply_mem(src, SRC_N, p.d, cut, &out, &n, NULL) != 0 && !out) refused++;
        free(out);
    }
    CHECK_INT(refused, tried);
    free(p.d);
}

/* Operations past either file, of no kind, of no length, or a number
 * longer than nine bytes. */
static void test_patch_out_of_bounds(void) {
    buf b;
    make_files(&b);
    free(b.d);
    unsigned char *out = NULL;
    size_t n = 0;
    dxl_err e;

    /* A copy past the source's end. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, RES_N);
    op_copy_at(&b, 100, SRC_N - 50);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "reaches past") != NULL);
    free(b.d);

    /* A copy before its start. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, RES_N);
    op_copy(&b, 10);
    op_copy_at(&b, 10, -11);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    free(b.d);

    /* An add past the result's end. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, 4);
    op_add(&b, "ABCDEFGH", 8);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    free(b.d);

    /* A copy past the result's end. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, RES_N);
    op_copy(&b, RES_N + 1);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    free(b.d);

    /* An add longer than what is left of the patch. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, RES_N);
    put_number(&b, 100 << 2);
    put(&b, "short", 5);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    free(b.d);

    /* No kind, no length, a number too long. */
    unsigned char kind3[] = { 7 }, empty[] = { 1 }, longer[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                                                  0xff, 0x01 };
    const unsigned char *bad[] = { kind3, empty, longer };
    size_t lens[] = { sizeof kind3, sizeof empty, sizeof longer };
    for (int i = 0; i < 3; i++) {
        memset(&b, 0, sizeof b);
        header(&b, src, SRC_N, res, RES_N);
        put(&b, bad[i], lens[i]);
        CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
        CHECK(out == NULL);
        free(b.d);
    }

    /* Not a patch, another version. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, res, RES_N);
    b.d[8] = 2;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    b.d[0] = 'X';
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    free(b.d);
}

/* From files: the result written whole; a failure leaves what was there,
 * or nothing, and no temporary file beside it. */
static void test_patch_files(void) {
    char *dir = dxl_xstrdup(scratch_dir("dxc-patch"));
    buf p;
    make_files(&p);
    char *sp = scratch_path(dir, "source.u"), *pp = scratch_path(dir, "p.dxlpatch"), *rp = scratch_path(dir, "out/r.u");
    write_bytes(sp, src, SRC_N);
    write_bytes(pp, p.d, p.n);
    dxl_err e;

    /* No folder for it: nothing made. */
    char nowhere[600];
    snprintf(nowhere, sizeof nowhere, "%s/none/r.u", dir);
    CHECK_INT(dxl_patch_apply(sp, pp, nowhere, &e), -1);
    CHECK(!dxl_path_exists(nowhere));

    dxl_path_mkdirs(scratch_path(dir, "out"));
    CHECK_INT(dxl_patch_apply(sp, pp, rp, &e), 0);
    size_t n = 0;
    char *got = slurp(rp, &n);
    CHECK_INT(n, RES_N);
    CHECK(got && !memcmp(got, res, RES_N));
    free(got);

    /* A wrong source: the result there kept as it is. */
    write_bytes(rp, "old", 3);
    src[5] ^= 1;
    write_bytes(sp, src, SRC_N);
    src[5] ^= 1;
    CHECK_INT(dxl_patch_apply(sp, pp, rp, &e), -1);
    got = slurp(rp, &n);
    CHECK(got && n == 3 && !memcmp(got, "old", 3));
    free(got);

    /* A result that cannot be written (read-only): kept, refused. */
    write_bytes(sp, src, SRC_N);
    chmod(rp, 0444);
    CHECK_INT(dxl_patch_apply(sp, pp, rp, &e), -1);
    got = slurp(rp, &n);
    CHECK(got && n == 3);
    free(got);
    chmod(rp, 0644);

    /* No file left behind but the result. */
    char *out = scratch_path(dir, "out");
    DIR *d = opendir(out);
    int others = 0;
    struct dirent *de;
    while (d && (de = readdir(d)))
        if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..") && strcmp(de->d_name, "r.u")) others++;
    if (d) closedir(d);
    CHECK_INT(others, 0);
    free(out);

    /* A cut patch from its file. */
    write_bytes(pp, p.d, p.n - 1);
    CHECK_INT(dxl_patch_apply(sp, pp, rp, &e), -1);
    free(sp);
    free(pp);
    free(rp);
    free(p.d);
    scratch_remove(dir);
    free(dir);
}

/* The diagonal past the source's end, a distance back bringing it in: what
 * tools/dxpatch.py makes where bytes are added after a copy that reached
 * the source's end, and applies. */
static void test_patch_diagonal_past_end(void) {
    buf b;
    make_files(&b);
    free(b.d);
    static unsigned char r[SRC_N + 4 + 100];
    memcpy(r, src, SRC_N);
    memcpy(r + SRC_N, "ABCD", 4);
    memcpy(r + SRC_N + 4, src, 100);
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, r, sizeof r);
    op_copy(&b, SRC_N);
    op_add(&b, "ABCD", 4);
    op_copy_at(&b, 100, -(int64_t)(SRC_N + 4));
    unsigned char *out = NULL;
    size_t n = 0;
    dxl_err e;
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), 0);
    CHECK(out && n == sizeof r && !memcmp(out, r, sizeof r));
    free(out);
    free(b.d);

    /* Still past it, the distance not enough: refused. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, r, sizeof r);
    op_copy(&b, SRC_N);
    op_add(&b, "ABCD", 4);
    op_copy_at(&b, 100, -3);
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    CHECK(out == NULL);
    free(b.d);

    /* A header naming a result larger than any taken: refused unallocated. */
    memset(&b, 0, sizeof b);
    header(&b, src, SRC_N, r, sizeof r);
    for (int i = 0; i < 8; i++) b.d[40 + i] = (unsigned char)(((uint64_t)DXL_PATCH_MAX + 1) >> (8 * i));
    CHECK_INT(dxl_patch_apply_mem(src, SRC_N, b.d, b.n, &out, &n, &e), -1);
    CHECK(strstr(dxl_err_msg(&e), "larger") != NULL);
    free(b.d);
}

/* ---- the wiring -------------------------------------------------------- */

typedef struct {
    char *root, *game, *app, *home;
    dxl_profile profile;
    dxl_config *config;
    dxl_dxc_place w;
} world;

static const char sys_ini[] =
    "[Engine.Engine]\nGameRenderDevice=D3DDrv.D3DRenderDevice\nConsole=Engine.Console\n"
    "Root=DeusEx.DeusExRootWindow\n\n[Core.System]\nSavePath=../Save\nPaths=../System/*.u\nPaths=../Maps/*.dx\n\n";
static const char user_ini[] =
    "[Engine.Input]\nSpace=Jump\nJoy1=Fire\nJoy2=Jump\nJoy3=\nJoyX=Axis astrafe speed=2\n"
    "JoyU=Axis aturn speed=5.9\n\n";

static void open_world(world *x, const char *tag) {
    memset(x, 0, sizeof *x);
    const char *dir = scratch_dir(tag);
    x->root = dxl_xstrdup(dir);
    x->game = scratch_path(dir, "game/System");
    x->app = scratch_path(dir, "app/data");
    x->home = scratch_path(dir, "home");
    dxl_path_mkdirs(x->game);
    dxl_path_mkdirs(x->home);
    char *p = scratch_path(dir, "game/System/DeusEx.u");
    write_bytes(p, src, SRC_N);
    free(p);
    buf patch;
    make_files(&patch);
    p = scratch_path(dir, "app/data/dxcontroller/" DXL_DXC_PATCH);
    write_bytes(p, patch.d, patch.n);
    free(p);
    free(patch.d);
    scratch_write(dir, "app/data/dxcontroller/" DXL_DXC_PACKAGE, "the mod's scripts");
    char *def = scratch_path(dir, "app/data/engine-settings.json.default");
    dxl_path_copy(DXL_ES_DEFAULTS, def, NULL);
    free(def);
    scratch_write(dir, "profile/DeusEx.ini", sys_ini);
    scratch_write(dir, "profile/User.ini", user_ini);
    x->profile.id = dxl_xstrdup("test");
    x->profile.dir = scratch_path(dir, "profile");
    x->profile.ini_path = scratch_path(dir, "profile/profile.ini");
    x->profile.ini = dxl_ini_new();
    x->profile.system_ini = scratch_path(dir, "profile/DeusEx.ini");
    x->profile.user_ini = scratch_path(dir, "profile/User.ini");
    x->profile.engine_dir = scratch_path(dir, "profile/engine");
    x->config = dxl_config_open_files(x->game, "DeusEx", x->profile.system_ini, x->profile.user_ini);
    x->w = (dxl_dxc_place){ x->config, &x->profile, x->app, x->home, x->game, 1 };
}

static void close_world(world *x) {
    dxl_config_free(x->config);
    dxl_profile_free(&x->profile);
    scratch_remove(x->root);
    free(x->root);
    free(x->game);
    free(x->app);
    free(x->home);
}

/* The files as a start leaves them, the inis written as commit writes
 * them, then the pad's mode. */
static int start(world *x, int multiplayer, char *why, size_t size) {
    int rc = dxl_dxc_start(&x->w, multiplayer, why, size);
    int saved = dxl_config_save(x->config, NULL) == 0;
    if (rc == 1) dxl_dxc_pad(&x->w, saved, NULL);
    dxl_profile_save(&x->profile, NULL);
    return rc;
}

static char *file(world *x, const char *rel) {
    char *p = scratch_path(x->root, rel);
    char *text = slurp(p, NULL);
    free(p);
    return text ? text : dxl_xstrdup("");
}

static int pad_mode(world *x) {
    char *json = scratch_path(x->root, "profile/engine/Settings.json");
    dxl_engine_settings *es = dxl_es_open(json, NULL);
    int on = dxl_es_bool(es, DXL_ES_PAD_DXCONTROLLER);
    dxl_es_free(es);
    free(json);
    return on;
}

static void test_on_and_off(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-onoff");
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    CHECK_STR(why, "");

    /* The patched file, made in a folder named by both SHA-1s. */
    char made[1024];
    snprintf(made, sizeof made, "%s/%s", x.home, DXL_DXC_FOLDER);
    char *leaf = NULL;
    DIR *d = opendir(made);
    struct dirent *de;
    while (d && (de = readdir(d)))
        if (de->d_name[0] != '.') leaf = dxl_xstrdup(de->d_name);
    if (d) closedir(d);
    CHECK(leaf && strlen(leaf) == 25 && leaf[12] == '-');
    char *made_u = dxl_xstrdup("");
    if (leaf) {
        free(made_u);
        size_t n = strlen(made) + strlen(leaf) + 16;
        made_u = malloc(n);
        snprintf(made_u, n, "%s/%s/DeusEx.u", made, leaf);
    }
    size_t n = 0;
    char *bytes = slurp(made_u, &n);
    CHECK(bytes && n == RES_N && !memcmp(bytes, res, RES_N));
    free(bytes);

    /* Every key set: our two folders first, the console and root, the
     * bindings, the engine's pad mode. */
    dxl_ini *sys = dxl_ini_load(x.profile.system_ini, NULL), *usr = dxl_ini_load(x.profile.user_ini, NULL);
    const char *paths[8];
    size_t np = dxl_ini_get_all(sys, "Core.System", "Paths", paths, 8);
    CHECK_INT(np, 4);
    if (np == 4) {
        CHECK(strstr(paths[0], leaf ? leaf : "?") && strstr(paths[0], "/*.u"));
        char want[1024];
        snprintf(want, sizeof want, "%s/dxcontroller/*.u", x.app);
        CHECK_STR(paths[1], want);
        CHECK_STR(paths[2], "../System/*.u");
        CHECK_STR(paths[3], "../Maps/*.dx");
    }
    CHECK_STR(dxl_ini_get(sys, "Engine.Engine", "Console"), DXL_DXC_CONSOLE);
    CHECK_STR(dxl_ini_get(sys, "Engine.Engine", "Root"), DXL_DXC_ROOT);
    for (size_t i = 0; i < dxl_dxc_binding_count(); i++)
        CHECK_STR(dxl_ini_get(usr, "Engine.Input", dxl_dxc_binding_at(i)->key), dxl_dxc_binding_at(i)->binding);
    CHECK_STR(dxl_ini_get(usr, "Engine.Input", "Space"), "Jump");
    CHECK_INT(pad_mode(&x), 1);
    dxl_ini_free(sys);
    dxl_ini_free(usr);

    /* A second start changes nothing. */
    char *a1 = file(&x, "profile/DeusEx.ini"), *a2 = file(&x, "profile/User.ini");
    char *a3 = file(&x, "profile/engine/Settings.json"), *a4 = file(&x, "profile/profile.ini");
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    char *b1 = file(&x, "profile/DeusEx.ini"), *b2 = file(&x, "profile/User.ini");
    char *b3 = file(&x, "profile/engine/Settings.json"), *b4 = file(&x, "profile/profile.ini");
    CHECK_STR(b1, a1);
    CHECK_STR(b2, a2);
    CHECK_STR(b3, a3);
    CHECK_STR(b4, a4);
    free(a1); free(a2); free(a3); free(a4); free(b1); free(b2); free(b3); free(b4);

    /* After the game: every key back, the files as they were but the pad
     * mode's, now off; a second rest changes nothing. */
    dxl_err e;
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    char *s1 = file(&x, "profile/DeusEx.ini"), *s2 = file(&x, "profile/User.ini");
    CHECK_STR(s1, sys_ini);
    CHECK_STR(s2, user_ini);
    CHECK_INT(pad_mode(&x), 0);
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 0);
    char *t1 = file(&x, "profile/DeusEx.ini"), *t2 = file(&x, "profile/User.ini");
    CHECK_STR(t1, sys_ini);
    CHECK_STR(t2, user_ini);
    free(s1); free(s2); free(t1); free(t2);

    /* The patched file gone: made again at the next start. */
    remove(made_u);
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    CHECK(dxl_path_size(made_u) == RES_N);

    /* The option off: a start undoes what the last one did. */
    dxl_profile_set(&x.profile, DXL_DXC_OPTION, "False");
    CHECK_INT(start(&x, 0, why, sizeof why), 0);
    char *u1 = file(&x, "profile/DeusEx.ini"), *u2 = file(&x, "profile/User.ini");
    CHECK_STR(u1, sys_ini);
    CHECK_STR(u2, user_ini);
    CHECK_INT(pad_mode(&x), 0);
    free(u1); free(u2);
    free(made_u);
    free(leaf);
    close_world(&x);
}

/* A multiplayer start runs without it, the last start's undone. */
static void test_multiplayer(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-mp");
    CHECK_INT(start(&x, 1, why, sizeof why), DXL_DXC_MULTIPLAYER);
    char *a = file(&x, "profile/DeusEx.ini");
    CHECK_STR(a, sys_ini);
    free(a);
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    CHECK_INT(start(&x, 1, why, sizeof why), DXL_DXC_MULTIPLAYER);
    char *b = file(&x, "profile/DeusEx.ini"), *c = file(&x, "profile/User.ini");
    CHECK_STR(b, sys_ini);
    CHECK_STR(c, user_ini);
    CHECK_INT(pad_mode(&x), 0);
    free(b);
    free(c);
    close_world(&x);

    CHECK_INT(dxl_dxc_multiplayer("", ""), 0);
    CHECK_INT(dxl_dxc_multiplayer("?loadgame=3", ""), 0);
    CHECK_INT(dxl_dxc_multiplayer("1.2.3.4:7790", ""), 1);
    CHECK_INT(dxl_dxc_multiplayer("", "-server"), 1);
    CHECK_INT(dxl_dxc_multiplayer("", "DXMP_Cathedral?listen"), 1);
    CHECK_INT(dxl_dxc_multiplayer("", "127.0.0.1"), 1);
    CHECK_INT(dxl_dxc_multiplayer("", "example.org:7790?password=x"), 1);
    CHECK_INT(dxl_dxc_multiplayer("", "01_NYC_UNATCOIsland.dx -log"), 0);
    CHECK_INT(dxl_dxc_multiplayer("", "-safe INI=a.b"), 0);
}

/* Cut short after profile.ini kept the player's, before the inis were
 * written: the next start's rest finds nothing of ours in them and keeps
 * them; and a profile.ini that lost what it kept still mends the console. */
static void test_cut_short(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-cut");
    CHECK_INT(dxl_dxc_start(&x.w, 0, why, sizeof why), 1);   /* profile.ini written, the inis not */
    dxl_config_reload(x.config);
    dxl_err e;
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    char *a = file(&x, "profile/DeusEx.ini");
    CHECK_STR(a, sys_ini);
    free(a);
    CHECK(!dxl_ini_get(x.profile.ini, "DXController.Undo", "Made"));

    /* The console of ours with nothing kept: the game's own put back, and
     * our folders out of the paths. */
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    dxl_ini_empty_section(x.profile.ini, "DXController.Undo");
    dxl_ini_empty_section(x.profile.ini, "DXController.Input");
    dxl_profile_save(&x.profile, NULL);
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    dxl_ini *sys = dxl_ini_load(x.profile.system_ini, NULL);
    CHECK_STR(dxl_ini_get(sys, "Engine.Engine", "Console"), "Engine.Console");
    CHECK_STR(dxl_ini_get(sys, "Engine.Engine", "Root"), "DeusEx.DeusExRootWindow");
    CHECK_INT(dxl_ini_get_all(sys, "Core.System", "Paths", NULL, 0), 2);
    dxl_ini_free(sys);
    close_world(&x);
}

/* The app moved: the old folder's path taken out, the new one's put in. */
static void test_app_moved(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-moved");
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    char *moved = scratch_path(x.root, "moved/data");
    dxl_path_copy_dir(x.app, moved, NULL, NULL, NULL);
    x.w.app_data = moved;
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    dxl_ini *sys = dxl_ini_load(x.profile.system_ini, NULL);
    const char *paths[8];
    size_t np = dxl_ini_get_all(sys, "Core.System", "Paths", paths, 8);
    CHECK_INT(np, 4);
    char want[1024];
    snprintf(want, sizeof want, "%s/dxcontroller/*.u", moved);
    if (np == 4) CHECK_STR(paths[1], want);
    dxl_ini_free(sys);
    dxl_err e;
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    char *a = file(&x, "profile/DeusEx.ini");
    CHECK_STR(a, sys_ini);
    free(a);
    free(moved);
    close_world(&x);
}

/* A path of the player's own beneath a folder named DXController (a mod's
 * System) is no path of ours: kept by the start and the undo. */
static void test_players_path_kept(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-own");
    dxl_ini *sys = dxl_config_ini(x.config);
    dxl_ini_append(sys, "Core.System", "Paths", "../DXController/System/*.u");
    dxl_ini_append(sys, "Core.System", "Paths", "C:\\Games\\DXController\\Textures\\*.u");
    dxl_config_save(x.config, NULL);
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    CHECK_INT(dxl_ini_get_all(dxl_config_ini(x.config), "Core.System", "Paths", NULL, 0), 6);
    dxl_err e;
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    const char *paths[8];
    size_t np = dxl_ini_get_all(dxl_config_ini(x.config), "Core.System", "Paths", paths, 8);
    CHECK_INT(np, 4);
    if (np == 4) {
        CHECK_STR(paths[2], "../DXController/System/*.u");
        CHECK_STR(paths[3], "C:\\Games\\DXController\\Textures\\*.u");
    }
    close_world(&x);
}

/* The engine's pad in the mod's mode only once the inis holding the mod's
 * console are saved: not by the start itself, and not where the save
 * failed (a full disk), the stock console then playing. */
static void test_pad_after_inis(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-pad");
    CHECK_INT(dxl_dxc_start(&x.w, 0, why, sizeof why), 1);
    CHECK_INT(pad_mode(&x), 0);
    dxl_err e;
    CHECK_INT(dxl_dxc_pad(&x.w, 0, &e), 0);
    CHECK_INT(pad_mode(&x), 0);
    CHECK_INT(dxl_config_save(x.config, NULL), 0);
    CHECK_INT(dxl_dxc_pad(&x.w, 1, &e), 0);
    CHECK_INT(pad_mode(&x), 1);
    /* A later start's save that fails: the mode a kill left put back. */
    CHECK_INT(dxl_dxc_pad(&x.w, 0, &e), 0);
    CHECK_INT(pad_mode(&x), 0);
    close_world(&x);
}

/* A pad key the game bound while the mod was on, set back to the player's
 * by the undo: launcher.log names the key, the binding dropped and the one
 * put back. The mod's own bindings and keys the player had none of
 * unbound by the game are not said. */
static void test_rest_says_dropped(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-dropped");
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    dxl_ini *usr = dxl_config_user_ini(x.config);
    dxl_ini_set(usr, "Engine.Input", "Joy1", "Duck");
    dxl_ini_set(usr, "Engine.Input", "Joy4", "Fire");
    dxl_ini_set(usr, "Engine.Input", "Joy5", "");
    dxl_config_save(x.config, NULL);
    char *log = scratch_path(x.root, "launcher.log");
    dxl_log_set_echo(0);
    dxl_log_open_new(log);
    dxl_err e;
    CHECK_INT(dxl_dxc_rest(&x.w, &e), 1);
    dxl_log_close();
    dxl_log_set_echo(1);
    char *said = slurp(log, NULL);
    CHECK(said && strstr(said, "Joy1 set back to the player's \"Fire\", the \"Duck\" a game with it on wrote dropped"));
    CHECK(said && strstr(said, "Joy4 taken away, the player's none, the \"Fire\" a game with it on wrote dropped"));
    CHECK(said && !strstr(said, "Joy2") && !strstr(said, "Joy5"));
    free(said);
    free(log);
    dxl_ini *back = dxl_ini_load(x.profile.user_ini, NULL);
    CHECK_STR(dxl_ini_get(back, "Engine.Input", "Joy1"), "Fire");
    CHECK(dxl_ini_get(back, "Engine.Input", "Joy4") == NULL);
    dxl_ini_free(back);
    close_world(&x);
}

/* Why it cannot be on, each reason; and a patched file that cannot be made
 * switches it off for the start, said and kept for the page. */
static void test_cannot_be_on(void) {
    world x;
    char why[600];
    open_world(&x, "dxc-able");
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 1);

    dxl_profile_set(&x.profile, "Mod", "/somewhere/a mod");
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 0);
    CHECK(strstr(why, "mod's profile") != NULL);
    CHECK_INT(start(&x, 0, why, sizeof why), 0);
    char *a = file(&x, "profile/DeusEx.ini");
    CHECK_STR(a, sys_ini);
    free(a);
    dxl_ini_remove(x.profile.ini, "Profile", "Mod");

    x.w.engine_has = 0;
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 0);
    CHECK(strstr(why, "Settings, Launcher, The engine") != NULL);
    x.w.engine_has = 1;

    char *pkg = scratch_path(x.root, "app/data/dxcontroller/" DXL_DXC_PACKAGE);
    remove(pkg);
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 0);
    CHECK(strstr(why, "install the launcher again") != NULL);
    scratch_write(x.root, "app/data/dxcontroller/" DXL_DXC_PACKAGE, "the mod's scripts");
    free(pkg);

    /* A DeusEx.u not the stock one: changed (its SHA-1 read again as its
     * time changed), then of another size. */
    char *stock = scratch_path(x.root, "game/System/DeusEx.u");
    src[10] ^= 1;
    write_bytes(stock, src, SRC_N);
    src[10] ^= 1;
    dxl_fs_set_mtime(stock, dxl_path_mtime(stock) - 100);
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 0);
    CHECK(strstr(why, "DeusEx.u is not the one") != NULL);
    write_bytes(stock, src, SRC_N - 1);
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 0);
    write_bytes(stock, src, SRC_N);
    CHECK_INT(dxl_dxc_able(&x.w, why, sizeof why), 1);
    free(stock);

    /* No room for the patched file (its folder a file): off for the
     * start, said, kept for the page; gone at the next good start. */
    scratch_write(x.root, "nohome", "a file");
    const char *home = x.w.data_home;
    char *nohome = scratch_path(x.root, "nohome");
    x.w.data_home = nohome;
    CHECK_INT(start(&x, 0, why, sizeof why), -1);
    CHECK(strstr(why, "could not be made") != NULL);
    CHECK(dxl_profile_get(&x.profile, DXL_DXC_PROBLEM) != NULL);
    a = file(&x, "profile/DeusEx.ini");
    CHECK_STR(a, sys_ini);
    free(a);
    x.w.data_home = home;
    CHECK_INT(start(&x, 0, why, sizeof why), 1);
    CHECK(dxl_profile_get(&x.profile, DXL_DXC_PROBLEM) == NULL);
    free(nohome);

    /* The option: on unless the profile says it is not. */
    CHECK_INT(dxl_dxc_option(&x.profile), 1);
    dxl_profile_set(&x.profile, DXL_DXC_OPTION, "False");
    CHECK_INT(dxl_dxc_option(&x.profile), 0);
    close_world(&x);
}

TEST_MAIN_BEGIN
    RUN(test_patch_applies);
    RUN(test_patch_wrong_source);
    RUN(test_patch_result_checked);
    RUN(test_patch_truncated);
    RUN(test_patch_out_of_bounds);
    RUN(test_patch_files);
    RUN(test_patch_diagonal_past_end);
    RUN(test_on_and_off);
    RUN(test_multiplayer);
    RUN(test_cut_short);
    RUN(test_app_moved);
    RUN(test_players_path_kept);
    RUN(test_pad_after_inis);
    RUN(test_rest_says_dropped);
    RUN(test_cannot_be_on);
TEST_MAIN_END
