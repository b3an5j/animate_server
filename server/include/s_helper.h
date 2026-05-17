#ifndef S_HELPER_H
#define S_HELPER_H

#include "errors.h"
#include <poll.h>
#include <sys/types.h>

#define POLLFD_INIT_N 32

extern volatile sig_atomic_t RUNNING;

extern int HANDSHAKE_PIPE[2];
#define HSK_R HANDSHAKE_PIPE[0]
#define HSK_W HANDSHAKE_PIPE[1]

extern int RPC_PIPE[2];
#define RPC_R RPC_PIPE[0]
#define RPC_W RPC_PIPE[1]

typedef struct {
    struct pollfd *pollfds;
    size_t         capacity;
    size_t         count;
} PollFds;

extern volatile PollFds POLLFDS;
#define S_POLL_FDS POLLFDS.pollfds
#define HSK_POLLFD S_POLL_FDS[0]
#define RPC_POLLFD S_POLL_FDS[1]
#define S_POLL_FD_CAPACITY POLLFDS.capacity
#define S_POLL_FD_COUNT POLLFDS.count

int  server_setup_pipes();
void server_destroy_pipes();

int  server_grow_pollfd();
int  server_insert_pollfd(int fd);
void server_remove_pollfd(int fd);

int server_setup_signals();

#endif /* S_HELPER_H */