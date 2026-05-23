#include "errors.h"

const char *errs[] = {[TOO_FEW]        = "Too few arguments provided.\n",
                      [TOO_MANY]       = "Too many arguments provided.\n",
                      [INV_ARG]        = "Invalid argument provided.\n",
                      [TIMEDOUT]       = "Server timed out.\n",
                      [POOL_FAIL]      = "Failed to make pool size",
                      [SIG_FAIL]       = "Failed to assign signal handlers.\n",
                      [TQ_FAIL]        = "Failed to allocate task.\n",
                      [CR_FAIL]        = "Failed to insert user to registry.\n",
                      [PIPE_FAIL]      = "Failed to open pipes for",
                      [PIPE_READ_FAIL] = "Failed to read from pipe",
                      [PIPE_WRITE_FAIL] = "Failed to write from pipe",
                      [PIPE_CLOSED]     = "pipe EOF detected.\n",
                      [PIPE_PARTIAL]    = "pipe partial read.\n",
                      [POLLFD_FAIL]     = "Failed to allocate poll fd array.\n",
                      [GROW_FAIL]       = "Failed to grow poll fd array.\n"};

const char s_usage[] = "Usage: ./animate_server <threadpool size>\nSpecify a "
                       "limit of number of threads, of at least 1.\n";

const char c_usage[] = "Usage: ./animate_client <server pid>\n";