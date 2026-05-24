#include "commands.h"
#include "auth.h"
#include "dbg.h"
#include "task.h"
#include <stdio.h>
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
    if (strncmp(cmd, "create_canvas ", 14) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 14;
        task_enqueue(CREATE_CANVAS, 0, args, client);
    }
    if (strncmp(cmd, "destroy_canvas ", 15) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 15;
        task_enqueue(DESTROY_CANVAS, 0, args, client);
    }
    if (strncmp(cmd, "create_sprite ", 14) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 14;
        task_enqueue(CREATE_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "create_rectangle ", 17) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 17;
        task_enqueue(CREATE_RECTANGLE, 0, args, client);
    }
    if (strncmp(cmd, "create_circle ", 14) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 14;
        task_enqueue(CREATE_CIRCLE, 0, args, client);
    }
    if (strncmp(cmd, "destroy_sprite ", 15) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 15;
        task_enqueue(DESTROY_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "place_sprite ", 13) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 13;
        task_enqueue(PLACE_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "placement_up ", 13) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 13;
        task_enqueue(PLACEMENT_UP, 0, args, client);
    }
    if (strncmp(cmd, "placement_down ", 15) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 15;
        task_enqueue(PLACEMENT_DOWN, 0, args, client);
    }
    if (strncmp(cmd, "placement_top ", 14) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 14;
        task_enqueue(PLACEMENT_TOP, 0, args, client);
    }
    if (strncmp(cmd, "placement_bottom ", 17) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 17;
        task_enqueue(PLACEMENT_BOTTOM, 0, args, client);
    }
    if (strncmp(cmd, "destroy_placement ", 18) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 18;
        task_enqueue(DESTROY_PLACEMENT, 0, args, client);
    }
    if (strncmp(cmd, "set_animation_params ", 21) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 21;
        task_enqueue(SET_ANIM_PARAM, 0, args, client);
    }
    if (strncmp(cmd, "generate ", 9) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 9;
        task_enqueue(GENERATE, 0, args, client);
    }
    if (strncmp(cmd, "share_canvas ", 13) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 13;
        task_enqueue(SHARE, 0, args, client);
    }
    if (strncmp(cmd, "barrier ", 8) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 8;
        task_enqueue(BARRIER, 0, args, client);
    }

    /* Unkown commands */
    return 0;
}
