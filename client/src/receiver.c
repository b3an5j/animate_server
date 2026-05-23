#include "receiver.h"
#include "c_helper.h"
#include "dbg.h"
#include "errors.h"
#include "fifo.h"
#include <stdio.h>

static void *receiver_thread(void *arg)
{
    (void)arg;
    char buf[MAX_RPC_BUF_LEN];

    while (1) {
        int alive = recv_server_reply(buf);
        if (!alive) {
            CONNECTION_STATE = DISCONNECTED;
            LOGGED_IN        = 0;
            break;
        }
    }
    debug_log("Receiver dead");
    return NULL;
}

int spawn_receiver(pthread_t *recv)
{
    return pthread_create(recv, NULL, receiver_thread, NULL);
}

int recv_server_reply(char *out)
{
    size_t out_len = 0;

    ErrType err =
        read_until_delim(S_READ, out, MAX_RPC_BUF_LEN, '\n', &out_len);

    if (err == PIPE_EMPTY) {
        return 1; // no data yet (nonblock)
    }

    if (err == PIPE_CLOSED || err == PIPE_READ_FAIL) {
        return 0; // disconnect
    }

    if (err != SUCCESS) {
        return 1; // ignore other
    }

    sanitise_whitespace(out);

    int a = 0, b = 0, c = 0;
    int fields = sscanf(out, "%d %d %d", &a, &b, &c);

    /* 1 FIELD */
    if (fields == 1) {
        switch (a) {
        case -1:
            printf("RPC Failed\n");
            break;
        case -2:
            printf("Value error\n");
            break;
        case -3:
            printf("Internal error\n");
            break;
        case 0:
            printf("Success\n");
            break;
        default:
            printf("%d\n", a);
            break;
        }
        fflush(stdout);
        return 1;
    }

    /* 2 FIELD */
    if (fields == 2) {
        if (a == 0 && b == -1) {
            printf("Data write failed\n");
        }
        else if (a == 0) {
            printf("Success %d\n", b);
        }
        else {
            printf("%d %d\n", a, b);
        }
        fflush(stdout);
        return 1;
    }

    /* 3 FIELD */
    if (fields == 3) {
        if (a == 0 && b == 0 && c == -1) {
            printf("Movie write failed\n");
        }
        else if (a == 0 && b == 0 && c == 0) {
            printf("Success\n");
        }
        else {
            printf("%d %d %d\n", a, b, c);
        }
        fflush(stdout);
        return 1;
    }

    return 1;
}
