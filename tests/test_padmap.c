/* SDL's mapping strings (core/padmap.h): made and read back, compared, a
 * GUID's CRC taken off and named, gamecontrollerdb.txt kept by GUID, Steam's
 * mappings of ours left out, and the engine's SDL_GAMECONTROLLERCONFIG. */
#include "test.h"
#include "scratch.h"
#include "core/padmap.h"

#define GUID "03008fe55e0400008e02000014010000"

static void test_format_and_parse(void) {
    dxl_padmap m;
    memset(&m, 0, sizeof m);
    snprintf(m.guid, sizeof m.guid, GUID);
    snprintf(m.name, sizeof m.name, "A pad, of ours");
    m.src[DXL_PADCTL_A] = (dxl_padsrc){ DXL_PADSRC_BUTTON, 0, 0, 0, 0 };
    m.src[DXL_PADCTL_DPUP] = (dxl_padsrc){ DXL_PADSRC_HAT, 0, 1, 0, 0 };
    m.src[DXL_PADCTL_LEFTY] = (dxl_padsrc){ DXL_PADSRC_AXIS, 1, 0, 0, 1 };
    m.src[DXL_PADCTL_LEFTTRIGGER] = (dxl_padsrc){ DXL_PADSRC_AXIS, 2, 0, 1, 0 };
    char text[512];
    dxl_padmap_format(&m, text, sizeof text);
    CHECK_STR(text, GUID ",A pad of ours,a:b0,dpup:h0.1,lefty:a1~,lefttrigger:+a2,platform:Linux,");
    dxl_padmap back;
    CHECK_INT(dxl_padmap_parse(text, &back), 0);
    CHECK_STR(back.guid, GUID);
    CHECK_STR(back.name, "A pad of ours");
    CHECK_INT(back.src[DXL_PADCTL_A].kind, DXL_PADSRC_BUTTON);
    CHECK_INT(back.src[DXL_PADCTL_DPUP].hat_mask, 1);
    CHECK_INT(back.src[DXL_PADCTL_LEFTY].invert, 1);
    CHECK_INT(back.src[DXL_PADCTL_LEFTTRIGGER].half, 1);
    CHECK_INT(back.src[DXL_PADCTL_B].kind, DXL_PADSRC_NONE);
    CHECK_INT(dxl_padmap_controls(&back), 4);
    CHECK(dxl_padmap_same(&back, &m));
    /* SDL's own text of it, its CRC in a field: the same mapping. */
    dxl_padmap sdl;
    CHECK_INT(dxl_padmap_parse("030000005e0400008e02000014010000,A pad of ours,a:b0,dpup:h0.1,lefty:a1~,"
                               "lefttrigger:+a2,platform:Linux,crc:e58f,", &sdl), 0);
    CHECK(dxl_padmap_same(&sdl, &m));
    sdl.src[DXL_PADCTL_LEFTY].invert = 0;
    CHECK(!dxl_padmap_same(&sdl, &m));
    sdl.src[DXL_PADCTL_LEFTY].invert = 1;
    sdl.src[DXL_PADCTL_B] = (dxl_padsrc){ DXL_PADSRC_BUTTON, 1, 0, 0, 0 };
    CHECK(!dxl_padmap_same(&sdl, &m));
    dxl_padmap none;
    memset(&none, 0, sizeof none);
    CHECK_INT(dxl_padmap_controls(&none), 0);
    /* SDL's own entries: controls this does not keep passed over. */
    CHECK_INT(dxl_padmap_parse(GUID ",X,a:b0,paddle1:b11,misc1:b12,platform:Linux,", &back), 0);
    CHECK_INT(dxl_padmap_parse("short,X,a:b0,", &back), -1);
    CHECK_INT(dxl_padmap_parse(GUID ",X,a:q0,", &back), -1);
    CHECK_INT(dxl_padmap_parse(GUID ",X,dpup:h0,", &back), -1);
}

static void test_guid_crc(void) {
    char out[33], crc[5];
    CHECK_INT(dxl_padmap_guid_no_crc(GUID, out), 1);
    CHECK_STR(out, "030000005e0400008e02000014010000");
    CHECK_INT(dxl_padmap_guid_no_crc(out, out), 0);
    /* Bytes 2 and 3, 8f e5: the CRC 0xe58f, as SDL writes its field. */
    CHECK_INT(dxl_padmap_guid_crc(GUID, crc), 1);
    CHECK_STR(crc, "e58f");
    CHECK_INT(dxl_padmap_guid_crc("03008FE55E0400008E02000014010000", crc), 1);
    CHECK_STR(crc, "e58f");
    CHECK_INT(dxl_padmap_guid_crc(out, crc), 0);
    CHECK_STR(crc, "");
}

static void test_db_and_engine(void) {
    const char *dir = scratch_dir("padmap");
    char *path = scratch_path(dir, "gamecontrollerdb.txt");
    remove(path);
    CHECK(dxl_padmap_engine_config(NULL, path) == NULL);
    dxl_err e;
    CHECK_INT(dxl_padmap_db_put(path, GUID ",Pad,a:b0,platform:Linux,", &e), 0);
    CHECK_INT(dxl_padmap_db_put(path, "050000005e040000e002000003090000,Other,a:b1,platform:Linux,", &e), 0);
    /* The GUID's line replaced, the other kept. */
    CHECK_INT(dxl_padmap_db_put(path, GUID ",Pad,a:b2,platform:Linux,", &e), 0);
    size_t len = 0;
    char *text = slurp(path, &len);
    CHECK_STR(text, GUID ",Pad,a:b2,platform:Linux,\n050000005e040000e002000003090000,Other,a:b1,platform:Linux,\n");
    free(text);
    text = dxl_padmap_db_read(path);
    CHECK(dxl_padmap_db_has(text, GUID));
    CHECK(dxl_padmap_db_has(text, "03008FE55E0400008E02000014010000"));
    CHECK(!dxl_padmap_db_has(text, "030000005e0400008e02000014010000"));
    CHECK(!dxl_padmap_db_has(text, "short"));
    CHECK(!dxl_padmap_db_has("# " GUID ",Pad,a:b0,\n", GUID));
    free(text);
    /* Steam's first, but its line of a GUID of ours; then ours, each with a
     * CRC in its GUID copied without it and with a field naming it. */
    const char *steam = "03008FE55E0400008E02000014010000,Steam's,a:b5,platform:Linux,\n"
                        "030000005e0400008e02000014010000,Steam's bare,a:b6,platform:Linux,\n"
                        "\n"
                        "03000000de2800000112000001000000,Steam Deck,a:b0,platform:Linux,";
    char *config = dxl_padmap_engine_config(steam, path);
    CHECK_STR(config, "030000005e0400008e02000014010000,Steam's bare,a:b6,platform:Linux,\n"
                      "03000000de2800000112000001000000,Steam Deck,a:b0,platform:Linux,\n"
                      GUID ",Pad,a:b2,platform:Linux,\n"
                      "030000005e0400008e02000014010000,Pad,a:b2,platform:Linux,crc:e58f,\n"
                      "050000005e040000e002000003090000,Other,a:b1,platform:Linux,\n");
    free(config);
    char *theirs = dxl_padmap_without_ours(GUID ",Steam's,a:b5,platform:Linux,\n", path);
    CHECK_STR(theirs, "");
    free(theirs);
    CHECK(dxl_padmap_without_ours(NULL, path) == NULL);
    CHECK(dxl_padmap_without_ours("", path) == NULL);
    free(path);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_format_and_parse);
    RUN(test_guid_crc);
    RUN(test_db_and_engine);
TEST_MAIN_END
