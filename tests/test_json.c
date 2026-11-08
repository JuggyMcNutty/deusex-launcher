#include "test.h"
#include "core/json.h"

/* What the engine wrote on the device (Settings.json, 2026-09-22). */
static const char *engine_file =
    "{\n"
    "  \"RenderDevice\": {\n"
    "    \"Type\": \"Vulkan\",\n"
    "    \"Antialias\": \"Off\",\n"
    "    \"UseVSync\": false,\n"
    "    \"HdrScale\": 128,\n"
    "    \"UseDebugLayer\": false\n"
    "  },\n"
    "  \"Games\": {\n"
    "    \"SearchList\": [\"/mnt/SDCARD/Roms/PORTS/DeusEx\"],\n"
    "    \"LastSelected\": 0\n"
    "  }\n"
    "}\n";

static dxl_json *parse(const char *s) {
    dxl_err e;
    dxl_json *v = dxl_json_parse(s, strlen(s), &e);
    if (!v) fprintf(stderr, "  (parse error: %s)\n", dxl_err_msg(&e));
    return v;
}

static void test_reads_the_engine_file(void) {
    dxl_json *v = parse(engine_file);
    CHECK(v != NULL);
    dxl_json *rd = dxl_json_get(v, "RenderDevice");
    CHECK_STR(dxl_json_get_string(rd, "Type", NULL), "Vulkan");
    CHECK_INT(dxl_json_get_bool(rd, "UseVSync", 1), 0);
    CHECK_INT((int)dxl_json_get_number(rd, "HdrScale", 0), 128);
    CHECK(dxl_json_get(v, "Missing") == NULL);
    CHECK_STR(dxl_json_get_string(rd, "Nope", "fallback"), "fallback");
    dxl_json_free(v);
}

/* Members we do not understand -- Games, with its array -- must come back
 * out, in order, exactly as values. */
static void test_round_trip_keeps_unknown_members(void) {
    dxl_json *v = parse(engine_file);
    char *out = dxl_json_render(v);
    dxl_json *again = parse(out);
    CHECK(again != NULL);
    char *out2 = dxl_json_render(again);
    CHECK_STR(out2, out);
    CHECK(strstr(out, "\"SearchList\": [") != NULL);
    CHECK(strstr(out, "\"/mnt/SDCARD/Roms/PORTS/DeusEx\"") != NULL);
    /* RenderDevice still precedes Games. */
    CHECK(strstr(out, "RenderDevice") < strstr(out, "Games"));
    free(out); free(out2);
    dxl_json_free(v); dxl_json_free(again);
}

static void test_set_replaces_in_place(void) {
    dxl_json *v = parse(engine_file);
    dxl_json *rd = dxl_json_get(v, "RenderDevice");
    dxl_json_set_string(rd, "Type", "GLES");
    dxl_json_set_bool(rd, "UseVSync", 1);
    dxl_json_set_number(rd, "Brand", 0.25);
    char *out = dxl_json_render(v);
    /* Type keeps its slot, before Antialias; the new member goes last. */
    CHECK(strstr(out, "\"Type\": \"GLES\"") != NULL);
    CHECK(strstr(out, "\"Type\"") < strstr(out, "\"Antialias\""));
    CHECK(strstr(out, "\"UseVSync\": true") != NULL);
    CHECK(strstr(out, "\"Brand\": 0.25") != NULL);
    CHECK(strstr(out, "\"UseDebugLayer\"") < strstr(out, "\"Brand\""));
    free(out);
    dxl_json_free(v);
}

/* The engine writes non-integers as std::to_string does ("0.150000"). An
 * untouched value keeps that spelling; integers are written without a
 * fraction. */
static void test_number_spelling(void) {
    dxl_json *v = parse("{\"a\": 0.150000, \"b\": 128}");
    dxl_json_set_number(v, "a", 0.15);      /* same value: keep spelling */
    dxl_json_set_number(v, "b", 64);
    dxl_json_set_number(v, "c", 1.0 / 3.0);
    char *out = dxl_json_render(v);
    CHECK(strstr(out, "\"a\": 0.150000") != NULL);
    CHECK(strstr(out, "\"b\": 64") != NULL);
    dxl_json *back = parse(out);
    CHECK(dxl_json_get_number(back, "c", 0) == 1.0 / 3.0);
    free(out);
    dxl_json_free(v); dxl_json_free(back);
}

static void test_string_escapes(void) {
    dxl_json *v = parse("{\"s\": \"a\\\"b\\\\c\\n\\u00e9\\ud83d\\ude00\"}");
    CHECK(v != NULL);
    CHECK_STR(dxl_json_get_string(v, "s", NULL), "a\"b\\c\n\xc3\xa9\xf0\x9f\x98\x80");
    char *out = dxl_json_render(v);
    dxl_json *back = parse(out);
    CHECK_STR(dxl_json_get_string(back, "s", NULL), dxl_json_get_string(v, "s", NULL));
    free(out);
    dxl_json_free(v); dxl_json_free(back);
}

/* What the engine's reader refuses too, which costs the whole file there:
 * known for what it is. */
static void test_rejects_malformed(void) {
    const char *bad[] = {
        "", "{", "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}", "{\"a\":1,,}", "[1,2,]", "[,]", "[1,2",
        "{\"a\":tru}", "{\"a\":1} x", "{\"a\": 1}\n{\"a\": 2}", "{\"a\":\"unterminated}",
        "{\"a\":01x}", "{\"a\": +1}", "{\"a\": .5}", "{\"a\": -x}", "{\"a\": 1e5e5}",
        "\v{\"a\": 1}",
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        dxl_err e;
        dxl_json *v = dxl_json_parse(bad[i], strlen(bad[i]), &e);
        if (v) fprintf(stderr, "  accepted: %s\n", bad[i]);
        CHECK(v == NULL);
        dxl_json_free(v);
    }
}

/* n arrays, one inside the next, the innermost holding inner. */
static char *nested(int n, const char *inner) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (int i = 0; i < n; i++) dxl_buf_add(&b, "[", 1);
    dxl_buf_puts(&b, inner);
    for (int i = 0; i < n; i++) dxl_buf_add(&b, "]", 1);
    return b.data;
}

/* What the engine's reader takes (DXEngine's JsonValue.cpp), so the engine
 * plays the file as it stands: taken here too, not called unreadable and
 * moved aside, the defaults written over it, at the launcher's next save. */
static void test_reads_what_the_engine_reads(void) {
    /* A number as loose as read_number spells it, its value atof's. */
    static const struct { const char *spelling; double want; } numbers[] = {
        { "-.5", -0.5 }, { "1e", 1 }, { "2.5E+", 2.5 }, { "-", 0 }, { "1.", 1 }, { "-e5", 0 },
    };
    for (size_t i = 0; i < sizeof numbers / sizeof *numbers; i++) {
        char text[64], written[64];
        snprintf(text, sizeof text, "{\"GammaOffset\": %s}", numbers[i].spelling);
        snprintf(written, sizeof written, "\"GammaOffset\": %s\n", numbers[i].spelling);
        dxl_json *v = parse(text);
        CHECK(v != NULL);
        if (!v) continue;
        CHECK(dxl_json_get_number(v, "GammaOffset", 99) == numbers[i].want);
        /* Untouched, it is written back as it came. */
        char *out = dxl_json_render(v);
        CHECK(strstr(out, written) != NULL);
        free(out);
        dxl_json_free(v);
    }

    /* A tab and a byte 0x01 in a string, as they stand; written escaped, and
     * read back the same. */
    dxl_json *v = parse("{\"Type\": \"Vulkan\t\x01\"}");
    CHECK(v != NULL);
    if (v) {
        CHECK_STR(dxl_json_get_string(v, "Type", NULL), "Vulkan\t\x01");
        char *out = dxl_json_render(v);
        dxl_json *back = parse(out);
        CHECK_STR(dxl_json_get_string(back, "Type", NULL), "Vulkan\t\x01");
        dxl_json_free(back);
        free(out);
    }
    dxl_json_free(v);

    /* Form feeds among the blanks, before, inside and after the document. */
    v = parse("\f{\f\"a\"\f:\f1\f}\f");
    CHECK(v != NULL);
    if (v) CHECK_INT((int)dxl_json_get_number(v, "a", 0), 1);
    dxl_json_free(v);

    /* Values nested 512 deep, the document the first: 512 arrays, or 511
     * around a number; one more refused. */
    static const struct { int arrays; const char *inner; int read; } depths[] = {
        { 512, "", 1 }, { 513, "", 0 }, { 511, "1", 1 }, { 512, "1", 0 },
    };
    for (size_t i = 0; i < sizeof depths / sizeof *depths; i++) {
        char *text = nested(depths[i].arrays, depths[i].inner);
        dxl_err e;
        v = dxl_json_parse(text, strlen(text), &e);
        if (!v != !depths[i].read)
            fprintf(stderr, "  %d arrays around \"%s\": %s\n", depths[i].arrays,
                    depths[i].inner, v ? "read" : dxl_err_msg(&e));
        CHECK_INT(v != NULL, depths[i].read);
        dxl_json_free(v);
        free(text);
    }
}

static void test_child_creates_and_replaces(void) {
    dxl_json *v = parse("{\"Gamepad\": 5}");
    dxl_json *g = dxl_json_child(v, "Gamepad");
    CHECK_INT(dxl_json_kind(g), DXL_JSON_OBJECT);
    dxl_json_set_bool(g, "Enabled", 1);
    dxl_json *n = dxl_json_child(v, "New");
    dxl_json_set_string(n, "k", "v");
    char *out = dxl_json_render(v);
    CHECK(strstr(out, "\"Enabled\": true") != NULL);
    CHECK(strstr(out, "\"New\": {") != NULL);
    free(out);
    dxl_json_free(v);
}

/* An array's elements and an object's values, in order; nothing of
 * anything else. */
static void test_elements(void) {
    const char *text = "{\"a\": [1, \"two\", {\"k\": true}], \"b\": 2}";
    dxl_json *v = dxl_json_parse(text, strlen(text), NULL);
    CHECK(v != NULL);
    dxl_json *a = dxl_json_get(v, "a");
    CHECK_INT((int)dxl_json_count(a), 3);
    CHECK_INT(dxl_json_kind(dxl_json_at(a, 1)), DXL_JSON_STRING);
    CHECK_INT(dxl_json_get_bool(dxl_json_at(a, 2), "k", 0), 1);
    CHECK(dxl_json_at(a, 3) == NULL);
    CHECK_INT((int)dxl_json_count(v), 2);
    CHECK_INT((int)dxl_json_get_number(dxl_json_at(v, 1), "x", 7), 7);
    CHECK_INT((int)dxl_json_count(dxl_json_at(a, 0)), 0);
    CHECK_INT((int)dxl_json_count(NULL), 0);
    CHECK(dxl_json_at(NULL, 0) == NULL);
    dxl_json_free(v);
}

/* A member given twice is the last given, as the engine's reader (a
 * std::map) keeps it: read there, set there, written once. */
static void test_last_member_wins(void) {
    dxl_json *v = parse("{\"RenderDevice\": {\"Type\": \"Vulkan\", \"Type\": \"OpenGL\"}}");
    CHECK(v != NULL);
    if (!v) return;
    dxl_json *rd = dxl_json_get(v, "RenderDevice");
    CHECK_STR(dxl_json_get_string(rd, "Type", NULL), "OpenGL");
    CHECK_INT((int)dxl_json_count(rd), 1);
    dxl_json_free(v);
}

TEST_MAIN_BEGIN
    RUN(test_reads_the_engine_file);
    RUN(test_round_trip_keeps_unknown_members);
    RUN(test_set_replaces_in_place);
    RUN(test_number_spelling);
    RUN(test_string_escapes);
    RUN(test_rejects_malformed);
    RUN(test_reads_what_the_engine_reads);
    RUN(test_last_member_wins);
    RUN(test_child_creates_and_replaces);
    RUN(test_elements);
TEST_MAIN_END
