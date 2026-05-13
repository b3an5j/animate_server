#define _POSIX_C_SOURCE 200809L

#include "s_helper.h"
#include "errors.h"
#include "queues.h"
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int get_threadpool_size(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "%s%s", errs[TOO_FEW], s_usage);
        retval = TOO_FEW;
        return 1;
    }
    if (argc > 2) {
        fprintf(stderr, "%s%s", errs[TOO_MANY], s_usage);
        retval = TOO_MANY;
        return 1;
    }

    THREADPOOL_SIZE = atol(argv[1]);
    if (THREADPOOL_SIZE <= 0) {
        fprintf(stderr, "%s%s", errs[INV_ARG], s_usage);
        retval = INV_ARG;
        return 1;
    }
    return 0;
}

void sigusr1_handler(int signum, siginfo_t *info, void *context)
{
    (void)signum;
    (void)context;

    // just in case
    if (info->si_code != SI_USER && info->si_code != SI_TKILL) {
        return;
    }
    enqueue_pending_client(info.si_pid);
}

void shutdown_handler(int signum)
{
    (void)signum;
    RUNNING = 0;
}

int setup_signals()
{
    struct sigaction sa = {0};
    sigemptyset(&sa.sa_mask);

    /* SIGUSR1 */
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Signal setup fail");
        return 1;
    }

    /* SIGINT */

    /* SIGTERM */

    /* SIGQUIT */

    /* SIGHUP */
    return 0;
}