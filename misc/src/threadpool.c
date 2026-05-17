#include "threadpool.h"
#include "task.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static ThreadPool THREADPOOL;

int get_threadpool_size(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "%s\n%s", errs[TOO_FEW], s_usage);
        fflush(stderr);
        retval = TOO_FEW;
        return 1;
    }
    if (argc > 2) {
        fprintf(stderr, "%s\n%s", errs[TOO_MANY], s_usage);
        fflush(stderr);
        retval = TOO_MANY;
        return 1;
    }

    TPOOL_SIZE = atol(argv[1]);
    if (TPOOL_SIZE <= 0) {
        fprintf(stderr, "%s\n%s", errs[INV_ARG], s_usage);
        fflush(stderr);
        retval = INV_ARG;
        return 1;
    }
    return 0;
}

static void *worker_routine(void *data)
{
}

int threadpool_init()
{
    tpool_threads = malloc(sizeof(*tpool_threads) * TPOOL_SIZE);
    if (tpool_threads == NULL) {
        fprintf(stderr, errs[POOL_FAIL], TPOOL_SIZE);
        fflush(stderr);
        retval = POOL_FAIL;
        return 1;
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
            return 1;
        }
        created++;
    }
    return 0;
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