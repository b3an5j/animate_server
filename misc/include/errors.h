#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    TOO_FEW,
    TOO_MANY,
    INV_ARG,
    PIPE_FAIL,
    POOL_FAIL,
    SIG_FAIL,
    TIMEDOUT
} ErrType;

extern ErrType retval;
extern char   *errs[];
extern char    s_usage[];
extern char    c_usage[];

#endif /* ERRORS_H */