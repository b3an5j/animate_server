#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <animate/animate.h>

#include "client_registry.h"
#include "errors.h"
#include "fifo.h"
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

    if (get_threadpool_size(argc, argv) != SUCCESS ||
        usertxt_check() != SUCCESS) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        goto teardown;
    }

    creg_init();
    task_init();

    if (server_setup_pipes() != SUCCESS || server_setup_signals() != SUCCESS) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        goto teardown;
    }
    if (threadpool_init() != SUCCESS) { // signal deaf threads
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
        int ready = poll(S_POLL_SLOTS, S_POLL_FD_COUNT, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("Poll failure");
            break;
        }

        /* Handle Handshake requests */
        if (HSK_POLLFD.revents & POLLIN) {
            pid_t client_pid;

            // exhaust the pipe
            while (true) {
                ErrType pipe_ret =
                    read_nonblock_pipe(HSK_R, &client_pid, sizeof(client_pid));

                if (pipe_ret == PIPE_EMPTY) {
                    break;
                }

                if (pipe_ret == PIPE_CLOSED) {
                    fprintf(stderr, errs[PIPE_CLOSED], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_CLOSED;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_PARTIAL) {
                    fprintf(stderr, errs[PIPE_PARTIAL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_PARTIAL;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_READ_FAIL) {
                    fprintf(stderr, errs[PIPE_READ_FAIL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_READ_FAIL;
                    goto mainloop_end;
                }

                if (pipe_ret != SUCCESS) {
                    RUNNING = 0;
                    retval  = FAIL;
                    goto mainloop_end;
                }

                task_enqueue(HANDSHAKE, client_pid, 0);
            }
        }

        /* Handle RPC inter thread communication */
        if (RPC_POLLFD.revents & POLLIN) {
            TaskResult taskresult;

            // exhaust the pipe
            while (true) {
                ErrType pipe_ret =
                    read_nonblock_pipe(RPC_R, &taskresult, sizeof(taskresult));

                if (pipe_ret == PIPE_EMPTY) {
                    break;
                }

                if (pipe_ret == PIPE_CLOSED) {
                    fprintf(stderr, errs[PIPE_CLOSED], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_CLOSED;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_PARTIAL) {
                    fprintf(stderr, errs[PIPE_PARTIAL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_PARTIAL;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_READ_FAIL) {
                    fprintf(stderr, errs[PIPE_READ_FAIL], "Inter thread");
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_READ_FAIL;
                    goto mainloop_end;
                }

                if (pipe_ret != SUCCESS) {
                    RUNNING = 0;
                    retval  = FAIL;
                    goto mainloop_end;
                }

                switch (taskresult.type) {
                case HANDSHAKE: {
                    pid_t client = taskresult.client_pid;
                    int   c2s_fd = taskresult.result_c2s_fd;
                    int   s2c_fd = taskresult.result_s2c_fd;

                    ActiveClient *client;
                    if ((client = creg_insert(client, c2s_fd, s2c_fd)) ==
                        NULL) {
                        // drop straight away
                        destroy_client_pipes(client, c2s_fd, s2c_fd);
                        continue;
                    }

                    if (server_insert_pollslots(c2s_fd, client) != SUCCESS) {
                        // drop straight away
                        creg_remove(c2s_fd);
                        continue;
                    }
                    break;
                }

                case RPC:
                    break;

                default:
                    break;
                }
                break;
            }
        }

        /* Handle client RPC requests */
        for (size_t i = 2; i < S_POLL_FD_COUNT;) {
            short         rev    = S_POLL_SLOTS[i].revents;
            int           fd     = S_POLL_SLOTS[i].fd;
            ActiveClient *client = S_POLL_SLOTS[i].client;

            // errors
            if (rev & (POLLERR | POLLNVAL)) {
                goto client_pipe_cleanup;
            }

            // exhaust pipe
            if (rev & (POLLIN | POLLHUP)) {
                while (true) {
                    char    request[MAX_RPC_BUF_LEN];
                    ErrType pipe_ret =
                        read_until_delim(fd, request, MAX_RPC_BUF_LEN, '\0');
                    if (pipe_ret == PIPE_CLOSED) {
                        fprintf(stderr, errs[PIPE_CLOSED], "Client");
                        fflush(stderr);
                        goto client_pipe_cleanup;
                    }
                    if (pipe_ret == PIPE_PARTIAL) {
                        fprintf(stderr, errs[PIPE_PARTIAL], "Client");
                        fflush(stderr);
                        goto client_pipe_cleanup;
                    }
                    if (pipe_ret == PIPE_READ_FAIL) {
                        fprintf(stderr, errs[PIPE_READ_FAIL], "Client");
                        fflush(stderr);
                        goto client_pipe_cleanup;
                    }
                    if (pipe_ret != SUCCESS) {
                        goto client_pipe_cleanup;
                    }

                    if (strncmp(request, "Login ", 6) == 0) {
                        char *username = &request[7];
                        task_enqueue(AUTHORISE, username, client);
                    }
                }
            }
            continue;

        client_pipe_cleanup:
            server_remove_pollslots(fd);
            creg_remove(client);
        }
    mainloop_end:;
    }

teardown:
    /* Teardown */
    threadpool_destroy();
    server_destroy_selfpipes();
    server_destroy_pollslots();
    creg_destroy();
    task_destroy();
    usertxt_close();
    return retval ? retval : 0;
}
