#ifndef C_HELPER_H
#define C_HELPER_H

#include "errors.h"
#include <sys/types.h>

extern pid_t                 SERVER_PID;
extern volatile sig_atomic_t CONNECTION_STATE;
extern int                   C_FDS[2];

// fifo aliases
#define S_READ C_FDS[0]
#define C_WRITE C_FDS[1]

typedef enum {
    IDLE,
    SYN_SENT,
    CONNECTED,
    DISCONNECTED,
    TIMED_OUT
} ConnectStage;

int set_serverpid(int argc, char *argv[]);
int client_setup_signals();
int perform_handshake();

#endif /* C_HELPER_H */