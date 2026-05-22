#ifndef S_HELPER_H
#define S_HELPER_H

#include "client_registry.h"
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
    struct pollfd pfd;
    ActiveClient *client;
} PollSlot;

typedef struct {
    PollSlot *data;
    size_t    capacity;
    size_t    count;
} PollSlots;

extern PollSlots POLLSLOTS;
#define S_POLL_SLOTS POLLSLOTS.data
#define HSK_SLOT S_POLL_SLOTS[0]
#define RPC_SLOT S_POLL_SLOTS[1]
#define HSK_POLLFD HSK_SLOT.pfd
#define RPC_POLLFD RPC_SLOT.pfd
#define S_POLL_FD_CAPACITY POLLSLOTS.capacity
#define S_POLL_FD_COUNT POLLSLOTS.count

int  server_setup_pipes();
void server_destroy_selfpipes();
void destroy_client_pipes(pid_t client_pid, int c2s_fd, int s2c_fd);

int  server_grow_pollslots();
int  server_insert_pollslots(int fd, ActiveClient *Client);
void server_remove_pollslots(int fd);
void server_destroy_pollslots();

int server_setup_signals();

#define MAX_UTXT_LINEBUF_LEN 64

extern FILE *USERTXT;
int          usertxt_check();
void         usertxt_get_balance(const char *username, uint16_t len);
void         usertxt_close();

#endif /* S_HELPER_H */