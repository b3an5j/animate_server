#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"
#include "dbg.h"
#include "fifo.h"

ErrType               retval = SUCCESS;
pid_t                 SERVER_PID;
volatile sig_atomic_t CONNECTION_STATE = IDLE;
uint8_t               LOGGED_IN;

int main(int argc, char **argv, char **envp)
{
    (void)envp;
    EN_DEBUG  = 1;
    LOGGED_IN = 0;

    /* Start procedure */
    if (set_serverpid(argc, argv) != SUCCESS) {
        goto teardown;
    }
    if (client_setup_signals() != SUCCESS) {
        goto teardown;
    }

    /* Connect */
    if (perform_handshake() != SUCCESS) {
        goto teardown;
    }

    /* Send RPC requests */
    authorise();
    while (CONNECTION_STATE == CONNECTED && LOGGED_IN) {
        char input[MAX_RPC_BUF_LEN];
        int  ret = get_user_input(input);

        if (ret == -1) { // EOF or error
            CONNECTION_STATE = DISCONNECTED;
            LOGGED_IN        = 0;
            break;
        }
        if (ret == 0) { // Disconnect
            CONNECTION_STATE = DISCONNECTED;
            LOGGED_IN        = 0;
            break;
        }

        // normal RPC
    }

teardown:
    /* Teardown */
    close(S_READ);
    close(C_WRITE);
    return 0;
}