#ifndef TASK_H
#define TASK_H

#define TASK_MAX_N 1024

extern volatile sig_atomic_t RUNNING;

typedef enum {
    HANDSHAKE,
    RPC
} TaskType;

typedef struct {
    TaskType type;
    union {
        struct {
            pid_t client_pid;
        } hsk;
        struct {
            int c2s_fd;
            int s2c_fd;
        } rpc;
    } info;
    ErrType retval;
} Task;

// Task aliases
#define task_client_pid info.hsk.client_pid
#define task_c2s_fd info.rpc.c2s_fd
#define task_s2c_fd info.rpc.s2c_fd

typedef struct {
    Task           *tasks[TASK_MAX_N];
    size_t          head;
    size_t          tail;
    size_t          count;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;  // producer
    pthread_cond_t  not_empty; // consumer
} TaskQueue;

extern TaskQueue TASK_Q;

// Task queue aliases
#define tq_tasks TASK_Q.tasks
#define tq_head TASK_Q.head
#define tq_tail TASK_Q.tail
#define tq_count TASK_Q.count
#define tq_lock TASK_Q.lock
#define tq_not_full TASK_Q.not_full
#define tq_not_empty TASK_Q.not_empty

void  task_init();
Task *task_make(pid_t client_pid);
void  task_enqueue(Task *task);
Task *task_dequeue();
void  task_unqueue(pid_t client_pid);
void  task_destroy();

#endif /* TASK_H */