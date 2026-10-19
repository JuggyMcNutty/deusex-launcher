#include "core/argv.h"

#include <stdlib.h>

char **dxl_argv_line(const char *exe, const char *line) {
    char **argv = dxl_xmalloc(3 * sizeof *argv);
    size_t n = 0;
    argv[n++] = dxl_xstrdup(exe);
    if (line && *line) argv[n++] = dxl_xstrdup(line);
    argv[n] = NULL;
    return argv;
}

void dxl_argv_free(char **argv) {
    if (!argv) return;
    for (char **p = argv; *p; p++) free(*p);
    free(argv);
}
