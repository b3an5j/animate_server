#ifndef S_HELPER_H
#define S_HELPER_H

#include "errors.h"

/* SIGNALING */
extern volatile sig_atomic_t RUNNING;

extern int HANDSHAKE_PIPE[2];
#define HSK_R HANDSHAKE_PIPE[0]
#define HSK_W HANDSHAKE_PIPE[1]

extern int RPC_PIPE[2];
#define RPC_R RPC_PIPE[0]
#define RPC_W RPC_PIPE[1]

int  server_setup_pipes();
void server_destroy_pipes();
int  server_setup_signals();

/* FIFO */
#define FIFO_MAX_NAME 64

typedef enum {
    C2S,
    S2C
} FifoType;

void set_name(const char *buf, FifoType type, pid_t pid);

#endif /* S_HELPER_H */