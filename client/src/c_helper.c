#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"
#include "errors.h"

static volatile sig_atomic_t CONNECTION_STATE = IDLE;

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

volatile sig_atomic_t TIMED_OUT = 0;

static void alarm_handler(int signum)
{
    (void)signum;
    TIMED_OUT = 1;
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
    return SUCCESS;
}

int perform_handshake()
{
    /* prevent race condition */
    // block the SIGUSR2 input temporarily
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGALRM);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    TIMED_OUT        = 0;
    CONNECTION_STATE = SYN_SENT;
    alarm(1);

    if (kill(SERVER_PID, SIGUSR1) == -1) {
        perror("Failed to signal server");
        alarm(0);
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        return SIG_FAIL;
    }

    // use the original where SIGUSR2 is allowed, wait
    while (!TIMED_OUT && CONNECTION_STATE != CONNECTED) {
        // can be woken up by any signal
        sigsuspend(&oldmask);
    }

    // restore old mask
    alarm(0);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    if (TIMED_OUT) {
        fprintf(stderr, "%s", errs[TIMEDOUT]);
        fflush(stderr);
        retval = TIMEDOUT;
        return TIMED_OUT;
    }

    printf("Connected to %d.\n", SERVER_PID);
    fflush(stdout);
    return SUCCESS;
}