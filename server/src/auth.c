#include "auth.h"
#include "client_registry.h"
#include "errors.h"
#include <ctype.h>
#include <dbg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ErrType usertxt_check()
{
    FILE *f = fopen(USERDB, "r");
    if (f == NULL) {
        perror("Failed to find user.txt");
        fflush(stderr);
        retval = USERTXT_FAIL;
        return USERTXT_FAIL;
    }
    fclose(f);
    return SUCCESS;
}

ResultType usertxt_get_balance(const char *username, long *balance)
{
    FILE *usertxt = fopen(USERDB, "r");
    if (usertxt == NULL) {
        return AUTH_UN;
    }

    size_t username_len = strlen(username);
    char   buffer[MAX_UTXT_LINEBUF_LEN];

    while (fgets(buffer, sizeof(buffer), usertxt) != NULL) {
        sanitise_whitespace(buffer);

        if (strncmp(username, buffer, username_len) != 0) {
            continue;
        }
        // substring
        if (!isspace((unsigned char)buffer[username_len])) {
            continue;
        }

        char *endptr;
        long  bal = strtol(buffer + username_len, &endptr, 10);
        if (endptr == buffer + username_len) { // malformed
            continue;
        }
        fclose(usertxt);
        *balance = bal;
        return bal > 0 ? AUTH_OK : AUTH_NO;
    }

    fclose(usertxt);
    return AUTH_UN;
}