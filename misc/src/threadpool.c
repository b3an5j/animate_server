#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

static ThreadPool THREADPOOL;

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
        fprintf(stderr, "%s\n%s", errs[INV_ARG], s_usage);
        fflush(stderr);
        retval = INV_ARG;
        return INV_ARG;
    }
    return SUCCESS;
}

static void *worker_routine(void *)
{
    while (RUNNING) {
        Task *task = task_dequeue();
        if (!task)
            continue;

        TaskResult taskresult;
        taskresult.type = task->type;

        switch (task->type) {
        case HANDSHAKE:
            // unlink first
            unlink(task->task_c2s_name);
            unlink(task->task_s2c_name);

            // make fifo
            mode_t old_umask = umask(0);
            if (mkfifo(task->task_c2s_name, 0666) != 0 ||
                mkfifo(task->task_s2c_name, 0666) != 0) {
                fprintf(stderr, errs[PIPE_FAIL], task->task_client_pid);
                // ignore and drop the handshake
                free(task);
                continue;
            }
            umask(old_umask);

            // signal client
            kill(task->task_client_pid, SIGUSR2);

            // open fifo
            int temp;
            temp = open(task->task_c2s_name, O_NONBLOCK | O_RDONLY);
            if (temp == -1) {
                unlink(task->task_c2s_name);
                unlink(task->task_s2c_name);

                fprintf(stderr, errs[PIPE_FAIL], task->task_client_pid);
                // ignore and drop the handshake
                free(task);
                continue;
            }
            taskresult.result_c2s_fd = temp;

            temp = open(task->task_s2c_name, O_RDWR); // simulate nonblock
            if (temp == -1) {
                unlink(task->task_c2s_name);
                unlink(task->task_s2c_name);

                fprintf(stderr, errs[PIPE_FAIL], task->task_client_pid);
                // ignore and drop the handshake
                free(task);
                continue;
            }
            taskresult.result_s2c_fd = temp;
            break;

        case RPC:
            break;

        case GOODBYE:
            break;

        default:
            break;
        }

        write(RPC_W, &taskresult, sizeof(TaskResult));
        free(task);
    }
}

int threadpool_init()
{
    tpool_threads = malloc(sizeof(*tpool_threads) * TPOOL_SIZE);
    if (tpool_threads == NULL) {
        fprintf(stderr, errs[POOL_FAIL], TPOOL_SIZE);
        fflush(stderr);
        return POOL_FAIL;
    }

    // assert non negative

    size_t created = 0;
    for (size_t i = 0; i < (size_t)TPOOL_SIZE; ++i) {
        if (pthread_create(&tpool_threads[i], NULL, worker_routine, NULL) !=
            0) {
            RUNNING = 0;
            pthread_cond_broadcast(&tq_not_empty); // wake up

            // reap
            for (size_t j = 0; j < created; ++j) {
                pthread_join(&tpool_threads[j], NULL);
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

    for (size_t i = 0; i < TPOOL_SIZE; ++i) {
        pthread_join(tpool_threads[i], NULL);
    }
}