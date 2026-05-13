#ifndef S_HELPER_H
#define S_HELPER_H

#include "errors.h"

extern long                  THREADPOOL_SIZE;
extern volatile sig_atomic_t RUNNING;

int get_threadpool_size(int argc, char **argv);
int setup_signals();

#endif /* S_HELPER_H */