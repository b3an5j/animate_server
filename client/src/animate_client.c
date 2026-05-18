#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"

pid_t SERVER_PID;

int main(int argc, char **argv, char **envp)
{
    (void)envp;

    /* Start procedure */
    if (set_serverpid(argc, argv) != SUCCESS) {
        return retval;
    }
    if (client_setup_signals() != SUCCESS) {
        return retval;
    }

    /* Connect */
    if (perform_handshake() != SUCCESS) {
        return retval;
    }

    /* Send RPC requests */
    while (CONNECTION_STATE == CONNECTED) {
        continue;
    }

    /* Teardown */
    close(S_READ);
    close(C_WRITE);
    return 0;
}