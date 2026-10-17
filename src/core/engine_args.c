#include "core/engine_args.h"
#include "core/argv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **v;
    size_t n, cap;
} words;

static void push(words *w, char *s) {
    if (w->n + 2 > w->cap) {
        w->cap = w->cap ? w->cap * 2 : 16;
        w->v = dxl_xrealloc(w->v, w->cap * sizeof *w->v);
    }
    w->v[w->n++] = s;
    w->v[w->n] = NULL;
}

static char *option(const char *name, const char *value) {
    size_t n = strlen(name) + strlen(value) + 2;
    char *s = dxl_xmalloc(n);
    snprintf(s, n, "%s=%s", name, value);
    return s;
}

char **dxl_engine_argv(const dxl_engine_command *c) {
    words w = { 0 };
    push(&w, dxl_xstrdup(c->engine));
    push(&w, dxl_xstrdup("--no-launcher"));
    push(&w, dxl_xstrdup(c->game));
    push(&w, option("--ini", c->ini));
    push(&w, option("--userini", c->user_ini));
    push(&w, option("--userdir", c->user_dir));
    if (c->url && *c->url) push(&w, option("--url", c->url));
    if (c->extra && *c->extra) {
        char **extra = dxl_argv_build("", c->extra);
        for (size_t i = 1; extra[i]; i++) push(&w, dxl_xstrdup(extra[i]));
        dxl_argv_free(extra);
    }
    push(&w, option("--cmdline", c->cmdline ? c->cmdline : ""));
    return w.v;
}

/* A word a shell takes as it is: no quoting needed. */
static int plain(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (!strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_=./:,+@%", *s))
            return 0;
    return 1;
}

char *dxl_argv_quoted(char *const *argv) {
    dxl_buf b;
    dxl_buf_init(&b);
    for (size_t i = 0; argv && argv[i]; i++) {
        if (i) dxl_buf_puts(&b, " ");
        if (plain(argv[i])) {
            dxl_buf_puts(&b, argv[i]);
            continue;
        }
        dxl_buf_puts(&b, "'");
        for (const char *s = argv[i]; *s; s++) {
            if (*s == '\'') dxl_buf_puts(&b, "'\\''");
            else dxl_buf_add(&b, s, 1);
        }
        dxl_buf_puts(&b, "'");
    }
    if (!b.data) dxl_buf_add(&b, "", 0);
    return b.data;
}

int dxl_engine_version_parse(const char *text, dxl_engine_version *out) {
    memset(out, 0, sizeof *out);
    const char *line = text ? text : "";
    int is_vibe = 0;
    while (*line) {
        const char *end = strchr(line, '\n');
        size_t n = end ? (size_t)(end - line) : strlen(line);
        if (n && line[n - 1] == '\r') n--;
        if (n >= 11 && strncmp(line, "VibeEngine ", 11) == 0) {
            snprintf(out->version, sizeof out->version, "%.*s", (int)n, line);
            is_vibe = 1;
        } else if (n >= 9 && strncmp(line, "features:", 9) == 0) {
            const char *f = line + 9;
            size_t m = n - 9;
            while (m && (*f == ' ' || *f == '\t')) { f++; m--; }
            snprintf(out->features, sizeof out->features, "%.*s", (int)m, f);
        }
        if (!end) break;
        line = end + 1;
    }
    return is_vibe ? 0 : -1;
}

int dxl_engine_has(const dxl_engine_version *v, const char *feature) {
    size_t n = strlen(feature);
    for (const char *f = v->features; *f; ) {
        while (*f == ' ') f++;
        const char *end = f;
        while (*end && *end != ' ') end++;
        if ((size_t)(end - f) == n && strncmp(f, feature, n) == 0) return 1;
        f = end;
    }
    return 0;
}
