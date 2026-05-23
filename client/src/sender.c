#include "sender.h"
#include "c_helper.h"
#include "dbg.h"
#include "errors.h"
#include "fifo.h"
#include <stdio.h>
#include <string.h>

int send_user_input(char *out)
{
    // Read a line from stdin
    if (!fgets(out, MAX_RPC_BUF_LEN, stdin)) {
        return -1; // break
    }

    sanitise_whitespace(out);
    if (out[0] == '\0') {
        return 1; // continue
    }

    debug_log("After sanitise: %s", out);

    // Detect Disconnect
    if (strcmp(out, "Disconnect") == 0) {
        const char *msg = "Disconnect\n";
        write_block_pipe(C_WRITE, msg, strlen(msg));
        return 0; // break
    }

    // Normal RPC
    char sendbuf[MAX_RPC_BUF_LEN];
    snprintf(sendbuf, sizeof(sendbuf), "%s\n", out);

    write_block_pipe(C_WRITE, sendbuf, strlen(sendbuf));

    return 1; // continue
}