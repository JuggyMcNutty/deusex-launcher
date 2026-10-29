/* A profile's saves and screenshots, listed: SaveInfo packages the test
 * writes itself in folders named as the engine names them and not, the
 * newest first, the quick save among them, one that does not read last;
 * pictures and other files, newest first. */
#include "test.h"
#include "scratch.h"
#include "core/saves.h"
#include "core/shotlist.h"
#include "upkg_writer.h"
#include "platform/fs.h"

#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>

/* A SaveInfo.dxs as the game writes one, its date, its words, its time. */
static void write_info(const char *dir, const char *folder, int slot, const int date[6], const char *desc,
                       const char *location, int seconds) {
    dxl_test_save sv = { { date[0], date[1], date[2], date[3], date[4], date[5] }, slot, desc, location, NULL,
                         seconds, slot == 10 };
    CHECK_INT(dxl_test_write_save(dir, folder, &sv), 0);
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
    /* An info by another case of its name: not one the engine opens, where
     * names keep their case (on Windows, whose names take any, it opens it). */
#ifdef _WIN32
    const int listed = 4;
#else
    const int listed = 5;
    write_info(dir, "Save0011", 11, newest, "Lower case", "Nowhere", 1);
    char *was = scratch_path(dir, "Save0011/SaveInfo.dxs"), *lower = scratch_path(dir, "Save0011/saveinfo.dxs");
    rename(was, lower);
    free(was);
    free(lower);
#endif
    char *empty = scratch_path(dir, "Save0007");
    dxl_fs_mkdir(empty);
    free(empty);

    dxl_saves s;
    dxl_err e;
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    CHECK_INT(s.count, listed);
    /* A profile's count of them is the list's. */
    CHECK_INT(dxl_saves_count(dir), listed);
    if (s.count == listed) {
        CHECK_STR(s.items[0].folder, "QuickSave");
        CHECK_INT(s.items[0].slot, DXL_SAVE_QUICK);
        CHECK_STR(s.items[1].folder, "Save0010");
        CHECK_STR(s.items[2].folder, "Save0003");
        CHECK_STR(s.items[2].info.description, "Before the statue");
        CHECK_STR(s.items[2].info.location, "NYC - Liberty Island");
        CHECK_STR(s.items[3].folder, "Save0007");
        CHECK_INT(s.items[3].readable, 0);
        if (listed == 5) {
            CHECK_STR(s.items[4].folder, "Save0011");
            CHECK_INT(s.items[4].readable, 0);
        }
        CHECK(s.items[0].info.snapshot.rgba == NULL);
        /* Its snapshot, read when asked for: the writer's picture in it. */
        dxl_saveinfo full;
        char *info = scratch_path(dir, "Save0010/SaveInfo.dxs");
        CHECK_INT(dxl_saveinfo_read(info, 1, &full, &e), 0);
        CHECK(full.snapshot.rgba && full.snapshot.w == 256 && full.snapshot.h == 128);
        if (full.snapshot.rgba) CHECK_INT(full.snapshot.rgba[0], dxl_ts_pixel(0, 0, 10));
        dxl_saveinfo_free(&full);
        free(info);
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

    /* The quick save's info gone: it no longer reads, and the newest that
     * does is Save0010. No folder: no saves, nothing to continue. */
    char *q = scratch_path(dir, "QuickSave/SaveInfo.dxs");
    remove(q);
    free(q);
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    CHECK(s.count > 0);
    if (s.count > 0) CHECK_STR(s.items[0].folder, "Save0010");
    dxl_saves_free(&s);
    char *gone = scratch_path(dir, "not-there");
    CHECK_INT(dxl_saves_list(gone, &s, &e), 0);
    CHECK_INT(s.count, 0);
    CHECK(dxl_saves_latest(&s) == NULL);
    dxl_saves_free(&s);
    free(gone);
    scratch_remove(dir);
}

/* A save's words for showing: UTF-8 as they are, the original's Latin-1
 * made UTF-8, cut at a whole character. */
static void test_text(void) {
    char out[32];
    dxl_text_utf8("Caf\xe9 Tonnerre", out, sizeof out);
    CHECK_STR(out, "Caf\xc3\xa9 Tonnerre");
    dxl_text_utf8("Caf\xc3\xa9", out, sizeof out);
    CHECK_STR(out, "Caf\xc3\xa9");
    dxl_text_utf8("\xff\xfe", out, sizeof out);
    CHECK_STR(out, "\xc3\xbf\xc3\xbe");
    dxl_text_utf8("Caf\xe9", out, 5);
    CHECK_STR(out, "Caf");
    dxl_text_utf8("\xe2\x82\xac!", out, 4);
    CHECK_STR(out, "\xe2\x82\xac");
    dxl_text_utf8(NULL, out, sizeof out);
    CHECK_STR(out, "");
    char cut[] = "Caf\xc3", whole[] = "Caf\xc3\xa9", latin[] = "Caf\xe9";
    dxl_text_drop_cut(cut);
    CHECK_STR(cut, "Caf");
    dxl_text_drop_cut(whole);
    CHECK_STR(whole, "Caf\xc3\xa9");
    dxl_text_drop_cut(latin);
    CHECK_STR(latin, "Caf");
    dxl_text_utf8("\xf5\xa1\xa2", out, sizeof out);   /* F5: no lead byte */
    CHECK_STR(out, "\xc3\xb5\xc2\xa1\xc2\xa2");
    /* Through a save the original wrote. */
    const char *dir = scratch_dir("saves-text");
    static const int when[6] = { 2026, 9, 24, 22, 28, 41 };
    write_info(dir, "Save0001", 1, when, "Caf\xe9 du Monde", "Paris - Denfert-Rochereau", 94);
    dxl_saves s;
    dxl_err e;
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    CHECK(s.count == 1 && !strcmp(s.items[0].info.description, "Caf\xc3\xa9 du Monde"));
    dxl_saves_free(&s);
    /* A description past 63 characters: its property's size two bytes in
     * the writer's tag, read whole. */
    char longdesc[101];
    memset(longdesc, 'd', 100);
    longdesc[100] = '\0';
    write_info(dir, "Save0002", 2, when, longdesc, "Paris", 95);
    CHECK_INT(dxl_saves_list(dir, &s, &e), 0);
    int found = 0;
    for (int i = 0; i < s.count; i++)
        if (!strcmp(s.items[i].info.description, longdesc)) found = 1;
    CHECK_INT(found, 1);
    dxl_saves_free(&s);
    scratch_remove(dir);
}

static void stamp(const char *dir, const char *rel, long when) {
    char *path = scratch_path(dir, rel);
    dxl_fs_set_mtime(path, when);
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
    dxl_fs_mkdir(sub);
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
    RUN(test_text);
    RUN(test_shots);
TEST_MAIN_END
