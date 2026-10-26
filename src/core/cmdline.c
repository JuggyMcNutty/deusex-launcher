#include "core/cmdline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* appStrfind (Core.dll 0x10145ca0): a substring in any case, but none
 * straight after a letter or digit -- INI= is not found within USERINI=, nor
 * LOG= within ABSLOG= (dx-reverse-info/cli-flags.md). */
static int letter_or_digit(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

static const char *strfind(const char *hay, const char *needle) {
    size_t n = strlen(needle);
    if (!hay || !n) return NULL;
    for (const char *p = hay; *p; p++) {
        if (p > hay && letter_or_digit(p[-1])) continue;
        if (dxl_strnicmp(p, needle, n) == 0) return p;
    }
    return NULL;
}

/* A word onto the line: one holding a space is quoted -- after its NAME=
 * when it has one, as the original's raw line keeps its quotes and Parse
 * reads them (dxl_cmd_value too) -- unless it holds a quote already or
 * begins with a blank, a relaunch's whole line handed over as one word. */
static void quoted_word(dxl_buf *b, const char *a) {
    if (!*a) return;
    if (!strchr(a, ' ') || strchr(a, '"') || *a == ' ' || *a == '\t') {
        dxl_buf_word(b, a);
        return;
    }
    const char *eq = strchr(a, '=');
    if (b->len) dxl_buf_add(b, " ", 1);
    if (eq) dxl_buf_add(b, a, (size_t)(eq - a) + 1);
    dxl_buf_add(b, "\"", 1);
    dxl_buf_puts(b, eq ? eq + 1 : a);
    dxl_buf_add(b, "\"", 1);
}

char *dxl_cmdline_join(int argc, char *const *argv) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (int i = 1; i < argc; i++) quoted_word(&b, argv[i]);
    if (!b.data) dxl_buf_puts(&b, "");
    size_t blanks = strspn(b.data, " \t");
    memmove(b.data, b.data + blanks, strlen(b.data + blanks) + 1);
    return b.data;
}

int dxl_cmd_param(const char *cmdline, const char *name) {
    if (!cmdline || !*cmdline || !name || !*name) return 0;
    size_t n = strlen(name);

    /* UE1 starts the scan at cmdline+1: a match at offset 0 could not have a
     * '-' before it anyway. */
    for (const char *p = cmdline + 1; (p = dxl_stristr(p, name)) != NULL; p++) {
        if (p == cmdline) continue;
        char before = p[-1];
        if (before != '-' && before != '/') continue;
        char after = p[n];
        if (after == '\0' || isspace((unsigned char)after) || after == '?') return 1;
    }
    return 0;
}

int dxl_cmd_value(const char *cmdline, const char *name, char *out, size_t size) {
    if (size) out[0] = '\0';
    if (!cmdline || !name) return 0;

    /* The caller passes the bare name; the '=' is part of what we search for,
     * matching Parse(Stream,"EXEC=",...). */
    size_t nlen = strlen(name);
    char *needle = dxl_xmalloc(nlen + 2);
    memcpy(needle, name, nlen);
    needle[nlen] = '=';
    needle[nlen + 1] = '\0';

    const char *found = strfind(cmdline, needle);
    free(needle);
    if (!found) return 0;

    const char *start = found + nlen + 1;
    size_t i = 0;
    if (*start == '"') {
        start++;
        while (*start && *start != '"') {
            if (i + 1 < size) out[i] = *start;
            i++; start++;
        }
    } else {
        while (*start && !isspace((unsigned char)*start)) {
            if (i + 1 < size) out[i] = *start;
            i++; start++;
        }
    }
    if (size) out[(i < size) ? i : size - 1] = '\0';
    return 1;
}

int dxl_cmd_find(const char *cmdline, const char *token) {
    return strfind(cmdline, token) != NULL;
}
