#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <animate/animate.h>

#include "client_registry.h"
#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

static volatile sig_atomic_t RUNNING = 0;

int main(int argc, char **argv, char **envp)
{
    /* Start procedure */
    // block, no interruption
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGQUIT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    if (get_threadpool_size(argc, argv) != 0) {
        return retval;
    }
    if (server_setup_pipes() != 0 || server_setup_signals() != 0) {
        server_destroy_pipes();
        return retval;
    }
    if (threadpool_init() != 0) {
        return retval;
    }
    task_init();

    pid_t SERVER_PID = getpid();
    printf("Server PID: %d\n", SERVER_PID);
    fflush(stdout);
    RUNNING = 1;

    // return the mask
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    /* Waiting handshake or RPC */
    while (RUNNING) {
        int ready = poll(S_POLL_FDS, S_POLL_FD_COUNT, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("Poll failure");
            break;
        }

        /* Handle Handshake requests */
        if (HSK_POLLFD.revents & POLLIN) {
            pid_t   client_pid;
            ssize_t n_read;
            while ((n_read = read(HSK_R, &client_pid, sizeof(pid_t))) ==
                   sizeof(pid_t)) {
                task_enqueue(HANDSHAKE, client_pid, 0, 0);
            }

            // sanity check
            if (n_read == 0) {
                fprintf(stderr, errs[PIPE_CLOSED], "Handshake");
                fflush(stderr);
                RUNNING = 0;
                break;
            }
            else if (n_read > 0 && n_read < sizeof(pid_t)) {
                fprintf(stderr, errs[PIPE_PARTIAL], "Handshake");
                fflush(stderr);
                RUNNING = 0;
                break;
            }
            else if (n_read < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
                fprintf(stderr, errs[PIPE_READ_FAIL], "Handshake");
                fflush(stderr);
                RUNNING = 0;
                break;
            }
        }

        /* Handle RPC inter thread communication */
        if (RPC_POLLFD.revents & POLLIN) {
            // take the fds opened for client
        }

        /* Handle client RPC requests */
    }

    /* Teardown */
    threadpool_destroy();
    creg_destroy();
    server_destroy_pipes();
    task_destroy();
    return 0;
}
