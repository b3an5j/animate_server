#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <animate/animate.h>

#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

static volatile sig_atomic_t RUNNING = 1;

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

    // return the mask
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    /* Waiting handshake or RPC */
    while (RUNNING) {
        // poll
    }

    /* Clean up */
    task_destroy();
    threadpool_destroy();
    server_destroy_pipes();
    return 0;
}
