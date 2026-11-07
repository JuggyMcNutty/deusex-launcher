/* A start of either program on Windows (platform/startup.h). Ctrl+C,
 * Ctrl+Break and a closed console end it from outside, through the
 * console's handler, on a thread of its own, which for a closed console
 * waits for the run's end, Windows ending the process as it returns. A
 * sign-out's and a shutdown's events never come to it: Windows gives them
 * to services, not to a program of the desktop, and never to one that loads
 * user32 or gdi32, as both programs do (bugs.md LPORT.9); a sign-out or a
 * shutdown ends the program where it is. Windows' error boxes are kept
 * from popping up over a crash or a missing DLL, the game's too, which
 * inherits the mode: the launcher reports how it ended. A GUI program has
 * no console of its own: deusex-launcher.exe borrows its parent's, when it
 * has one, for what its start says -- its --help and --version, an option
 * it does not know, a hand-over, a dry run -- and lets go of it as its run
 * begins, its own from then on, as a GUI program started from a cmd.exe
 * is: that console's Ctrl+C and closing no stop of its (bugs.md LPORT.8).
 */
#include "platform/startup.h"
#include "platform/win32/internal.h"

#include <stdio.h>

static atomic_int *stop_flag;
static void (*wake_up)(void);
static HANDLE ended;   /* set once the run is done: a closing console waits for it */

static BOOL WINAPI on_console(DWORD event) {
    if (stop_flag) *stop_flag = 1;
    if (wake_up) wake_up();
    /* Windows ends the process as the handler returns for a closed
     * console: the stop is waited for -- Quit to the game, Running.ini
     * taken away, LastRun.ini written -- within the five seconds Windows
     * allows. */
    if (ended && event == CTRL_CLOSE_EVENT) WaitForSingleObject(ended, 4500);
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

void dxl_startup(atomic_int *stop, void (*wake)(void)) {
    stop_flag = stop;
    wake_up = wake;
    ended = CreateEventW(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(on_console, TRUE);
}

void dxl_startup_ended(void) {
    if (ended) SetEvent(ended);
}

/* The console's code page as the program found it, where it made it
 * UTF-8, else 0: its UTF-8 is the program's while it has the console, and
 * the console, a cmd.exe's it was started from, has its own again as the
 * program ends or lets go of it. */
static UINT console_cp;
/* The console is the parent's, attached to for what the start says. */
static int borrowed;

static void console_cp_back(void) {
    if (console_cp) SetConsoleOutputCP(console_cp);
    console_cp = 0;
}

void dxl_startup_detach(void) {
    if (!borrowed) return;
    borrowed = 0;
    fflush(stdout);
    fflush(stderr);
    console_cp_back();
    /* What is written from here goes nowhere, as a GUI program's does. */
    if (!freopen("NUL", "w", stdout)) {}
    if (!freopen("NUL", "w", stderr)) {}
    FreeConsole();
}

/* Before main: a GUI program started from a console writes to it, in
 * UTF-8; the error mode for this program and those it starts. */
__attribute__((constructor)) static void startup_early(void) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((!out || out == INVALID_HANDLE_VALUE) && AttachConsole(ATTACH_PARENT_PROCESS)) {
        borrowed = 1;
        if (!freopen("CONOUT$", "w", stdout)) {}
        if (!freopen("CONOUT$", "w", stderr)) {}
    }
    UINT cp = GetConsoleOutputCP();
    if (cp && cp != CP_UTF8 && SetConsoleOutputCP(CP_UTF8)) {
        console_cp = cp;
        atexit(console_cp_back);
    }
}
