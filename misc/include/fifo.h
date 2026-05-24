#ifndef FIFO_H
#define FIFO_H

#include "errors.h"
#include <sys/types.h>

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
    CREATE_CANVAS,
    DESTROY_CANVAS,
    CREATE_SPRITE,
    CREATE_RECTANGLE,
    CREATE_CIRCLE,
    DESTROY_SPRITE,
    PLACE_SPRITE,
    PLACEMENT_UP,
    PLACEMENT_DOWN,
    PLACEMENT_TOP,
    PLACEMENT_BOTTOM,
    DESTROY_PLACEMENT,
    SET_ANIM_PARAM,
    GENERATE,
    SHARE,
    BARRIER
} TaskType;

void set_name(char *buf, FifoType type, pid_t pid);
void sanitise_whitespace(char *str);

ErrType write_rpc_msg(int fd, const void *buf, size_t len);
ErrType read_rpc_msg(int fd, void *buf, size_t maxlen, size_t *out_len);

ErrType write_nonblock_pipe(int fd, const void *buf, size_t n_target);
ErrType write_block_pipe(int fd, const void *buf, size_t n_target);
ErrType read_nonblock_pipe(int fd, void *buf, size_t n_target);
ErrType read_until_delim(int     fd,
                         char   *buf,
                         size_t  maxlen,
                         char    delim,
                         size_t *out_len);

#endif /* FIFO_H */