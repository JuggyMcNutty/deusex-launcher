/* dxl-cli's server listing (src/cli_main.c, built in here, its main
 * renamed, for its static clean(), its look's calls and list_servers()):
 * others' text -- a server's, a master's refusal -- printed with no control
 * character a terminal acts on, C1's CSI and OSC among them (bugs.md
 * LCORE.19), and the launcher's log the same; each server printed as it
 * answers, one with no name by its address (LCORE.20). */
#define main dxl_cli_main
#include "cli_main.c"
#undef main

#include "test.h"
#include "core/log.h"
#include "platform/os.h"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

/* Each control a '?', C0's, DEL and C1's, the rest as it is: UTF-8's other
 * characters whole, a cut at a whole one. */
static void test_no_controls(void) {
    char out[64];
    dxl_text_no_controls("a\x1b[2Jb\x07\x7f", out, sizeof out);
    CHECK_STR(out, "a?[2Jb??");
    dxl_text_no_controls("CSI\xc2\x9b" "2J OSC\xc2\x9d" "0;t", out, sizeof out);
    CHECK_STR(out, "CSI?2J OSC?0;t");
    dxl_text_no_controls("caf\xc3\xa9 \xc2\xa0nbsp \xe2\x82\xac", out, sizeof out);   /* é, U+00A0, € kept */
    CHECK_STR(out, "caf\xc3\xa9 \xc2\xa0nbsp \xe2\x82\xac");
    dxl_text_no_controls("raw\x9b" "2J\xe9", out, sizeof out);                      /* no UTF-8: 0x9b alone */
    CHECK_STR(out, "raw?2J\xe9");
    dxl_text_no_controls("ab\xe2\x82\xac", out, 5);                                  /* € does not fit */
    CHECK_STR(out, "ab");
    char same[] = "in\x1bplace\xc2\x9b";
    dxl_text_no_controls(same, same, sizeof same);
    CHECK_STR(same, "in?place?");
}

/* A server's text as dxl_server_take_status keeps it, then as the CLI
 * prints it. */
static void test_server_text(void) {
    dxl_server sv;
    dxl_server_init(&sv, "203.0.113.9", 7791);
    /* hostname: Latin-1 0x9b, CSI, then "2J", clearing the screen;
     * mapname: UTF-8 U+009D, OSC, setting the window's title to its BEL;
     * gametype: a C0 ESC. */
    dxl_server_take_status(&sv, "\\hostname\\Evil\x9b" "2J"
                                "\\mapname\\DXMP\xc2\x9d" "0;pwned\x07"
                                "\\gametype\\DM\x1b" "[31m"
                                "\\numplayers\\1\\maxplayers\\8\\gamever\\1100");
    char shown[256];
    clean(sv.name, shown, sizeof shown);
    CHECK(strstr(shown, "\xc2\x9b") == NULL);
    CHECK_STR(shown, "Evil?2J");
    clean(sv.map, shown, sizeof shown);
    CHECK(strstr(shown, "\xc2\x9d") == NULL);
    CHECK_STR(shown, "DXMP?0;pwned?");
    clean(sv.game_type, shown, sizeof shown);
    CHECK_STR(shown, "DM?[31m");
    dxl_server_free(&sv);
}

/* The launcher's log, the window's server search's refusal among it: no
 * control of it in launcher.log nor on stderr. */
static void test_log(void) {
    char path[512];
    snprintf(path, sizeof path, "%s/dxl-cli-log-%ld.txt", dxl_test_tmp(), (long)dxl_os_pid());
    dxl_log_set_echo(0);
    dxl_log_open_new(path);
    dxl_log("multiplayer: 0 server(s), 0 answered; %s", "master: \x1b]0;pwned\x07 \xc2\x9b" "2J");
    dxl_log_close();
    dxl_log_set_echo(1);
    char *said = slurp(path, NULL);
    CHECK(said != NULL && strstr(said, "master: ?]0;pwned? ?2J") != NULL);
    free(said);
    remove(path);
}

#ifndef _WIN32
/* stdout into a file of the test's while fn runs, what it printed then
 * into out. */
static void printed(void (*fn)(void), char *out, size_t size) {
    char path[512];
    snprintf(path, sizeof path, "%s/dxl-cli-out-%ld.txt", dxl_test_tmp(), (long)dxl_os_pid());
    fflush(stdout);
    int kept = dup(1);
    FILE *f = fopen(path, "w+b");
    CHECK(f != NULL);
    if (f) dup2(fileno(f), 1);
    fn();
    fflush(stdout);
    dup2(kept, 1);
    close(kept);
    if (f) fclose(f);
    char *said = slurp(path, NULL);
    snprintf(out, size, "%s", said ? said : "");
    free(said);
    remove(path);
}

static servers look;

/* The look's calls as dxl_browse makes them: a server listed, then its
 * answer, with no name. */
static void one_answers(void) {
    memset(&look, 0, sizeof look);
    listed(&look, "203.0.113.7", 7791, 0);
    listed(&look, "203.0.113.8", 7791, 0);
    answered(&look, "203.0.113.7", 7791, 42, "\\mapname\\DXMP_Smuggler\\gametype\\DeathMatchGame"
                                              "\\numplayers\\2\\maxplayers\\8\\gamever\\1100\\hostport\\7790");
}

/* A server's lines as it answers, before the look ends: titled by its
 * address where it gives no name, as Multiplayer titles it. */
static void test_printed_as_it_answers(void) {
    char out[1024];
    printed(one_answers, out, sizeof out);
    CHECK_STR(out, "203.0.113.7\n    DXMP_Smuggler, DeathMatchGame, 2/8, 42 ms, 203.0.113.7:7790\n");
    for (int i = 0; i < look.count; i++) dxl_server_free(&look.s[i]);
}

static int refusing_fd = -1;

/* A master that greets with a refusal whose text sets the terminal's title,
 * then closes. */
static void *refusing_master(void *p) {
    (void)p;
    int c = accept(refusing_fd, NULL, NULL);
    if (c < 0) return NULL;
    const char *no = "\\error\\\x1b]0;pwned\x07\xc2\x9b" "2J\\final\\";
    send(c, no, strlen(no), MSG_NOSIGNAL);
    usleep(100 * 1000);
    close(c);
    return NULL;
}

/* --servers=<that master>: its refusal on stderr, no control of it there. */
static void test_master_refusal(void) {
    refusing_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t salen = sizeof sa;
    int listening = refusing_fd >= 0 && bind(refusing_fd, (struct sockaddr *)&sa, sizeof sa) == 0 &&
                    listen(refusing_fd, 1) == 0 && getsockname(refusing_fd, (struct sockaddr *)&sa, &salen) == 0;
    CHECK(listening);   /* a socket on 127.0.0.1 */
    if (!listening) return;
    pthread_t t;
    pthread_create(&t, NULL, refusing_master, NULL);
    char at[64];
    snprintf(at, sizeof at, "127.0.0.1:%d", ntohs(sa.sin_port));

    /* stderr into a file of the test's while the CLI looks. */
    char path[512];
    snprintf(path, sizeof path, "%s/dxl-cli-clean-%ld.txt", dxl_test_tmp(), (long)getpid());
    fflush(stderr);
    int kept = dup(2);
    FILE *err_file = fopen(path, "w+b");
    CHECK(err_file != NULL);
    if (err_file) dup2(fileno(err_file), 2);
    int rc = list_servers(at);
    fflush(stderr);
    dup2(kept, 2);
    close(kept);
    if (err_file) fclose(err_file);

    char *said = slurp(path, NULL);
    CHECK_INT(rc, 1);
    CHECK(said != NULL && strstr(said, "pwned") != NULL);   /* the refusal is said */
    CHECK(said != NULL && strchr(said, '\x1b') == NULL);
    CHECK(said != NULL && strchr(said, '\x07') == NULL);
    CHECK(said != NULL && strstr(said, "\xc2\x9b") == NULL);
    free(said);
    remove(path);
    pthread_join(t, NULL);
    close(refusing_fd);
}
#endif

TEST_MAIN_BEGIN
    RUN(test_no_controls);
    RUN(test_server_text);
    RUN(test_log);
#ifndef _WIN32
    RUN(test_printed_as_it_answers);
    RUN(test_master_refusal);
#endif
TEST_MAIN_END
