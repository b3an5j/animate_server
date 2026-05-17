#include "task.h"
#include "errors.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static TaskQueue TASK_Q;

void task_init()
{
    tq_head  = NULL;
    tq_tail  = NULL;
    tq_count = 0;
    pthread_mutex_init(&tq_lock, NULL);
    pthread_cond_init(&tq_not_empty, NULL);
    return 0;
}

int task_enqueue(TaskType type, //
                 pid_t    client_pid,
                 int      c2s_fd,
                 int      s2c_fd)
{
    Task *newtask = malloc(sizeof(*newtask));
    if (newtask == NULL) {
        fprintf(stderr, errs[TQ_FAIL]);
        fflush(stderr);
        retval = TQ_FAIL;
        return 1;
    }

    newtask->type = type;
    newtask->next = NULL;

    switch (type) {
    case HANDSHAKE:
    case GOODBYE:
        newtask->task_client_pid = client_pid;
        set_name(newtask->task_c2s_name, C2S, client_pid);
        set_name(newtask->task_s2c_name, S2C, client_pid);
        break;

    case RPC:
        newtask->task_c2s_fd = c2s_fd;
        newtask->task_s2c_fd = s2c_fd;
        break;

    default:
        break;
    }

    // update info
    pthread_mutex_lock(&tq_lock);

    if (tq_tail == NULL) { // empty queue
        tq_head = newtask;
        tq_tail = newtask;
    }
    else { // populated queue
        tq_tail->next = newtask;
        tq_tail       = newtask;
    }
    tq_count++;

    // signal consumer
    pthread_cond_signal(&tq_not_empty);

    pthread_mutex_unlock(&tq_lock);
    return 0;
}

Task *task_dequeue()
{
    pthread_mutex_lock(&tq_lock);

    /* Get */
    // will go to work even if cond signal dropped
    while (tq_head == NULL && RUNNING) {
        pthread_cond_wait(&tq_not_empty, &tq_lock);
    }

    // shutdown
    if (!RUNNING) {
        pthread_mutex_unlock(&tq_lock);
        return NULL;
    }

    Task *currtask = tq_head;
    if (currtask) {
        /* Unlink */
        tq_head = tq_head->next;

        /* Update info */
        if (--tq_count == 0) {
            tq_head = NULL;
            tq_tail = NULL;
        }
    }

    pthread_mutex_unlock(&tq_lock);
    return currtask;
}

void task_unqueue(pid_t client_pid)
{
    pthread_mutex_lock(&tq_lock);

    pthread_mutex_unlock(&tq_lock);
}

void task_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(&tq_lock);

    Task *next;
    while (tq_head) {
        next = tq_head->next;
        free(tq_head);
        tq_head = next;
    }

    pthread_mutex_unlock(&tq_lock);

    pthread_mutex_destroy(&tq_lock);
    pthread_cond_destroy(&tq_not_empty);
}