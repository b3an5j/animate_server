#define _POSIX_C_SOURCE 200809L

#include "queues.h"
#include "errors.h"
#include <signal.h>
#include <stdio.h>

/* FIFO */
void set_name(const char *buf, FifoType type, pid_t pid)
{
    switch (type) {
    case C2S:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_C2S_%d", pid);
        break;
    case S2C:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_S2C_%d", pid);
        break;
    default:
        break;
    }
}

/* PENDING CLIENTS */
void enqueue_pending_client(pid_t client_pid)
{
    if (pc_count < PENDING_MAX_N) {
        pc_pids[pc_insert] = client_pid;
        pc_insert          = (pc_insert + 1) % PENDING_MAX_N;
        pc_count++;
    }
}

pid_t dequeue_pending_client()
{
    if (!pc_count) {
        return -1;
    }

    pid_t client_pid = pc_pids[pc_take];
    pc_take          = (pc_take + 1) % PENDING_MAX_N;
    pc_count--;
    return client_pid;
}

/* TASK */
void enqueue_task(pid_t client_pid, int c2s_fd, int s2c_fd)
{
    TaskParams
}

void dequeue_task();
void unqueue_task();