#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <sys/types.h>

extern volatile sig_atomic_t RUNNING;

typedef struct {
    long       size;
    pthread_t *threads;
} ThreadPool;

extern ThreadPool THREADPOOL;
#define TPOOL_SIZE THREADPOOL.size
#define tpool_threads THREADPOOL.threads

int  get_threadpool_size(int argc, char **argv);
int  threadpool_init();
void threadpool_destroy();

#endif /* THREADPOOL_H */