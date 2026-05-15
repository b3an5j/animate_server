#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "errors.h"
#include "pending_client.h"
#include "s_helper.h"

/* SIGNALING */
// pipes
int HANDSHAKE_PIPE[2]; // connects main loop with signal handler
int RPC_PIPE[2];       // connects main loop with worker threads

int server_setup_pipes()
{
    if (pipe2(HANDSHAKE_PIPE, O_NONBLOCK) != 0 || pipe2(RPC_PIPE, NULL) != 0) {
        fprintf(stderr, errs[PIPE_FAIL]);
        retval = PIPE_FAIL;
        return 1;
    }
    return 0;
}

void server_destroy_pipes()
{
    if (RUNNING)
        return;

    close(HSK_R);
    close(HSK_W);
    close(RPC_R);
    close(RPC_W);
}

// signal handlers
static void sigusr1_handler(int signum, siginfo_t *info, void *context)
{
    (void)signum;
    (void)context;

    pid_t client_pid = info.si_pid;
    write(HSK_W, &client_pid, sizeof(client_pid));
}

static void shutdown_handler(int signum)
{
    (void)signum;
    RUNNING = 0;
}

int server_setup_signals()
{
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);

    /* SIGUSR1 */
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return 1;
    }

    /* SIGINT */
    sa.sa_flags     = 0;
    sa.sa_sigaction = shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return 1;
    }

    /* SIGTERM */
    sa.sa_flags     = 0;
    sa.sa_sigaction = shutdown_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return 1;
    }

    /* SIGQUIT */
    sa.sa_flags     = 0;
    sa.sa_sigaction = shutdown_handler;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return 1;
    }

    /* SIGHUP */
    sa.sa_flags     = 0;
    sa.sa_sigaction = shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        return 1;
    }

    return 0;
}

/* FIFO */
void set_name(const char *buf, FifoType type, pid_t pid)
{
    switch (type) {
    case C2S:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_C2S_%d", pid);
        break;
    case S2C:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_S2C_%d", pid);
        break;
    default:
        break;
    }
}