#ifndef FIFO_H
#define FIFO_H

#include "errors.h"

#define FIFO_MAX_NAME 64
#define MAX_USERNAME_LEN 32
#define MAX_RPC_BUF_LEN 128

typedef enum {
    C2S,
    S2C
} FifoType;

typedef enum {
    HANDSHAKE,
    AUTHORISE,
    RPC,
    GOODBYE
} TaskType;

void    set_name(char *buf, FifoType type, pid_t pid);
void    sanitise_whitespace(char *str);
ErrType read_nonblock_pipe(int fd, void *buf, size_t n_target);
ErrType read_until_delim(int fd, void *buf, size_t maxlen, char delim);

#endif /* FIFO_H */