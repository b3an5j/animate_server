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

            // exhaust the pipe
            unsigned char empty_pipe = 0;
            while (!empty_pipe) {
                size_t n_read   = 0;
                size_t n_target = sizeof(pid_t);
                pid_t  client_pid;
                char  *ptr = (char *)&client_pid;

                while (n_read < n_target) {
                    ssize_t n = read(HSK_R, ptr + n_read, n_target - n_read);
                    if (n > 0) {
                        n_read += (size_t)n;
                    }
                    else if (n == 0) {
                        fprintf(stderr, errs[PIPE_CLOSED], "Handshake");
                        fflush(stderr);
                        RUNNING = 0;
                        goto mainloop_end;
                    }
                    else { // errors
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // nonblocking, could be empty or broken
                            if (!n_read) {
                                empty_pipe = 1;
                                break;
                            }
                            else {
                                fprintf(
                                    stderr, errs[PIPE_PARTIAL], "Handshake");
                                fflush(stderr);
                                RUNNING = 0;
                                goto mainloop_end;
                            }
                        }
                        else if (errno == EINTR) {
                            continue;
                        }
                        else {
                            fprintf(stderr, errs[PIPE_READ_FAIL], "Handshake");
                            fflush(stderr);
                            RUNNING = 0;
                            goto mainloop_end;
                        }
                    }
                }

                if (!empty_pipe && n_read == n_target) {
                    task_enqueue(HANDSHAKE, client_pid, 0, 0);
                }
            }
        }

        /* Handle RPC inter thread communication */
        if (RPC_POLLFD.revents & POLLIN) {

            // exhaust the pipe
            unsigned char empty_pipe = 0;
            while (!empty_pipe) {
                size_t     n_read   = 0;
                size_t     n_target = sizeof(TaskResult);
                TaskResult taskresult;
                char      *ptr = (char *)&taskresult;

                while (n_read < n_target) {
                    ssize_t n = read(RPC_R, ptr + n_read, n_target - n_read);
                    if (n > 0) {
                        n_read += (size_t)n;
                    }
                    else if (n == 0) {
                        fprintf(stderr, errs[PIPE_CLOSED], "Inter thread");
                        fflush(stderr);
                        RUNNING = 0;
                        goto mainloop_end;
                    }
                    else { // errors
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // nonblocking, could be empty or broken
                            if (!n_read) {
                                empty_pipe = 1;
                                break;
                            }
                            else {
                                fprintf(
                                    stderr, errs[PIPE_PARTIAL], "Inter thread");
                                fflush(stderr);
                                RUNNING = 0;
                                goto mainloop_end;
                            }
                        }
                        else if (errno == EINTR) {
                            continue;
                        }
                        else {
                            fprintf(
                                stderr, errs[PIPE_READ_FAIL], "Inter thread");
                            fflush(stderr);
                            RUNNING = 0;
                            goto mainloop_end;
                        }
                    }
                }
                if (empty_pipe) {
                    break;
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
                        continue;
                    }

                    if (server_insert_pollfd(c2s_fd) != SUCCESS) {
                        // drop straight away
                        creg_remove(c2s_fd);
                        continue;
                    }
                    break;

                case RPC:
                    break;

                default:
                    break;
                }
            }
        }

        /* Handle client RPC requests */
        for (size_t i = 2; i < S_POLL_FD_COUNT;) {
            // errors
            if (S_POLL_FDS[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                creg_remove(S_POLL_FDS[i].fd);
                server_remove_pollfd(S_POLL_FDS[i].fd);
                continue;
            }

            // normal
            if (S_POLL_FDS[i].revents & POLLIN) {
                // do sth
            }
            ++i;
        }
    mainloop_end:
    }

teardown:
    /* Teardown */
    threadpool_destroy();
    creg_destroy();
    server_destroy_pipes();
    task_destroy();
    return retval ? retval : 0;
}
