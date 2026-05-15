#include "task.h"
#include <pthread.h>

static TaskQueue TASK_Q;

void task_init()
{
    tq_head      = 0;
    tq_tail      = 0;
    tq_count     = 0;
    tq_lock      = PTHREAD_MUTEX_INITIALIZER;
    tq_not_full  = PTHREAD_COND_INITIALIZER;
    tq_not_empty = PTHREAD_COND_INITIALIZER;
}

Task *task_make(pid_t client_pid)
{
    Task *task       = malloc(sizeof(*task));
    task->client_pid = client_pid;
    set_name(task->c2s_name, C2S, client_pid);
    set_name(task->s2c_name, S2C, client_pid);
    return task;
}

void task_enqueue(Task *task)
{
    pthread_mutex_lock(tq_lock);

    while (tq_count == TASK_MAX_N) {
        pthread_cond_wait()
    }

    // update info
    tq_tasks[head] = task;
    tq_head++;
    tq_count++;

    // signal consumer

    pthread_mutex_unlock(tq_lock);
}

Task *task_dequeue()
{
    pthread_mutex_lock(tq_lock);

    pthread_mutex_unlock(tq_lock);
}

void task_unqueue(pid_t client_pid)
{
    pthread_mutex_lock(tq_lock);

    pthread_mutex_unlock(tq_lock);
}

void task_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(tq_lock);
    for (size_t i = 0; i < tq_count; ++i) {
        free(tq_tasks[i]);
    }
    free(tq_tasks);
    pthread_mutex_lock(tq_lock);
}