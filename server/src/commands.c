#include "commands.h"
#include "auth.h"
#include "task.h"
#include <string.h>

int handle_command(ActiveClient *client, const char *cmd)
{
    // return 0 to not remove, else 1

    /* LOGIN */
    if (strncmp(cmd, "Login ", 6) == 0) {

        if (client->logged_in) {
            // already logged in
            return 0;
        }

        const char *username = cmd + 6;

        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        task_enqueue(AUTHORISE, 0, username, client);
        return 0;
    }

    /* DISCONNECT */
    if (strncmp(cmd, "Disconnect", 10) == 0) {
        return 1;
    }

    /* OTHER RPC */
    return 0;
}
