#include "test.h"
#include "scratch.h"
#include "core/safemode.h"

/* The SafeOptions page: dx-reverse-info/wizard.md "SafeOptions". Each box
 * gives the original's strings as the shipped binary reads the boxes: boxes
 * 3, 4 and 5 read box 2 (its shipped bug, seen live), their own ticks never
 * read. */

static char *flags(dxl_safe_options o) { return dxl_safe_flags(&o); }

static void test_the_page_opens_with_all_but_reset(void) {
    dxl_safe_options d = dxl_safe_defaults();
    CHECK(d.no_sound && d.no_3d_sound && d.no_3d_video && d.window && d.res &&
          d.no_processor && d.no_joy);
    CHECK_INT(d.reset_config, 0);
    /* As the page opens, the flags are what the original gives too: its bug
     * only shows when box 2 differs from boxes 3 to 5. */
    char *f = flags(d);
    CHECK_STR(f, " -nosound -no3dsound -nohard -nohard -noddraw -defaultres"
                 " -nommx -nokni -nok6 -nojoy");
    free(f);
}

static void test_each_box_alone(void) {
    struct { dxl_safe_options o; const char *want; } cases[] = {
        { { .no_sound = 1 },     " -nosound" },
        { { .no_3d_sound = 1 },  " -no3dsound -nohard -nohard -noddraw -defaultres" },
        { { .no_3d_video = 1 },  "" },
        { { .window = 1 },       "" },
        { { .res = 1 },          "" },
        { { .reset_config = 1 }, "" },
        { { .no_processor = 1 }, " -nommx -nokni -nok6" },
        { { .no_joy = 1 },       " -nojoy" },
        { { 0 },                 "" },
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; i++) {
        char *f = flags(cases[i].o);
        CHECK_STR(f, cases[i].want);
        free(f);
    }
}

/* The live run's case (dx-reverse-info/wizard.md, "Observed under wine"): box 2 cleared,
 * boxes 3 to 5 ticked. The original gives none of their flags: its relaunch
 * runs as DeusEx.exe -nosound -nommx -nokni -nok6 -nojoy. */
static void test_boxes_three_to_five_read_box_two(void) {
    dxl_safe_options o = dxl_safe_defaults();
    o.no_3d_sound = 0;
    char *f = flags(o);
    CHECK_STR(f, " -nosound -nommx -nokni -nok6 -nojoy");
    free(f);
}

static void test_reset_deletes_the_ini(void) {
    const char *dir = scratch_dir("reset");
    scratch_write(dir, "deusex.INI", "[FirstRun]\r\nFirstRun=1100\r\n");
    dxl_err e;
    CHECK_INT(dxl_safe_reset_config(dir, "DeusEx", &e), 0);
    CHECK_INT(scratch_exists(dir, "deusex.INI"), 0);
    /* Already gone is fine. */
    CHECK_INT(dxl_safe_reset_config(dir, "DeusEx", &e), 0);
    scratch_remove(dir);
}

TEST_MAIN_BEGIN
    RUN(test_the_page_opens_with_all_but_reset);
    RUN(test_each_box_alone);
    RUN(test_boxes_three_to_five_read_box_two);
    RUN(test_reset_deletes_the_ini);
TEST_MAIN_END
