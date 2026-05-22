#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    SUCCESS,
    FAIL,
    TOO_FEW,
    TOO_MANY,
    USERTXT_FAIL,
    TIMEDOUT,
    POOL_FAIL,
    INV_ARG,
    SIG_FAIL,
    TQ_FAIL,
    CR_FAIL,
    PIPE_FAIL,
    PIPE_READ_FAIL,
    PIPE_WRITE_FAIL,
    PIPE_EMPTY,
    PIPE_FULL,
    PIPE_CLOSED,
    PIPE_PARTIAL,
    POLLFD_FAIL,
    GROW_FAIL
} ErrType;

extern ErrType     retval;
extern const char *errs[];
extern const char  s_usage[];
extern const char  c_usage[];

#endif /* ERRORS_H */