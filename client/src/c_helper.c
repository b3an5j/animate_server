#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"
#include "dbg.h"
#include "errors.h"
#include "fifo.h"

/* Handshake */
int C_FDS[2];

int set_serverpid(int argc, char *argv[])
{
    if (argc == 1) {
        fprintf(stderr, "%s%s", errs[TOO_FEW], c_usage);
        fflush(stderr);
        retval = TOO_FEW;
        return TOO_FEW;
    }
    if (argc > 2) {
        fprintf(stderr, "%s%s", errs[TOO_MANY], c_usage);
        fflush(stderr);
        retval = TOO_MANY;
        return TOO_MANY;
    }

    SERVER_PID = atoi(argv[1]);
    if (SERVER_PID <= 0) {
        fprintf(stderr, "%s%s", errs[INV_ARG], c_usage);
        fflush(stderr);
        retval = INV_ARG;
        return INV_ARG;
    }
    return SUCCESS;
}

static void sigusr2_handler(int signum)
{
    (void)signum;
    CONNECTION_STATE = CONNECTED;
}

static void alarm_handler(int signum)
{
    (void)signum;
    CONNECTION_STATE = TIMED_OUT;
}

static void shutdown_handler(int signum)
{
    (void)signum;
    CONNECTION_STATE = DISCONNECTED;
}

int client_setup_signals()
{
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);

    /* SIGUSR2 */
    sa.sa_handler = sigusr2_handler;
    if (sigaction(SIGUSR2, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* ALARM */
    sa.sa_handler = alarm_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGINT */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGTERM */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGQUIT */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGHUP */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    return SUCCESS;
}

static void reset_alarm_handler()
{
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = SIG_DFL;

    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        perror("Failed to reset alarm handler");
        fflush(stderr);
    }
}

int perform_handshake()
{
    // block, no interruption
    sigset_t mask, oldmask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    CONNECTION_STATE = SYN_SENT;
    alarm(1);

    // send syn
    if (kill(SERVER_PID, SIGUSR1) == -1) {
        perror("Failed to signal server");
        fflush(stderr);

        alarm(0);
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return SIG_FAIL;
    }

    // receive signals
    while (CONNECTION_STATE == SYN_SENT) {
        // can be woken up by any signal
        sigsuspend(&oldmask);
    }

    // restore old mask
    alarm(0);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    if (CONNECTION_STATE == TIMED_OUT) {
        goto hsk_to;
    }

    // open the server fifo
    char name_temp[FIFO_MAX_NAME];

    // read end
    alarm(1);
    set_name(name_temp, S2C, getpid());
    int r_temp = open(name_temp, O_RDONLY);
    if (r_temp == -1) {
        goto hsk_fail;
    }
    S_READ = r_temp;

    // write end
    set_name(name_temp, C2S, getpid());
    int w_temp = open(name_temp, O_WRONLY);
    if (w_temp == -1) {
        close(S_READ);
        goto hsk_fail;
    }
    C_WRITE = w_temp;

    alarm(0);
    reset_alarm_handler();
    printf("Connected to %d.\n", SERVER_PID);
    fflush(stdout);
    return SUCCESS;

hsk_to:
    fprintf(stderr, "%s", errs[TIMEDOUT]);
    fflush(stderr);
    retval = TIMEDOUT;
    return TIMEDOUT;

hsk_fail:
    CONNECTION_STATE = DISCONNECTED;

    fprintf(stderr, errs[PIPE_FAIL], getpid());
    fflush(stderr);
    retval = PIPE_FAIL;
    return PIPE_FAIL;
}

/* RPC request */
static char INPUT[MAX_RPC_BUF_LEN];

void authorise()
{
    char *ret;

    while (CONNECTION_STATE == CONNECTED && !LOGGED_IN &&
           (ret = fgets(INPUT, MAX_RPC_BUF_LEN, stdin))) {

        // strip newline
        size_t len = strlen(INPUT);
        if (len > 0 && INPUT[len - 1] == '\n')
            INPUT[len - 1] = '\0';

        // collapse whitespace
        sanitise_whitespace(INPUT);
        if (!*INPUT)
            continue;

        // attempt
        if (strncmp(INPUT, "Login ", 6) != 0) {
            printf("Not logged in\n");
            fflush(stdout);
            continue;
        }

        // add newline back
        char outbuf[MAX_RPC_BUF_LEN + 1];
        snprintf(outbuf, sizeof(outbuf), "%s\n", INPUT);

        // send exactly one text command
        write_block_pipe(C_WRITE, outbuf, strlen(outbuf));

        // read server reply
        char   reply[MAX_RPC_BUF_LEN];
        size_t reply_len;

        ErrType r =
            read_until_delim(S_READ, reply, MAX_RPC_BUF_LEN, '\n', &reply_len);
        if (r != SUCCESS) {
            printf("Server disconnected\n");
            return;
        }

        reply[reply_len] = '\0';

        // handle reply
        if (strncmp(reply, "Reject", 6) == 0) {
            // print rejection
            printf("%s\n", reply);
            fflush(stdout);
            CONNECTION_STATE = DISCONNECTED;
            return;
        }
        long        balance  = strtol(reply, NULL, 10);
        const char *username = INPUT + 6;

        printf("Welcome %s. Your balance is %ld\n", username, balance);
        fflush(stdout);

        LOGGED_IN = 1;
        return;
    }
}

int get_user_input(char *out)
{
    // Read a line from stdin
    if (!fgets(out, MAX_RPC_BUF_LEN, stdin)) {
        return -1;
    }

    sanitise_whitespace(out);
    if (out[0] == '\0') {
        return 1; // continue
    }

    debug_log("After sanitise: %s\n", out);

    // Detect Disconnect
    if (strcmp(out, "Disconnect") == 0) {
        const char *msg = "Disconnect\n";
        write_block_pipe(C_WRITE, msg, strlen(msg));
        return 0; // break loop
    }

    // Normal RPC
    char sendbuf[MAX_RPC_BUF_LEN];
    snprintf(sendbuf, sizeof(sendbuf), "%s\n", out);

    write_block_pipe(C_WRITE, sendbuf, strlen(sendbuf));

    return 1; // continue loop
}
