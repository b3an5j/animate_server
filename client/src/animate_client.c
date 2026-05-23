#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"
#include "dbg.h"
#include "fifo.h"
#include "sender.h"

ErrType               retval = SUCCESS;
pid_t                 SERVER_PID;
volatile sig_atomic_t CONNECTION_STATE = IDLE;
uint8_t               LOGGED_IN;
pthread_t             RECV;

int recv_started = 0;
int pipes_opened = 0;

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
    pipes_opened = 1;

    authorise();
    if (!LOGGED_IN) {
        goto teardown;
    }

    if (spawn_receiver(&RECV) != SUCCESS) {
        goto teardown;
    }
    recv_started = 1;
    debug_log("Receiver created");

    /* Send RPC requests */
    while (CONNECTION_STATE == CONNECTED && LOGGED_IN) {

        char input[MAX_RPC_BUF_LEN];

        int send = send_user_input(input);
        if (send == -1) { // EOF or error
            CONNECTION_STATE = DISCONNECTED;
            LOGGED_IN        = 0;
            break;
        }
        if (send == 0) { // Disconnect
            CONNECTION_STATE = DISCONNECTED;
            LOGGED_IN        = 0;
            break;
        }
    }

teardown:
    /* Teardown */
    if (pipes_opened) {
        close(S_READ);
        close(C_WRITE);

        if (recv_started) {
            pthread_join(RECV, NULL);
        }
    }
    return retval ? retval : 0;
}