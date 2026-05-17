#include "errors.h"

ErrType retval;

char *errs[] = {[TOO_FEW]        = "Too few arguments provided.\n",
                [TOO_MANY]       = "Too many arguments provided.\n",
                [INV_ARG]        = "Invalid argument provided.\n",
                [PIPE_FAIL]      = "Faile to open pipes.\n",
                [POOL_FAIL]      = "Failed to make pool of size %l.\n",
                [SIG_FAIL]       = "Failed to assign signal handlers.\n",
                [TQ_FAIL]        = "Failed to allocate task.\n",
                [POLLFD_FAIL]    = "Failed to allocate poll fd array.\n",
                [CR_FAIL]        = "Failed to insert user to registry.\n",
                [TIMEDOUT]       = "Server timed out.\n",
                [PIPE_READ_FAIL] = "Failed to read from pipe\n",
                [PIPE_CLOSED]    = "%s pipe EOF detected.\n",
                [PIPE_PARTIAL]   = "%s pipe partial read.\n",
                [GROW_FAIL]      = "Failed to grow poll fd array.\n"};

char s_usage[] = "Usage: ./animate_server <threadpool size>\nSpecify a limit "
                 "of number of threads, of at least 1.\n";

char c_usage[] = "Usage: ./animate_client <server pid>\n";