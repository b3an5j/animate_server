#include "errors.h"

ErrType retval;

char *errs[] = {[TOO_FEW]   = "Too few arguments provided.\n",
                [TOO_MANY]  = "Too many arguments provided.\n",
                [INV_ARG]   = "Invalid argument provided.\n",
                [PIPE_FAIL] = "Faile to open pipes.\n",
                [POOL_FAIL] = "Failed to make pool of size %l.\n",
                [SIG_FAIL]  = "Failed to assign signal handlers.\n",
                [TIMEDOUT]  = "Server timed out.\n"};

char s_usage[] = "Usage: ./animate_server <threadpool size>\nSpecify a limit "
                 "of number of threads, of at least 1.\n";

char c_usage[] = "Usage: ./animate_client <server pid>\n";