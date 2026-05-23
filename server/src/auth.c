#include "auth.h"
#include "client_registry.h"
#include "errors.h"
#include <ctype.h>
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

static DisconnectEvent *disc_head = NULL;

void schedule_disconnect(ActiveClient *client)
{
    DisconnectEvent *dc = malloc(sizeof(*dc));
    dc->client          = client;

    gettimeofday(&dc->when, NULL);
    dc->when.tv_usec += 500000;
    if (dc->when.tv_usec >= 1000000) {
        dc->when.tv_sec += 1;
        dc->when.tv_usec -= 1000000;
    }

    dc->next  = disc_head;
    disc_head = dc;
}

void process_disconnect_events()
{
    struct timeval now;
    gettimeofday(&now, NULL);

    DisconnectEvent **pp = &disc_head;

    while (*pp) {
        DisconnectEvent *dc = *pp;

        if (now.tv_sec > dc->when.tv_sec || (now.tv_sec == dc->when.tv_sec &&
                                             now.tv_usec >= dc->when.tv_usec)) {
            *pp = dc->next;
            creg_remove(dc->client);
            free(dc);
            continue;
        }
        pp = &dc->next;
    }
}

void destroy_all_disconnect_events()
{
    DisconnectEvent *dc = disc_head;

    while (dc) {
        DisconnectEvent *next = dc->next;
        creg_remove(dc->client);
        free(dc);
        dc = next;
    }

    disc_head = NULL;
}
