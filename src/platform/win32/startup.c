/* A start of either program on Windows (platform/startup.h). Ctrl+C,
 * Ctrl+Break and a closed console end it from outside, through the
 * console's handler, on a thread of its own, which for a closed console, a
 * sign-out or a shutdown waits for the run's end, Windows ending the
 * process as it returns. Windows' error boxes are kept
 * from popping up over a crash or a missing DLL, the game's too, which
 * inherits the mode: the launcher reports how it ended. A GUI program has
 * no console of its own: deusex-launcher.exe writes its --help and
 * --version to its parent's, when it has one.
 */
#include "platform/startup.h"
#include "platform/win32/internal.h"

#include <stdio.h>

static volatile int *stop_flag;
static void (*wake_up)(void);
static HANDLE ended;   /* set once the run is done: a closing console waits for it */

static BOOL WINAPI on_console(DWORD event) {
    if (stop_flag) *stop_flag = 1;
    if (wake_up) wake_up();
    /* Windows ends the process as the handler returns for a closed
     * console, a sign-out or a shutdown: the stop is waited for -- Quit to
     * the game, Running.ini taken away, LastRun.ini written -- within what
     * Windows allows (five seconds for the console, more for the rest). */
    if (ended && (event == CTRL_CLOSE_EVENT || event == CTRL_LOGOFF_EVENT || event == CTRL_SHUTDOWN_EVENT))
        WaitForSingleObject(ended, event == CTRL_CLOSE_EVENT ? 4500 : 15000);
    return TRUE;
}

char **dxl_startup_link_argv(char **argv) {
    /* Past the program: quoted, or to the first blank. */
    const wchar_t *p = GetCommandLineW();
    if (*p == L'"') {
        p++;
        while (*p && *p != L'"') p++;
        if (*p) p++;
    } else {
        while (*p && *p != L' ' && *p != L'\t') p++;
    }
    while (*p == L' ' || *p == L'\t') p++;
    if (_wcsnicmp(p, L"\"deusex://", 10) != 0) return NULL;
    p++;
    size_t n = wcslen(p);
    while (n && (p[n - 1] == L' ' || p[n - 1] == L'\t')) n--;
    if (n && p[n - 1] == L'"') n--;
    wchar_t *w = dxl_xmalloc((n + 1) * sizeof *w);
    memcpy(w, p, n * sizeof *w);
    w[n] = 0;
    char **out = dxl_xmalloc(3 * sizeof *out);
    out[0] = argv[0];
    out[1] = dxl_u8(w, 0);
    out[2] = NULL;
    free(w);
    return out;
}

void dxl_startup(volatile int *stop, void (*wake)(void)) {
    stop_flag = stop;
    wake_up = wake;
    ended = CreateEventW(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(on_console, TRUE);
}

void dxl_startup_ended(void) {
    if (ended) SetEvent(ended);
}

/* Before main: a GUI program started from a console writes to it; the
 * error mode for this program and those it starts. */
__attribute__((constructor)) static void startup_early(void) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((!out || out == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!freopen("CONOUT$", "w", stdout)) {}
        if (!freopen("CONOUT$", "w", stderr)) {}
    }
    SetConsoleOutputCP(CP_UTF8);
}
