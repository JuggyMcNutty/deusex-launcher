/* A program's end as each platform reports it (platform/os.h): a crash and
 * a kill, their codes, the status they leave and their names -- POSIX's
 * signals, Windows' exception codes. */
#ifndef DXL_TEST_ENDS_H
#define DXL_TEST_ENDS_H

#ifdef _WIN32
#define END_CRASH         ((int)0xC0000005u)
#define END_CRASH_STATUS  END_CRASH
#define END_CRASH_NAME    "an access violation (0xC0000005)"
#define END_KILL          ((int)0x40010004u)
#define END_KILL_STATUS   END_KILL
#define END_KILL_NAME     "the launcher (0x40010004)"
#define END_UNNAMED       0x40
#define END_UNNAMED_NAME  "exception 0x00000040"
#else
#include <signal.h>
#define END_CRASH         SIGSEGV
#define END_CRASH_STATUS  (128 + SIGSEGV)
#define END_CRASH_NAME    "SIGSEGV"
#define END_KILL          SIGKILL
#define END_KILL_STATUS   (128 + SIGKILL)
#define END_KILL_NAME     "SIGKILL"
#define END_UNNAMED       64
#define END_UNNAMED_NAME  "signal 64"
#endif

#endif
