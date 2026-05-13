#ifndef QUEUES_H
#define QUEUES_H

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>

/* FIFO */
#define FIFO_MAX_NAME 64

typedef enum {
    C2S,
    S2C
} FifoType;

void set_name(const char *buf, FifoType type, pid_t pid);

/* PENDING CLIENTS */
#define PENDING_MAX_N 64

typedef struct {
    pid_t        pending_pids[PENDING_MAX_N];
    sig_atomic_t pending_count;
    int          ppid_insert;
    int          ppid_take;
} PendingClients;

volatile PendingClients PENDING_CLIENTS = {
    .pending_count = 0, .ppid_insert = 0, .ppid_take = 0};
;

// Pending client aliases
#define pc_pids PENDING_CLIENTS.pending_pids
#define pc_count PENDING_CLIENTS.pending_count
#define pc_insert PENDING_CLIENTS.ppid_insert
#define ppid_take PENDING_CLIENTS.ppid_take

pid_t enqueue_pending_client(pid_t client_pid);
pid_t dequeue_pending_client();

/* TASK */
#define TASK_MAX_N 1024

typedef struct {
    pid_t   client_pid;
    int     c2s_fd;
    int     s2c_fd;
    ErrType retval;
} Task;

typedef volatile struct {
    Task            tasks[TASK_MAX_N];
    size_t          head;
    size_t          tail;
    size_t          count;
    pthread_mutex_t lock;
    sem_t           tasks_available;
    sem_t           space_available;
} TaskQueue;

TaskQueue TASK_Q = {.head            = 0,
                    .tail            = 0,
                    .count           = 0,
                    .tasks_available = 0,
                    .space_available = TASK_MAX_N};

// Task queue aliases
#define tq_tasks TASK_Q.tasks
#define tq_head TASK_Q.head
#define tq_tail TASK_Q.tail
#define tq_count TASK_Q.count
#define tq_lock TASK_Q.lock
#define tq_tasks_available TASK_Q.tasks_available
#define tq_space_available TASK_Q.space_available

void enqueue_task(pid_t client_pid, int c2s_fd, int s2c_fd);
void dequeue_task();
void unqueue_task();

#endif /* QUEUES_H */