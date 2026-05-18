#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"
#include "errors.h"
#include "fifo.h"

volatile sig_atomic_t CONNECTION_STATE = IDLE;
int                   C_FDS[2];

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
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return SIG_FAIL;
    }

    /* ALARM */
    sa.sa_handler = alarm_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return SIG_FAIL;
    }

    /* SIGINT */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return SIG_FAIL;
    }

    /* SIGTERM */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return SIG_FAIL;
    }

    /* SIGQUIT */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return SIG_FAIL;
    }

    /* SIGHUP */
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
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

void perform_goodbye();

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
    int r_temp = set_name(name_temp, S2C, getpid());
    r_temp     = open(name_temp, O_RDONLY);
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