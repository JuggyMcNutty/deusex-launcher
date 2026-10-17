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

/* The engine silently discards the whole file on any parse error, so we
 * must at least know when we are looking at one. */
static void test_rejects_malformed(void) {
    const char *bad[] = {
        "", "{", "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}", "[1,2", "{\"a\":tru}",
        "{\"a\":1} x", "{\"a\":\"unterminated}", "{\"a\":01x}",
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        dxl_err e;
        dxl_json *v = dxl_json_parse(bad[i], strlen(bad[i]), &e);
        if (v) fprintf(stderr, "  accepted: %s\n", bad[i]);
        CHECK(v == NULL);
        dxl_json_free(v);
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

TEST_MAIN_BEGIN
    RUN(test_reads_the_engine_file);
    RUN(test_round_trip_keeps_unknown_members);
    RUN(test_set_replaces_in_place);
    RUN(test_number_spelling);
    RUN(test_string_escapes);
    RUN(test_rejects_malformed);
    RUN(test_child_creates_and_replaces);
TEST_MAIN_END
