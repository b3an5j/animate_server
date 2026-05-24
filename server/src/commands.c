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
    if (strncmp(cmd, "animate_create_canvas ", 22) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 22;
        task_enqueue(CREATE_CANVAS, 0, args, client);
    }
    if (strncmp(cmd, "animate_destroy_canvas ", 23) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 23;
        task_enqueue(DESTROY_CANVAS, 0, args, client);
    }
    if (strncmp(cmd, "animate_create_sprite ", 22) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 22;
        task_enqueue(CREATE_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "animate_create_rectangle ", 25) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 25;
        task_enqueue(CREATE_RECTANGLE, 0, args, client);
    }
    if (strncmp(cmd, "animate_create_circle ", 22) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 22;
        task_enqueue(CREATE_CIRCLE, 0, args, client);
    }
    if (strncmp(cmd, "animate_destroy_sprite ", 23) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 23;
        task_enqueue(DESTROY_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "animate_place_sprite ", 21) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 21;
        task_enqueue(PLACE_SPRITE, 0, args, client);
    }
    if (strncmp(cmd, "animate_placement_up ", 21) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 21;
        task_enqueue(PLACEMENT_UP, 0, args, client);
    }
    if (strncmp(cmd, "animate_placement_down ", 23) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 23;
        task_enqueue(PLACEMENT_DOWN, 0, args, client);
    }
    if (strncmp(cmd, "animate_placement_top ", 22) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 22;
        task_enqueue(PLACEMENT_TOP, 0, args, client);
    }
    if (strncmp(cmd, "animate_placement_bottom ", 25) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 25;
        task_enqueue(PLACEMENT_BOTTOM, 0, args, client);
    }
    if (strncmp(cmd, "animate_destroy_placement ", 26) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 26;
        task_enqueue(DESTROY_PLACEMENT, 0, args, client);
    }
    if (strncmp(cmd, "animate_set_animation_params ", 30) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 30;
        task_enqueue(SET_ANIM_PARAM, 0, args, client);
    }
    if (strncmp(cmd, "generate ", 10) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 10;
        task_enqueue(GENERATE, 0, args, client);
    }
    if (strncmp(cmd, "share_canvas ", 14) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 14;
        task_enqueue(SHARE, 0, args, client);
    }
    if (strncmp(cmd, "barrier ", 9) == 0) {
        if (creg_acquire_client(client) == FAIL) {
            return 1;
        }
        const char *args = cmd + 9;
        task_enqueue(BARRIER, 0, args, client);
    }

    /* Unkown commands */
    return 0;
}
