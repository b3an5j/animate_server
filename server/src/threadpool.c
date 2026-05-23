#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "auth.h"
#include "fifo.h"
#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

#define MAX_OPEN_RETRIES 10
#define OPEN_WAIT_TIME 10000000 // 10 ms

ThreadPool THREADPOOL;

int get_threadpool_size(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "%s\n%s", errs[TOO_FEW], s_usage);
        fflush(stderr);
        retval = TOO_FEW;
        return TOO_FEW;
    }
    if (argc > 2) {
        fprintf(stderr, "%s\n%s", errs[TOO_MANY], s_usage);
        fflush(stderr);
        retval = TOO_MANY;
        return TOO_MANY;
    }

    TPOOL_SIZE = atol(argv[1]);
    if (TPOOL_SIZE <= 0) {
        fprintf(stderr, "%s %ld.\n\n%s", errs[POOL_FAIL], TPOOL_SIZE, s_usage);
        fflush(stderr);
        retval = POOL_FAIL;
        return POOL_FAIL;
    }
    else if (TPOOL_SIZE > 0) {
        return SUCCESS;
    }
    fprintf(stderr, "%s\n%s", errs[INV_ARG], s_usage);
    fflush(stderr);
    retval = INV_ARG;
    return INV_ARG;
}

static void *worker_routine(void *)
{
    while (RUNNING) {
        Task *task = task_dequeue();
        if (!task)
            continue;

        switch (task->type) {
        case HANDSHAKE: {
            TaskResult taskresult;
            pid_t      client_pid    = task->task_rawpid;
            taskresult.result_rawpid = client_pid;

            // unlink first
            unlink(task->task_c2s_name);
            unlink(task->task_s2c_name);

            // make fifo
            mode_t  old_umask = umask(0);
            uint8_t fifo_fail = mkfifo(task->task_c2s_name, 0666) != 0 ||
                                mkfifo(task->task_s2c_name, 0666) != 0;
            umask(old_umask);
            if (fifo_fail) {
                goto hsk_fail;
            }

            // signal client
            if (kill(client_pid, SIGUSR2) == -1) {
                goto hsk_fail;
            }

            // open read fifo
            int r_temp = open(task->task_c2s_name, O_NONBLOCK | O_RDONLY);
            if (r_temp == -1) {
                goto hsk_fail;
            }
            taskresult.result_c2s_fd = r_temp;

            // open write fifo
            int w_temp = open(task->task_s2c_name, O_NONBLOCK | O_WRONLY);
            if (w_temp == -1) {
                if (errno == ENXIO) { // give chance
                    struct timespec t_temp = {0};
                    t_temp.tv_nsec         = OPEN_WAIT_TIME;

                    for (int attempts = 0; attempts < MAX_OPEN_RETRIES;
                         ++attempts) {
                        w_temp =
                            open(task->task_s2c_name, O_NONBLOCK | O_WRONLY);

                        if (w_temp != -1)
                            break;

                        if (errno == ENXIO)
                            nanosleep(&t_temp, NULL);
                        else // other err
                            break;
                    }
                }

                if (w_temp == -1) {
                    close(taskresult.result_c2s_fd);
                    goto hsk_fail;
                }
            }
            taskresult.result_s2c_fd = w_temp;

            taskresult.type = HSK_OK;
            // ADD INTERNAL ERROR
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;

        hsk_fail:
            unlink(task->task_c2s_name);
            unlink(task->task_s2c_name);

            fprintf(stderr, "%s %d.\n", errs[PIPE_FAIL], client_pid);
            fflush(stderr);
            // ignore and drop the handshake
            free(task);
            continue;
        }

        case AUTHORISE: {
            TaskResult    taskresult;
            ActiveClient *client     = task->task_client;
            taskresult.result_client = client;

            long       balance;
            ResultType ret  = usertxt_get_balance(task->task_request, &balance);
            taskresult.type = ret;
            if (ret == AUTH_OK) {
                taskresult.result_balance = balance;
                strncpy(taskresult.result_username,
                        task->task_request,
                        MAX_USERNAME_LEN - 1);
                taskresult.result_username[MAX_USERNAME_LEN - 1] = '\0';
            }

            // ADD INTERNAL ERROR
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case RPC:
            break;

        default:
            break;
        }

        free(task);
    }
    return NULL;
}

int threadpool_init()
{
    pthread_t *temp;
    temp = malloc(sizeof(*tpool_threads) * TPOOL_SIZE);
    if (temp == NULL) {
        fprintf(stderr, errs[POOL_FAIL], TPOOL_SIZE);
        fflush(stderr);
        return POOL_FAIL;
    }
    tpool_threads = temp;

    // assert non negative

    size_t created = 0;
    for (size_t i = 0; i < (size_t)TPOOL_SIZE; ++i) {
        if (pthread_create(&tpool_threads[i], NULL, worker_routine, NULL) !=
            0) {
            RUNNING = 0;
            pthread_cond_broadcast(&tq_not_empty); // wake up

            // reap
            for (size_t j = 0; j < created; ++j) {
                pthread_join(tpool_threads[j], NULL);
            }
            free(tpool_threads);
            return POOL_FAIL;
        }
        created++;
    }
    return SUCCESS;
}

void threadpool_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(&tq_lock);
    pthread_cond_broadcast(&tq_not_empty);
    pthread_mutex_unlock(&tq_lock);

    for (long i = 0; i < TPOOL_SIZE; ++i) {
        pthread_join(tpool_threads[i], NULL);
    }
    free(tpool_threads);
    tpool_threads = NULL;
}