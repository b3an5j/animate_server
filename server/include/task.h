#ifndef TASK_H
#define TASK_H

#include "client_registry.h"
#include "errors.h"
#include "fifo.h"
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>

extern volatile sig_atomic_t RUNNING;

/* Task */
typedef struct Task {
    TaskType type;
    union {
        pid_t         client_pid;
        ActiveClient *client;
    } info;
    union {
        struct {
            char c2s_name[FIFO_MAX_NAME];
            char s2c_name[FIFO_MAX_NAME];
        } fifo;
        struct {
            char request[MAX_RPC_BUF_LEN];
            int  s2c_fd;
        } rpc;
    } data;
    struct Task *next;
} Task;

// Task aliases
#define task_client info.client
#define task_rawpid info.client_pid
#define task_c2s_name data.fifo.c2s_name
#define task_s2c_name data.fifo.s2c_name
#define task_s2c_fd data.rpc.s2c_fd
#define task_request data.rpc.request

/* Task result */
typedef enum {
    HSK_OK,
    AUTH_OK,
    AUTH_NO,
    AUTH_UN,
    RPC_DONE
} ResultType;

typedef struct {
    ResultType type;
    union {
        pid_t         pid;
        ActiveClient *ptr;
    } client;
    union {
        int fds[2];
        struct {
            char username[MAX_USERNAME_LEN + 1];
            long balance;
        } auth;
        struct {
            int a;
            int b;
            int c;
        } rpc;
    } result;
} TaskResult;

// Task result aliases
#define result_rawpid client.pid
#define result_client client.ptr
#define result_c2s_fd result.fds[0]
#define result_s2c_fd result.fds[1]
#define result_username result.auth.username
#define result_balance result.auth.balance
#define result_retval result.rpc

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

void    task_init();
ErrType task_enqueue(TaskType      type, //
                     pid_t         client_pid,
                     const char   *request,
                     ActiveClient *client);
Task   *task_dequeue();
void    task_destroy();

#endif /* TASK_H */