#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "c_helper.h"

pid_t SERVER_PID;

int main(int argc, char **argv, char **envp)
{
    /* Init */
    if (set_serverpid(argc, argv) != 0) {
        return retval;
    }
    if (setup_signals() != 0) {
        return retval;
    }

    /* Connect */
    if (perform_handshake() != 0) {
        return retval;
    }
    return 0;
}