#ifndef AUTH_H
#define AUTH_H

#include "errors.h"
#include "task.h"

#define USERDB "users.txt"
#define MAX_UTXT_LINEBUF_LEN 64

#define MSG_REJECT_BALANCE "Reject BALANCE\n"
#define MSG_REJECT_UNAUTHORISED "Reject UNAUTHORISED\n"

ErrType    usertxt_check();
ResultType usertxt_get_balance(const char *username, long *balance);

#endif /* AUTH_H */