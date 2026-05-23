#ifndef AUTH_H
#define AUTH_H

#include "errors.h"
#include "task.h"
#include <sys/time.h>

#define USERDB "users.txt"
#define MAX_UTXT_LINEBUF_LEN 64

#define MSG_REJECT_BALANCE "Reject BALANCE\n"
#define MSG_REJECT_UNAUTHORISED "Reject UNAUTHORISED\n"

typedef struct DisconnectEvent {
    ActiveClient           *client;
    struct timeval          when;
    struct DisconnectEvent *next;
} DisconnectEvent;

ErrType    usertxt_check();
ResultType usertxt_get_balance(const char *username, long *balance);

void schedule_disconnect(ActiveClient *client);
void process_disconnect_events();
void destroy_all_disconnect_events();

#endif /* AUTH_H */