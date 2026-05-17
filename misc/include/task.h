#ifndef TASK_H
#define TASK_H

#include "errors.h"
#include "fifo.h"
#include <sys/types.h>

extern volatile sig_atomic_t RUNNING;

/* Task */
typedef enum {
    HANDSHAKE,
    RPC,
    GOODBYE
} TaskType;

typedef struct Task {
    TaskType type;
    union {
        struct {
            pid_t client_pid;
            char  c2s_name[FIFO_MAX_NAME];
            char  s2c_name[FIFO_MAX_NAME];
        } fifo;
        struct {
            int c2s_fd;
            int s2c_fd;
        } rpc;
    } info;
    struct Task *next;
} Task;

// Task aliases
#define task_client_pid info.fifo.client_pid
#define task_c2s_name info.fifo.c2s_name
#define task_s2c_name info.fifo.s2c_name
#define task_c2s_fd info.rpc.c2s_fd
#define task_s2c_fd info.rpc.s2c_fd

/* Task result */
typedef struct {
    TaskType type;
    pid_t    client_pid;
    union {
        int fds[2];
    } result;
} TaskResult;

// Task result aliases
#define result_c2s_fd result.fds[0]
#define result_s2c_fd result.fds[1]

/* Task queue */
typedef struct {
    Task           *head;
    Task           *tail;
    size_t          count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty; // consumer
} TaskQueue;

extern TaskQueue TASK_Q;

// Task queue aliases
#define tq_head TASK_Q.head
#define tq_tail TASK_Q.tail
#define tq_count TASK_Q.count
#define tq_lock TASK_Q.lock
#define tq_not_empty TASK_Q.not_empty

void  task_init();
void  task_enqueue(TaskType type, //
                   pid_t    client_pid,
                   int      c2s_fd,
                   int      s2c_fd);
Task *task_dequeue();
void  task_unqueue(pid_t client_pid);
void  task_destroy();

#endif /* TASK_H */