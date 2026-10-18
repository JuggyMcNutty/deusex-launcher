/* A profile's saves and screenshots, listed: SaveInfo packages the test
 * writes itself in folders named as the engine names them and not, the
 * newest first, the quick save among them, one that does not read last;
 * pictures and other files, newest first. */
#include "test.h"
#include "scratch.h"
#include "core/saves.h"
#include "core/shotlist.h"
#include "upkg_writer.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>

enum { N_NONE, N_CORE, N_CLASS, N_INFO_CLASS, N_INFO, N_YEAR, N_MONTH, N_DAY, N_HOUR, N_MINUTE, N_SECOND, N_INDEX,
       N_DESC, N_LOCATION, N_MAP, N_TIME, N_NAMES };
static const char *const names[N_NAMES] = {
    "None", "Core", "Class", "DeusExSaveInfo", "MyDeusExSaveInfo", "Year", "Month", "Day", "Hour", "Minute",
    "Second", "DirectoryIndex", "Description", "MissionLocation", "MapName", "saveTime",
};

static void int_prop(buf *b, int name, int value) {
    tag(b, name, DXL_UPROP_INT, 2, 0, 0, 0);
    u32(b, (uint32_t)value);
}

static void str_prop(buf *b, int name, const char *value) {
    tag(b, name, DXL_UPROP_STR, 5, 0, 0, (int)strlen(value) + 2);
    str(b, value);
}

/* A SaveInfo.dxs as the game writes one, its date, its words, its time. */
static void write_info(const char *dir, const char *folder, int slot, const int date[6], const char *desc,
                       const char *location, int seconds) {
    buf obj = { 0 };
    static const int fields[6] = { N_YEAR, N_MONTH, N_DAY, N_HOUR, N_MINUTE, N_SECOND };
    for (int i = 0; i < 6; i++) int_prop(&obj, fields[i], date[i]);
    int_prop(&obj, N_INDEX, slot);
    str_prop(&obj, N_DESC, desc);
    str_prop(&obj, N_LOCATION, location);
    str_prop(&obj, N_MAP, "01_NYC_UNATCOIsland");
    int_prop(&obj, N_TIME, seconds);
    idx(&obj, N_NONE);

    buf h = { 0 };
    u32(&h, DXL_UPKG_TAG);
    u16(&h, 68); u16(&h, 0); u32(&h, 0);
    size_t counts = h.n;
    for (int i = 0; i < 6; i++) u32(&h, 0);
    size_t noff = h.n;
    for (int i = 0; i < N_NAMES; i++) {
        str(&h, names[i]);
        u32(&h, 0);
    }
    size_t ioff = h.n;
    idx(&h, N_CORE); idx(&h, N_CLASS); u32(&h, 0); idx(&h, N_INFO_CLASS);
    size_t off = h.n;
    put(&h, obj.d, obj.n);
    size_t eoff = h.n;
    idx(&h, -1); idx(&h, 0); u32(&h, 0); idx(&h, N_INFO);
    u32(&h, 0x00070004u);
    idx(&h, (int32_t)obj.n);
    idx(&h, (int32_t)off);
    uint32_t v[6] = { N_NAMES, (uint32_t)noff, 1, (uint32_t)eoff, 1, (uint32_t)ioff };
    for (int i = 0; i < 6; i++) memcpy(h.d + counts + 4 * (size_t)i, &v[i], 4);

    char rel[64];
    snprintf(rel, sizeof rel, "%s/SaveInfo.dxs", folder);
    char *sub = scratch_path(dir, folder);
    mkdir(sub, 0755);
    free(sub);
    char *path = scratch_path(dir, rel);
    FILE *f = fopen(path, "wb");
    fwrite(h.d, 1, h.n, f);
    fclose(f);
    free(path);
    free(h.d);
    free(obj.d);
}

static void test_slots(void) {
    int slot = 99;
    CHECK(dxl_save_slot_of("Save0010", &slot) && slot == 10);
    CHECK(dxl_save_slot_of("Save0000", &slot) && slot == 0);
    CHECK(dxl_save_slot_of("Save12345", &slot) && slot == 12345);
    CHECK(dxl_save_slot_of("QuickSave", &slot) && slot == DXL_SAVE_QUICK);
    /* Not a folder the engine loads: another name for a number, another
     * case, the mission in progress. */
    CHECK(!dxl_save_slot_of("Save010", &slot));
    CHECK(!dxl_save_slot_of("save0010", &slot));
    CHECK(!dxl_save_slot_of("Save00a1", &slot));
    CHECK(!dxl_save_slot_of("Save", &slot));
    CHECK(!dxl_save_slot_of("Save-001", &slot));
    CHECK(!dxl_save_slot_of("Current", &slot));
    CHECK(!dxl_save_slot_of("quicksave", &slot));
    char url[32];
    dxl_save_url(10, url, sizeof url);
    CHECK_STR(url, "?loadgame=10");
    dxl_save_url(DXL_SAVE_QUICK, url, sizeof url);
    CHECK_STR(url, "?loadgame=-1");
}

static void test_list(void) {
    const char *dir = scratch_dir("saves");
    static const int older[6] = { 2026, 10, 1, 10, 0, 0 }, newer[6] = { 2026, 10, 14, 20, 47, 15 },
                     newest[6] = { 2026, 10, 15, 8, 0, 0 };
    write_info(dir, "Save0003", 3, older, "Before the statue", "NYC - Liberty Island", 452);
    write_info(dir, "Save0010", 10, newer, "NYC - Liberty Island", "NYC - Liberty Island", 3600 + 5 * 60 + 7);
    write_info(dir, "QuickSave", -1, newest, "Quick Save", "UNATCO HQ", 30);
    /* Not saves: a name the engine does not give, the mission in
     * progress, a file of a save's name. Save0007's info is not there. */
    write_info(dir, "Save010", 10, newest, "Not one", "Nowhere", 1);
    write_info(dir, "Current", -2, newest, "Not one", "Nowhere", 1);
    scratch_write(dir, "Save0004", "a file");
    char *empty = scratch_path(dir, "Save0007");
    mkdir(empty, 0755);
    free(empty);

    dxl_saves s;
    dxl_err e;
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    CHECK_INT(s.count, 4);
    if (s.count == 4) {
        CHECK_STR(s.items[0].folder, "QuickSave");
        CHECK_INT(s.items[0].slot, DXL_SAVE_QUICK);
        CHECK_STR(s.items[1].folder, "Save0010");
        CHECK_STR(s.items[2].folder, "Save0003");
        CHECK_STR(s.items[2].info.description, "Before the statue");
        CHECK_STR(s.items[2].info.location, "NYC - Liberty Island");
        CHECK_STR(s.items[3].folder, "Save0007");
        CHECK_INT(s.items[3].readable, 0);
        CHECK(s.items[0].info.snapshot.rgba == NULL);
        CHECK(dxl_saves_latest(&s) == &s.items[0]);

        char text[64];
        dxl_save_when(&s.items[1], text, sizeof text);
        CHECK_STR(text, "14 October 2026, 20:47");
        dxl_save_when(&s.items[3], text, sizeof text);
        CHECK_STR(text, "");
        dxl_save_played(&s.items[1], text, sizeof text);
        CHECK_STR(text, "1 h 05 min");
        dxl_save_played(&s.items[2], text, sizeof text);
        CHECK_STR(text, "7 min");
        dxl_save_played(&s.items[0], text, sizeof text);
        CHECK_STR(text, "under a minute");
        char *path = dxl_save_path(dir, &s.items[1]);
        char *want = scratch_path(dir, "Save0010");
        CHECK_STR(path, want);
        free(path);
        free(want);
    }
    dxl_saves_free(&s);

    /* None read: nothing to continue. No folder: no saves. */
    char *q = scratch_path(dir, "QuickSave/SaveInfo.dxs");
    remove(q);
    free(q);
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    CHECK_STR(s.items[0].folder, "Save0010");
    dxl_saves_free(&s);
    char *gone = scratch_path(dir, "not-there");
    CHECK_INT(dxl_saves_list(gone, &s, &e), 0);
    CHECK_INT(s.count, 0);
    CHECK(dxl_saves_latest(&s) == NULL);
    dxl_saves_free(&s);
    free(gone);
    scratch_remove(dir);
}

static void stamp(const char *dir, const char *rel, long when) {
    char *path = scratch_path(dir, rel);
    struct timeval tv[2] = { { when, 0 }, { when, 0 } };
    utimes(path, tv);
    free(path);
}

static void test_shots(void) {
    const char *dir = scratch_dir("shots");
    scratch_write(dir, "Shot0001.bmp", "BM one");
    scratch_write(dir, "Shot0002.bmp", "BM two");
    scratch_write(dir, "Shot0003.BMP", "BM three");
    scratch_write(dir, "island.png", "png");
    scratch_write(dir, "notes.txt", "not a picture");
    scratch_write(dir, ".hidden.png", "hidden");
    scratch_write(dir, "empty.bmp", "");
    char *sub = scratch_path(dir, "folder.bmp");
    mkdir(sub, 0755);
    free(sub);
    stamp(dir, "Shot0001.bmp", 1000);
    stamp(dir, "Shot0002.bmp", 3000);
    stamp(dir, "Shot0003.BMP", 2000);
    stamp(dir, "island.png", 3000);

    dxl_shots s;
    dxl_err e;
    CHECK_INT(dxl_shots_list(dir, &s, &e), 0);
    CHECK_INT(s.count, 4);
    if (s.count == 4) {
        /* Of one time, the later name first. */
        CHECK_STR(s.items[0].name, "island.png");
        CHECK_STR(s.items[1].name, "Shot0002.bmp");
        CHECK_STR(s.items[2].name, "Shot0003.BMP");
        CHECK_STR(s.items[3].name, "Shot0001.bmp");
        CHECK_INT(s.items[3].size, 6);
        CHECK_INT(s.items[3].mtime, 1000);
    }
    dxl_shots_free(&s);
    CHECK(dxl_shot_name_ok("a.png"));
    CHECK(!dxl_shot_name_ok(".png"));
    CHECK(!dxl_shot_name_ok("a.jpg"));
    char *gone = scratch_path(dir, "not-there");
    CHECK_INT(dxl_shots_list(gone, &s, &e), 0);
    CHECK_INT(s.count, 0);
    free(gone);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_slots);
    RUN(test_list);
    RUN(test_shots);
TEST_MAIN_END
