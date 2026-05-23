#define _GNU_SOURCE

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_registry.h"
#include "dbg.h"
#include "errors.h"
#include "fifo.h"
#include "s_helper.h"

// pipes
int       HANDSHAKE_PIPE[2]; // connects main loop with signal handler
int       RPC_PIPE[2];       // connects main loop with worker threads
PollSlots POLLSLOTS;

int server_setup_pipes()
{
    /* Open self pipes */
    if (pipe2(HANDSHAKE_PIPE, O_NONBLOCK) != 0 ||
        pipe2(RPC_PIPE, O_NONBLOCK) != 0) {
        fprintf(stderr, "%s %d.\n", errs[PIPE_FAIL], 0);
        fflush(stderr);
        retval = PIPE_FAIL;
        return PIPE_FAIL;
    }

    /* Initialise DArray */
    struct pollfd *pf = malloc(sizeof(*pf) * POLLFD_INIT_N);
    ActiveClient **cl = malloc(sizeof(*cl) * POLLFD_INIT_N);

    if (!pf || !cl) {
        free(pf);
        free(cl);
        fputs(errs[POLLFD_FAIL], stderr);
        retval = POLLFD_FAIL;
        return POLLFD_FAIL;
    }

    S_POLL_PFDS        = pf;
    S_POLL_CLIENT      = cl;
    S_POLL_FD_CAPACITY = POLLFD_INIT_N;

    HSK_POLLFD.fd      = HSK_R;
    HSK_POLLFD.events  = POLLIN;
    HSK_POLLFD.revents = 0;
    S_POLL_CLIENT[0]   = NULL;

    RPC_POLLFD.fd      = RPC_R;
    RPC_POLLFD.events  = POLLIN;
    RPC_POLLFD.revents = 0;
    S_POLL_CLIENT[1]   = NULL;

    S_POLL_FD_COUNT = 2;
    return SUCCESS;
}

void server_destroy_selfpipes()
{
    if (RUNNING)
        return;
    if (HSK_R > 2)
        close(HSK_R);
    if (HSK_W > 2)
        close(HSK_W);
    if (RPC_R > 2)
        close(RPC_R);
    if (RPC_W > 2)
        close(RPC_W);
}

void destroy_client_pipes(pid_t client_pid, int c2s_fd, int s2c_fd)
{
    close(c2s_fd);
    close(s2c_fd);

    char temp[FIFO_MAX_NAME];
    set_name(temp, C2S, client_pid);
    unlink(temp);
    set_name(temp, S2C, client_pid);
    unlink(temp);

    debug_log(
        "Closing c2s_fd=%d s2c_fd=%d for pid=%d", c2s_fd, s2c_fd, client_pid);
}

int server_grow_pollslots()
{
    size_t new_capacity = S_POLL_FD_CAPACITY * 2;

    struct pollfd *pf =
        reallocarray(S_POLL_PFDS, new_capacity, sizeof(*S_POLL_PFDS));
    if (pf == NULL) {
        fputs(errs[GROW_FAIL], stderr);
        fflush(stderr);
        retval = GROW_FAIL;
        return GROW_FAIL;
    }
    S_POLL_PFDS = pf;

    ActiveClient **cl =
        reallocarray(S_POLL_CLIENT, new_capacity, sizeof(*S_POLL_CLIENT));
    if (cl == NULL) {
        fputs(errs[GROW_FAIL], stderr);
        fflush(stderr);
        retval = GROW_FAIL;
        return GROW_FAIL;
    }
    S_POLL_CLIENT = cl;

    S_POLL_FD_CAPACITY = new_capacity;
    return SUCCESS;
}

int server_insert_pollslots(int fd, ActiveClient *client)
{
    // Check if grow needed
    if (S_POLL_FD_CAPACITY == S_POLL_FD_COUNT) {
        if (server_grow_pollslots() != SUCCESS) {
            return GROW_FAIL;
        }
    }

    size_t i               = S_POLL_FD_COUNT++;
    S_POLL_PFDS[i].fd      = fd;
    S_POLL_PFDS[i].events  = POLLIN;
    S_POLL_PFDS[i].revents = 0;
    S_POLL_CLIENT[i]       = client;
    return SUCCESS;
}

void server_remove_pollslots(int fd)
{
    // look for fd, better parallel
    for (size_t i = 0; i < S_POLL_FD_COUNT; ++i) {
        if (S_POLL_PFDS[i].fd != fd) {
            continue;
        }

        size_t last      = S_POLL_FD_COUNT - 1;
        S_POLL_PFDS[i]   = S_POLL_PFDS[last];
        S_POLL_CLIENT[i] = S_POLL_CLIENT[last];
        S_POLL_FD_COUNT--;
        return;
    }
}

void server_destroy_pollslots()
{
    if (RUNNING) {
        return;
    }

    free(S_POLL_PFDS);
    free(S_POLL_CLIENT);
}

// signal handlers
static void sigusr1_handler(int signum, siginfo_t *info, void *context)
{
    (void)signum;
    (void)context;

    pid_t client_pid = info->si_pid;
    write(HSK_W, &client_pid, sizeof(pid_t));
}

static void shutdown_handler(int signum)
{
    (void)signum;
    RUNNING = 0;
}

int server_setup_signals()
{
    struct sigaction sa;

    /* SIGUSR1 */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGINT */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGTERM */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGQUIT */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGQUIT, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGHUP */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = shutdown_handler;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    /* SIGPIPE */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags   = 0;
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        fputs(errs[SIG_FAIL], stderr);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    return SUCCESS;
}