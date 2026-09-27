#include "core/cmdline.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *dxl_cmdline_join(int argc, char *const *argv) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (int i = 1; i < argc; i++) dxl_buf_word(&b, argv[i]);
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
        if (after == '\0' || isspace((unsigned char)after)) return 1;
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

    const char *found = dxl_stristr(cmdline, needle);
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
    return dxl_stristr(cmdline, token) != NULL;
}
