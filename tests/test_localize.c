#include "test.h"
#include "scratch.h"
#include "core/localize.h"

/* How the original reads its .int files: dx-reverse-info/wizard.md "How a
 * page gets its text". */

static const char *sys;

static void setup(void) {
    const char *dir = scratch_dir("loc");
    scratch_write(dir, "System/Startup.int",
                  "[General]\r\nRun=Run!\r\nWorldHigh=\"High detail textures\"\r\n"
                  "Half=\"only one quote\r\nEmpty=\r\n"
                  "[IDDIALOG_ConfigPageDriver]\r\nIDC_DriverInfo=One.\\n\\nTwo.\r\n");
    scratch_write(dir, "System/window.INT", "[General]\r\nNextButton=&Next >\r\n");
    scratch_write(dir, "System/Startup.frt", "[General]\r\nRun=Lancer!\r\n");
    static char path[300];
    snprintf(path, sizeof path, "%s/System", dir);
    sys = path;
}

static void test_values_and_quotes(void) {
    dxl_loc *l = dxl_loc_open(sys, NULL);
    CHECK_STR(dxl_loc_general(l, "Startup", "Run"), "Run!");
    /* Both quotes go, as UE1's reader drops them... */
    CHECK_STR(dxl_loc_general(l, "Startup", "WorldHigh"), "High detail textures");
    /* ...and only when both are there. */
    CHECK_STR(dxl_loc_general(l, "Startup", "Half"), "\"only one quote");
    CHECK_STR(dxl_loc_general(l, "Startup", "Empty"), "");
    /* A file in another case is found; the text is returned raw, & and all. */
    CHECK_STR(dxl_loc_general(l, "Window", "NextButton"), "&Next >");
    dxl_loc_free(l);
}

static void test_missing_strings(void) {
    dxl_loc *l = dxl_loc_open(sys, "int");
    CHECK_STR(dxl_loc_general(l, "Startup", "Nope"), "<?int?Startup.General.Nope?>");
    CHECK_STR(dxl_loc_get(l, "Nowhere", "General", "Run", 0), "<?int?Nowhere.General.Run?>");
    CHECK_STR(dxl_loc_get(l, "Startup", "Descriptions", "Nope", 1), "");
    dxl_loc_free(l);
}

/* A language's own file first, then the international one. */
static void test_language_falls_back_to_int(void) {
    dxl_loc *l = dxl_loc_open(sys, "frt");
    CHECK_STR(dxl_loc_general(l, "Startup", "Run"), "Lancer!");
    CHECK_STR(dxl_loc_general(l, "Startup", "WorldHigh"), "High detail textures");
    CHECK_STR(dxl_loc_general(l, "Startup", "Nope"), "<?frt?Startup.General.Nope?>");
    dxl_loc_free(l);
}

static void test_line_format(void) {
    dxl_loc *l = dxl_loc_open(sys, NULL);
    char *s = dxl_loc_line_format(dxl_loc_get(l, "Startup", "IDDIALOG_ConfigPageDriver",
                                              "IDC_DriverInfo", 0));
    CHECK_STR(s, "One.\n\nTwo.");
    free(s);
    dxl_loc_free(l);
    /* Any other escaped character stays, without its backslash. */
    s = dxl_loc_line_format("a\\\\b\\qc\\");
    CHECK_STR(s, "a\\bqc");
    free(s);
}

/* An .int in UTF-16 reads as the original reads it, an 8-bit one as
 * Latin-1: UTF-8 either way, as the pages draw it (bugs.md MAIN.9). */
static void test_utf16_and_latin1(void) {
    static const char wide[] = "\xff\xfe[\0G\0e\0n\0e\0r\0a\0l\0]\0\r\0\n\0R\0u\0n\0=\0\xa9\x03!\0\r\0\n\0";
    char *p = scratch_path(scratch_dir("loc"), "System/Wide.int");
    FILE *f = fopen(p, "wb");   /* scratch_write's fputs stops at a NUL */
    CHECK(f != NULL);
    if (f) { fwrite(wide, 1, sizeof wide - 1, f); fclose(f); }
    free(p);
    scratch_write(scratch_dir("loc"), "System/Startup.det", "[General]\r\nRun=Los geht\xb4s! \xc4\xd6\xdc\r\n");
    dxl_loc *l = dxl_loc_open(sys, "det");
    CHECK_STR(dxl_loc_general(l, "Wide", "Run"), "\xce\xa9!");
    CHECK_STR(dxl_loc_general(l, "Startup", "Run"), "Los geht\xc2\xb4s! \xc3\x84\xc3\x96\xc3\x9c");
    dxl_loc_free(l);
}

TEST_MAIN_BEGIN
    setup();
    RUN(test_values_and_quotes);
    RUN(test_missing_strings);
    RUN(test_language_falls_back_to_int);
    RUN(test_line_format);
    RUN(test_utf16_and_latin1);
    scratch_remove(scratch_dir("loc"));
TEST_MAIN_END
