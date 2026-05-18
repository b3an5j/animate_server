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
#include "s_helper.h"

// pipes
int              HANDSHAKE_PIPE[2]; // connects main loop with signal handler
int              RPC_PIPE[2];       // connects main loop with worker threads
volatile PollFds POLLFDS;

int server_setup_pipes()
{
    /* Open self pipes */
    if (pipe2(HANDSHAKE_PIPE, O_NONBLOCK) != 0 || pipe2(RPC_PIPE, 0) != 0) {
        fprintf(stderr, errs[PIPE_FAIL], 0);
        fflush(stderr);
        retval = PIPE_FAIL;
        return PIPE_FAIL;
    }

    /* Initialise DArray */
    S_POLL_FDS = malloc(sizeof(*S_POLL_FDS) * POLLFD_INIT_N);
    if (S_POLL_FDS == NULL) {
        fprintf(stderr, errs[POLLFD_FAIL]);
        fflush(stderr);
        retval = POLLFD_FAIL;
        return POLLFD_FAIL;
    }
    S_POLL_FD_CAPACITY = POLLFD_INIT_N;

    HSK_POLLFD.fd      = HSK_R;
    HSK_POLLFD.events  = POLLIN;
    HSK_POLLFD.revents = 0;

    RPC_POLLFD.fd      = RPC_R;
    RPC_POLLFD.events  = POLLIN;
    RPC_POLLFD.revents = 0;

    S_POLL_FD_COUNT = 2;
    return SUCCESS;
}

void server_destroy_pipes()
{
    if (RUNNING)
        return;

    close(HSK_R);
    close(HSK_W);
    close(RPC_R);
    close(RPC_W);
    free(S_POLL_FDS);
}

int server_grow_pollfd()
{
    size_t new_capacity = S_POLL_FD_CAPACITY * 2;

    struct pollfd *temp =
        reallocarray(S_POLL_FDS, new_capacity, sizeof(*S_POLL_FDS));
    if (temp == NULL) {
        fprintf(stderr, errs[GROW_FAIL]);
        fflush(stderr);
        return GROW_FAIL;
    }

    S_POLL_FDS         = temp;
    S_POLL_FD_CAPACITY = new_capacity;
    return SUCCESS;
}

int server_insert_pollfd(int fd)
{
    // Check if grow needed
    if (S_POLL_FD_CAPACITY == S_POLL_FD_COUNT) {
        if (server_grow_pollfd() != SUCCESS) {
            return GROW_FAIL;
        }
    }

    struct pollfd *cell = &S_POLL_FDS[S_POLL_FD_COUNT];
    cell->fd            = fd;
    cell->events        = POLLIN;
    cell->revents       = 0;
    S_POLL_FD_COUNT++;
    return SUCCESS;
}

void server_remove_pollfd(int fd)
{
    if (!S_POLL_FDS || S_POLL_FD_COUNT == 0)
        return;

    // look for fd, better parallel
    for (size_t i = 0; i < S_POLL_FD_COUNT; ++i) {
        if (S_POLL_FDS[i].fd != fd)
            continue;

        // insert last to empty cell
        S_POLL_FDS[i] = S_POLL_FDS[S_POLL_FD_COUNT - 1];
        S_POLL_FD_COUNT--;
        return;
    }
}

// signal handlers
static void sigusr1_handler(int signum, siginfo_t *info, void *context)
{
    (void)signum;
    (void)context;

    pid_t client_pid = info.si_pid;
    write(HSK_W, &client_pid, sizeof(pid_t));
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