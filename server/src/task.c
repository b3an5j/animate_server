#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_registry.h"
#include "errors.h"
#include "task.h"

TaskQueue TASK_Q;

void task_init()
{
    tq_head  = NULL;
    tq_tail  = NULL;
    tq_count = 0;
    pthread_mutex_init(&tq_lock, NULL);
    pthread_cond_init(&tq_not_empty, NULL);
}

ErrType task_enqueue(TaskType      type, //
                     pid_t         client_pid,
                     const char   *request,
                     ActiveClient *client)
{
    Task *newtask = malloc(sizeof(*newtask));
    if (newtask == NULL) {
        fputs(errs[TQ_FAIL], stderr);
        fflush(stderr);
        retval = TQ_FAIL;
        return TQ_FAIL;
    }

    newtask->type = type;
    newtask->next = NULL;

    switch (type) {
    case HANDSHAKE:
        newtask->task_rawpid = client_pid;
        set_name(newtask->task_c2s_name, C2S, client_pid);
        set_name(newtask->task_s2c_name, S2C, client_pid);
        break;

    case AUTHORISE:
    case RPC:
        newtask->task_client = client;
        strncpy(newtask->task_request, request, MAX_RPC_BUF_LEN - 1);
        newtask->task_request[MAX_RPC_BUF_LEN - 1] = '\0';
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
    return SUCCESS;
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