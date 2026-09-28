#include "test.h"
#include "scratch.h"
#include "core/migrate.h"

/* dx-reverse-info/launch-flow.md section 6: each ..\Save\*.usa gets a slot
 * name where User.ini has none. */

static void test_slot_names_for_usa_files(void) {
    const char *dir = scratch_dir("migrate");
    scratch_write(dir, "System/placeholder", "");
    scratch_write(dir, "Save/Save3.usa", "x");
    scratch_write(dir, "Save/save12.USA", "x");   /* any case */
    scratch_write(dir, "Save/Save.usa", "x");     /* no number: slot 0 */
    scratch_write(dir, "Save/notes.txt", "x");
    scratch_write(dir, "Save/Save0001/SaveInfo.dxs", "x");   /* a Deus Ex save: a folder */
    scratch_write(dir, "Save/Dir.usa/inside", "x");          /* a folder, not a file */

    const char *text = "[UnrealShare.UnrealSlotMenu]\r\nSlotNames[12]=Mine\r\n";
    dxl_ini *user = dxl_ini_parse(text, strlen(text));
    char sys[300];
    snprintf(sys, sizeof sys, "%s/System", dir);
    CHECK_INT(dxl_migrate_saves(sys, user), 2);
    CHECK_STR(dxl_ini_get(user, "UnrealShare.UnrealSlotMenu", "SlotNames[3]"), "Saved game");
    CHECK_STR(dxl_ini_get(user, "UnrealShare.UnrealSlotMenu", "SlotNames[0]"), "Saved game");
    CHECK_STR(dxl_ini_get(user, "UnrealShare.UnrealSlotMenu", "SlotNames[12]"), "Mine");

    /* Run again: nothing left to name. */
    CHECK_INT(dxl_migrate_saves(sys, user), 0);
    dxl_ini_free(user);
    scratch_remove(dir);
}

static void test_no_save_folder(void) {
    const char *dir = scratch_dir("nosave");
    scratch_write(dir, "System/placeholder", "");
    char sys[300];
    snprintf(sys, sizeof sys, "%s/System", dir);
    dxl_ini *user = dxl_ini_new();
    CHECK_INT(dxl_migrate_saves(sys, user), 0);
    CHECK_INT(dxl_ini_dirty(user), 0);
    CHECK_INT(dxl_migrate_saves(sys, NULL), 0);
    dxl_ini_free(user);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_slot_names_for_usa_files);
    RUN(test_no_save_folder);
TEST_MAIN_END
