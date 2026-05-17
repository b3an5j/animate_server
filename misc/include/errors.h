#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    TOO_FEW,
    TOO_MANY,
    INV_ARG,
    PIPE_FAIL,
    POOL_FAIL,
    SIG_FAIL,
    TQ_FAIL,
    POLLFD_FAIL,
    CR_FAIL,
    TIMEDOUT,
    PIPE_READ_FAIL,
    PIPE_CLOSED,
    PIPE_PARTIAL,
    GROW_FAIL
} ErrType;

extern ErrType retval;
extern char   *errs[];
extern char    s_usage[];
extern char    c_usage[];

#endif /* ERRORS_H */