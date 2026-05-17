#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    SUCCESS,
    TOO_FEW,
    TOO_MANY,
    TIMEDOUT,
    POOL_FAIL,
    INV_ARG,
    SIG_FAIL,
    TQ_FAIL,
    CR_FAIL,
    PIPE_FAIL,
    PIPE_READ_FAIL,
    PIPE_CLOSED,
    PIPE_PARTIAL,
    POLLFD_FAIL,
    GROW_FAIL
} ErrType;

extern ErrType retval;
extern char   *errs[];
extern char    s_usage[];
extern char    c_usage[];

#endif /* ERRORS_H */