/* POSIX hand-over: replace this process with the game command.
 *
 * exec is the POSIX analogue of the original's ShellExecute-then-exit, and
 * avoids a window where both processes are alive.
 */
#include "platform/launch.h"

#include "core/argv.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

int dxl_platform_launch(const char *exe, const char *flags, const char *workdir,
                        dxl_err *err) {
    if (workdir && *workdir && chdir(workdir) != 0) {
        dxl_err_set(err, "cannot enter %s: %s", workdir, strerror(errno));
        return -1;
    }
    char **argv = dxl_argv_build(exe, flags);
    execv(exe, argv);

    /* Only reached on failure -- exec does not return on success. */
    dxl_err_set(err, "cannot exec %s: %s", exe, strerror(errno));
    dxl_argv_free(argv);
    return -1;
}
