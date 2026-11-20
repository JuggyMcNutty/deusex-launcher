/* A start of either program on Windows (platform/startup.h). Ctrl+C,
 * Ctrl+Break and a closed console end it from outside, through the
 * console's handler, on a thread of its own, which for a closed console
 * waits for the run's end, Windows ending the process as it returns. A
 * sign-out's and a shutdown's console events never come to it: Windows
 * gives them to no program that loads user32 or gdi32, as both programs do
 * (bugs.md LPORT.9). A hidden top-level window of the launcher's, on a
 * thread of its own, has the session's end instead (a message-only window
 * is not told): WM_ENDSESSION is a stop, waited for as a closed console's
 * is, within the 5 s Windows gives a program with no visible window before
 * it ends it (bugs.md D.99). Windows' error boxes are kept
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
#include <string.h>

static atomic_int *stop_flag;
static void (*wake_up)(void);
static HANDLE ended;   /* set once the run is done: a closing console waits for it */
static atomic_int session_ending;
static HANDLE session_thread;

/* How long the session's end waits for the run's: under the 5 s after
 * which Windows ends a program with no visible window that has not
 * answered WM_ENDSESSION. */
#define SESSION_WAIT_MS 4500

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

/* The session's end, told to every top-level window: asked first, and
 * never refused -- the player's sign-out, shutdown or restart goes on --
 * then, once it is sure (wParam TRUE), the launcher's stop: Quit to the
 * game, its end recorded, all before this returns, as Windows may end the
 * process any time after. One that was called off is nothing. */
static LRESULT CALLBACK on_session(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_QUERYENDSESSION) return TRUE;
    if (msg == WM_ENDSESSION) {
        if (wp) {
            atomic_store(&session_ending, 1);
            if (stop_flag) *stop_flag = 1;
            if (wake_up) wake_up();
            if (ended) WaitForSingleObject(ended, SESSION_WAIT_MS);
        }
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* The hidden window's thread, its messages pumped for the program's life,
 * whatever the main thread waits on. */
static DWORD WINAPI session_main(void *arg) {
    (void)arg;
    HINSTANCE inst = GetModuleHandleW(NULL);
    WNDCLASSW c;
    memset(&c, 0, sizeof c);
    c.lpfnWndProc = on_session;
    c.hInstance = inst;
    c.lpszClassName = DXL_SESSION_CLASS;
    if (!RegisterClassW(&c)) return 1;
    /* No WS_VISIBLE, no owner: top-level, never shown. */
    HWND w = CreateWindowExW(0, DXL_SESSION_CLASS, L"DXLauncher", WS_OVERLAPPED, 0, 0, 0, 0, NULL, NULL, inst, NULL);
    if (!w) return 1;
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) DispatchMessageW(&m);
    return 0;
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
    if (!ended) ended = CreateEventW(NULL, TRUE, FALSE, NULL);
    SetConsoleCtrlHandler(on_console, TRUE);
    if (!session_thread) session_thread = CreateThread(NULL, 0, session_main, NULL, 0, NULL);
}

void dxl_startup_ended(void) {
    if (ended) SetEvent(ended);
}

int dxl_startup_session_ended(void) { return atomic_load(&session_ending); }

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
