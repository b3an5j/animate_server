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

volatile sig_atomic_t RUNNING = 0;

int main(int argc, char **argv, char **envp)
{
    (void)envp;

    /* Start procedure */
    // block, no interruption
    sigset_t mask, oldmask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    if (get_threadpool_size(argc, argv) != SUCCESS) {
        RUNNING = 0;
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        goto teardown;
    }

    creg_init();
    task_init();

    if (server_setup_pipes() != SUCCESS || server_setup_signals() != SUCCESS) {
        RUNNING = 0;
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        goto teardown;
    }
    if (threadpool_init() != SUCCESS) { // signal deaf threads
        RUNNING = 0;
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        goto teardown;
    }

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

            // exhaust the pipe
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
            TaskResult taskresult;
            size_t     n_read;

            // sanity check
            if ((n_read = read(RPC_R, &taskresult, sizeof(TaskResult))) !=
                sizeof(TaskResult)) {
                if (n_read == 0) {
                    fprintf(stderr, errs[PIPE_CLOSED], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    break;
                }
                else if (n_read > 0 && n_read < sizeof(TaskResult)) {
                    fprintf(stderr, errs[PIPE_PARTIAL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    break;
                }
                else if (n_read < 0) {
                    fprintf(stderr, errs[PIPE_READ_FAIL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    break;
                }
            }

            switch (taskresult.type) {
            case HANDSHAKE:
                pid_t client = taskresult.client_pid;
                int   c2s_fd = taskresult.result_c2s_fd;
                int   s2c_fd = taskresult.result_s2c_fd;

                if (creg_insert(client, c2s_fd, s2c_fd) != SUCCESS) {
                    // drop straight away
                    close(taskresult.result_c2s_fd);
                    close(taskresult.result_s2c_fd);

                    char temp[FIFO_MAX_NAME];
                    set_name(temp, C2S, client);
                    unlink(temp);
                    set_name(temp, S2C, client);
                    unlink(temp);
                    goto skip_ithread;
                }

                if (server_insert_pollfd(c2s_fd) != SUCCESS) {
                    // drop straight away
                    creg_remove(client);

                    close(taskresult.result_c2s_fd);
                    close(taskresult.result_s2c_fd);

                    char temp[FIFO_MAX_NAME];
                    set_name(temp, C2S, client);
                    unlink(temp);
                    set_name(temp, S2C, client);
                    unlink(temp);
                    goto skip_ithread;
                }
                break;

            case RPC:
                break;

            default:
                break;
            }
        }
    skip_ithread:

        /* Handle client RPC requests */
    }

teardown:
    /* Teardown */
    threadpool_destroy();
    creg_destroy();
    server_destroy_pipes();
    task_destroy();
    return retval ? retval : 0;
}
