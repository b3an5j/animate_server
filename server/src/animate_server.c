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

#include "auth.h"
#include "client_registry.h"
#include "commands.h"
#include "dbg.h"
#include "errors.h"
#include "fifo.h"
#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

volatile sig_atomic_t RUNNING = 1;
ErrType               retval  = SUCCESS;

int main(int argc, char **argv, char **envp)
{
    (void)envp;

    EN_DEBUG = 1;

    /* Start procedure */
    // block, no interruption
    sigset_t all;
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, NULL);

    if (get_threadpool_size(argc, argv) != SUCCESS) {
        goto teardown;
    }
    // if (usertxt_check() != SUCCESS) {
    //     goto teardown;
    // }

    creg_init();
    task_init();
    if (server_setup_pipes() != SUCCESS) {
        goto teardown;
    }

    if (threadpool_init() != SUCCESS) { // signal deaf threads
        goto teardown;
    }
    if (server_setup_signals() != SUCCESS) {
        goto teardown;
    }

    pid_t SERVER_PID = getpid();
    printf("Server PID: %d\n", SERVER_PID);
    fflush(stdout);

    // return the mask
    pthread_sigmask(SIG_UNBLOCK, &all, NULL);

    /* Waiting handshake or RPC */
    while (RUNNING) {
        int ready = poll(S_POLL_PFDS, S_POLL_FD_COUNT, -1);
        if (ready == -1) {
            if (errno == EINTR) {
                debug_log("Poll interrupted by signal");
                continue;
            }
            perror("Poll failure");
            break;
        }

        /* Handle Handshake requests */
        if (HSK_POLLFD.revents & POLLIN) {
            pid_t client_pid;

            debug_log("Entered handshake request handler");
            for (int i = 0; RUNNING && i < HSK_MAX_PER_WAKE; i++) {
                ErrType pipe_ret =
                    read_nonblock_pipe(HSK_R, &client_pid, sizeof(client_pid));

                if (pipe_ret == PIPE_EMPTY) {
                    break;
                }

                if (pipe_ret == PIPE_CLOSED) {
                    fprintf(stderr, "%s %s", "Inter thread", errs[PIPE_CLOSED]);
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_CLOSED;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_PARTIAL) {
                    fprintf(
                        stderr, "%s %s", "Inter thread", errs[PIPE_PARTIAL]);
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

                task_enqueue(HANDSHAKE, client_pid, NULL, NULL);
            }
        }

        /* Handle RPC inter thread communication */
        if (RPC_POLLFD.revents & POLLIN) {
            TaskResult taskresult;

            debug_log("Entered inter thread handler");
            for (int i = 0; RUNNING && i < RPC_MAX_PER_WAKE; i++) {
                ErrType pipe_ret =
                    read_nonblock_pipe(RPC_R, &taskresult, sizeof(taskresult));

                if (pipe_ret == PIPE_EMPTY) {
                    break;
                }

                if (pipe_ret == PIPE_CLOSED) {
                    fprintf(stderr, "%s %s", "Inter thread", errs[PIPE_CLOSED]);
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_CLOSED;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_PARTIAL) {
                    fprintf(
                        stderr, "%s %s", "Inter thread", errs[PIPE_PARTIAL]);
                    fflush(stderr);
                    RUNNING = 0;
                    retval  = PIPE_PARTIAL;
                    goto mainloop_end;
                }

                if (pipe_ret == PIPE_READ_FAIL) {
                    fprintf(stderr,
                            "%s %s.\n",
                            errs[PIPE_READ_FAIL],
                            "Inter thread");
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
                case HSK_OK: {
                    pid_t         client_pid = taskresult.result_rawpid;
                    int           c2s_fd     = taskresult.result_c2s_fd;
                    int           s2c_fd     = taskresult.result_s2c_fd;
                    ActiveClient *client;

                    if ((client = creg_insert(client_pid, c2s_fd, s2c_fd)) ==
                        NULL) {
                        // drop straight away
                        destroy_client_pipes(client_pid, c2s_fd, s2c_fd);
                        continue;
                    }

                    if (server_insert_pollslots(c2s_fd, client) != SUCCESS) {
                        // drop straight away
                        creg_remove(client);
                        continue;
                    }
                    printf("Successful handshake with %d\n", client_pid);
                    fflush(stdout);
                    break;
                }

                case AUTH_OK: {
                    ActiveClient *client     = taskresult.result_client;
                    pid_t         client_pid = client->client_pid;
                    char         *username   = taskresult.result_username;

                    // store username
                    strncpy(client->username, username, MAX_USERNAME_LEN - 1);
                    client->username[MAX_USERNAME_LEN - 1] = '\0';

                    // send balance
                    char buffer[64];
                    int  n = snprintf(buffer,
                                      sizeof(buffer),
                                      "%ld\n",
                                      taskresult.result_balance);

                    creg_release_client(client);

                    write_block_pipe(client->s2c_fd, buffer, n);

                    client->logged_in = 1;

                    printf(
                        "Successful login with %d: %s\n", client_pid, username);
                    break;
                }

                case AUTH_NO: {
                    ActiveClient *client = taskresult.result_client;

                    creg_release_client(client);
                    write_block_pipe(client->s2c_fd,
                                     MSG_REJECT_BALANCE,
                                     strlen(MSG_REJECT_BALANCE));

                    struct timespec ts = {0};
                    ts.tv_sec          = 1;
                    ts.tv_nsec         = 0;
                    nanosleep(&ts, NULL);
                    creg_remove(client);
                    debug_log("removed");
                    break;
                }

                case AUTH_UN: {
                    ActiveClient *client = taskresult.result_client;

                    creg_release_client(client);
                    write_block_pipe(client->s2c_fd,
                                     MSG_REJECT_UNAUTHORISED,
                                     strlen(MSG_REJECT_UNAUTHORISED));

                    struct timespec ts = {0};
                    ts.tv_sec          = 1;
                    ts.tv_nsec         = 0;
                    nanosleep(&ts, NULL);
                    creg_remove(client);
                    debug_log("removed");
                    break;
                }

                default:
                    break;
                }
            }
        }

        /* Handle client RPC requests */
        debug_log("Entered client request handler");
        for (size_t i = 2; RUNNING && i < S_POLL_FD_COUNT;) {
            short         rev     = S_POLL_PFDS[i].revents;
            int           read_fd = S_POLL_PFDS[i].fd;
            ActiveClient *client  = S_POLL_CLIENT[i];
            // int write_fd = client->s2c_fd;
            uint8_t remove = false;

            // errors
            if (rev & (POLLERR | POLLNVAL)) {
                remove = true;
            }
            else if (rev & (POLLIN | POLLHUP)) {
                // handle the client
                for (int req = 0; req < FIFO_MAX_PER_WAKE; req++) {
                    char    request[MAX_RPC_BUF_LEN];
                    size_t  len;
                    ErrType pipe_ret = read_until_delim(
                        read_fd, request, MAX_RPC_BUF_LEN, '\n', &len);

                    if (pipe_ret == SUCCESS) {
                        request[len] = '\0';
                        debug_log("Client says: %s", request);
                        int remove_client = handle_command(client, request);
                        if (remove_client) {
                            remove = true;
                            break;
                        }
                    }
                    else if (pipe_ret == PIPE_EMPTY) {
                        break;
                    }
                    else if (pipe_ret == PIPE_CLOSED) {
                        fprintf(stderr, errs[PIPE_CLOSED], "Client");
                        fflush(stderr);
                        remove = true;
                        break;
                    }
                    else if (pipe_ret == PIPE_READ_FAIL) {
                        fprintf(stderr, errs[PIPE_READ_FAIL], "Client");
                        fflush(stderr);
                        remove = true;
                        break;
                    }
                }
            }
            if (!remove) {
                ++i;
            }
            else {
                server_remove_pollslots(read_fd);
                creg_remove(client);
            }
        }
    mainloop_end:;
    }

teardown:
    RUNNING = 0;
    /* Teardown */
    threadpool_destroy();
    server_destroy_selfpipes();
    server_destroy_pollslots();
    creg_destroy();
    task_destroy();
    return retval ? retval : 0;
}