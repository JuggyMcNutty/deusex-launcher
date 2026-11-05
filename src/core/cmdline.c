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

/* The player's words as one line. A word holding a space is quoted --
 * after its NAME= when it has one, as the original's raw line keeps its
 * quotes and the engine's Parse reads them (dxl_cmd_value too); one that
 * holds a quote already goes as it is. */
char *dxl_cmdline_join(int argc, char *const *argv) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!*a) continue;
        if (!strchr(a, ' ') || strchr(a, '"')) {
            dxl_buf_word(&b, a);
            continue;
        }
        const char *eq = strchr(a, '=');
        if (b.len) dxl_buf_add(&b, " ", 1);
        if (eq) dxl_buf_add(&b, a, (size_t)(eq - a) + 1);
        dxl_buf_add(&b, "\"", 1);
        dxl_buf_puts(&b, eq ? eq + 1 : a);
        dxl_buf_add(&b, "\"", 1);
    }
    if (!b.data) dxl_buf_puts(&b, "");
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

/* A character's length in the original's wide line, at its UTF-8 lead byte:
 * a continuation byte none, a four-byte character two (its UTF-16 pair). */
static size_t utf16_units(unsigned char c) {
    return (c & 0xc0) == 0x80 ? 0 : c >= 0xf0 ? 2 : 1;
}

int dxl_cmd_value(const char *cmdline, const char *name, size_t len, char *out, size_t size) {
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

    /* Parse copies the value's first len - 1 characters (appStrncpy, Core.dll
     * 0x101233c0), past the quote of a quoted one, then cuts it at its first
     * quote, or one not quoted at its first space, CR, LF, tab or comma
     * (0x10146c4e to 0x10146c96). */
    const char *start = found + nlen + 1;
    int quoted = *start == '"';
    if (quoted) start++;
    const char *stops = quoted ? "\"" : " \r\n\t,";
    size_t i = 0, units = 0;
    for (; start[i] && !strchr(stops, start[i]); i++) {
        units += utf16_units((unsigned char)start[i]);
        if (units >= len) break;
        if (i + 1 < size) out[i] = start[i];
    }
    if (size) out[(i < size) ? i : size - 1] = '\0';
    return 1;
}

int dxl_cmd_find(const char *cmdline, const char *token) {
    return strfind(cmdline, token) != NULL;
}
