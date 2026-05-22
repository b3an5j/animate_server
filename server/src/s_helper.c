#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "client_registry.h"
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
        fprintf(stderr, errs[PIPE_FAIL], 0);
        fflush(stderr);
        retval = PIPE_FAIL;
        return PIPE_FAIL;
    }

    /* Initialise DArray */
    S_POLL_SLOTS = malloc(sizeof(*S_POLL_SLOTS) * POLLFD_INIT_N);
    if (S_POLL_SLOTS == NULL) {
        fprintf(stderr, errs[POLLFD_FAIL]);
        fflush(stderr);
        retval = POLLFD_FAIL;
        return POLLFD_FAIL;
    }
    S_POLL_FD_CAPACITY = POLLFD_INIT_N;

    HSK_POLLFD.fd      = HSK_R;
    HSK_POLLFD.events  = POLLIN;
    HSK_POLLFD.revents = 0;
    HSK_SLOT.client    = NULL;

    RPC_POLLFD.fd      = RPC_R;
    RPC_POLLFD.events  = POLLIN;
    RPC_POLLFD.revents = 0;
    RPC_SLOT.client    = NULL;

    S_POLL_FD_COUNT = 2;
    return SUCCESS;
}

void server_destroy_selfpipes()
{
    if (RUNNING)
        return;

    close(HSK_R);
    close(HSK_W);
    close(RPC_R);
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
}

int server_grow_pollslots()
{
    size_t new_capacity = S_POLL_FD_CAPACITY * 2;

    PollSlots *temp =
        reallocarray(S_POLL_SLOTS, new_capacity, sizeof(*S_POLL_SLOTS));
    if (temp == NULL) {
        fprintf(stderr, errs[GROW_FAIL]);
        fflush(stderr);
        retval = GROW_FAIL;
        return GROW_FAIL;
    }

    S_POLL_SLOTS       = temp;
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

    PollSlot *slot    = &S_POLL_SLOTS[S_POLL_FD_COUNT];
    slot->pfd.fd      = fd;
    slot->pfd.events  = POLLIN;
    slot->pfd.revents = 0;
    slot->client      = client;
    S_POLL_FD_COUNT++;
    return SUCCESS;
}

void server_remove_pollslots(int fd)
{
    if (!S_POLL_SLOTS || S_POLL_FD_COUNT == 0)
        return;

    // look for fd, better parallel
    for (size_t i = 0; i < S_POLL_FD_COUNT; ++i) {
        if (S_POLL_SLOTS[i].fd != fd)
            continue;

        // insert last to empty cell
        S_POLL_SLOTS[i] = S_POLL_SLOTS[S_POLL_FD_COUNT - 1];
        S_POLL_FD_COUNT--;
        return;
    }
}

void server_destroy_pollslots()
{
    free(S_POLL_SLOTS);
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
    struct sigaction sa;

    /* SIGUSR1 */
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        fprintf(stderr, errs[SIG_FAIL]);
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
        fprintf(stderr, errs[SIG_FAIL]);
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
        fprintf(stderr, errs[SIG_FAIL]);
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
        fprintf(stderr, errs[SIG_FAIL]);
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
        fprintf(stderr, errs[SIG_FAIL]);
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
        fprintf(stderr, errs[SIG_FAIL]);
        fflush(stderr);
        retval = SIG_FAIL;
        return SIG_FAIL;
    }

    return SUCCESS;
}

// user.txt
FILE *USERTXT = NULL;
int   usertxt_check()
{
    USERTXT = fopen("user.txt", "a+");
    if (USERTXT == NULL) {
        perror("Failed to open/create user.txt");
        fflush(stderr);
        retval = USERTXT_FAIL;
        return USERTXT_FAIL;
    }
    return SUCCESS;
}

void usertxt_get_balance(const char *username, uint16_t len, int s2c_fd)
{
    rewind(USERTXT);

    char buffer[MAX_UTXT_LINEBUF_LEN];
    char balance[BALANCE_SIZE_B] = {0};
    while (fgets(buffer, sizeof(buffer), USERTXT) != NULL) {
        sanitise_whitespace(buffer);

        size_t buflen = strlen(buffer);
        if (buflen < len) {
            continue;
        }

        // skip same substring
        if (!isblank((unsigned char)buffer[len])) {
            continue;
        }

        if (strncmp(username, buffer, len) != 0) {
            continue;
        }

        char *head = buffer + len;
        char *end;
        long  bal = strtol(head, &end, 10);
        if (head == end || bal <= 0) {
            memset(balance, 'F', BALANCE_SIZE_B);
            write(s2c_fd, balance, sizeof(balance));
            return;
        }

        snprintf(balance, sizeof(balance), "%ld", bal);
        write(s2c_fd, balance, sizeof(balance));
        return;
    }
    memset(balance, 'N', sizeof(balance));
    write(s2c_fd, balance, sizeof(balance));
}

void usertxt_close()
{
    if (USERTXT == NULL) {
        return;
    }
    fclose(USERTXT);
}