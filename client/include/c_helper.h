#ifndef C_HELPER_H
#define C_HELPER_H

#include "errors.h"
#include <sys/types.h>

extern pid_t                 SERVER_PID;
extern volatile sig_atomic_t CONNECTION_STATE;
extern volatile sig_atomic_t TIMED_OUT;

typedef enum {
    IDLE,
    SYN_SENT,
    CONNECTED
} ConnectStage;

int set_serverpid(int argc, char *argv[]);
int client_setup_signals();
int perform_handshake();

#endif /* C_HELPER_H */