#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// #include "animate.h"
#include <animate/animate.h>

#include "auth.h"
#include "dbg.h"
#include "fifo.h"
#include "object_table.h"
#include "s_helper.h"
#include "task.h"
#include "threadpool.h"

#define MAX_OPEN_RETRIES 10
#define MAX_PATH_LEN 256
#define MAX_FILENAME_LEN 128
#define COMMAND_LEN 1024
#define OPEN_WAIT_TIME 10000000 // 10 ms

ThreadPool THREADPOOL;

int get_threadpool_size(int argc, char **argv)
{
    if (argc == 1) {
        fprintf(stderr, "%s\n%s", errs[TOO_FEW], s_usage);
        fflush(stderr);
        retval = TOO_FEW;
        return TOO_FEW;
    }
    if (argc > 2) {
        fprintf(stderr, "%s\n%s", errs[TOO_MANY], s_usage);
        fflush(stderr);
        retval = TOO_MANY;
        return TOO_MANY;
    }

    TPOOL_SIZE = atol(argv[1]);
    if (TPOOL_SIZE <= 0) {
        fprintf(stderr, "%s %ld.\n\n%s", errs[POOL_FAIL], TPOOL_SIZE, s_usage);
        fflush(stderr);
        retval = POOL_FAIL;
        return POOL_FAIL;
    }
    else if (TPOOL_SIZE > 0) {
        return SUCCESS;
    }
    fprintf(stderr, "%s\n%s", errs[INV_ARG], s_usage);
    fflush(stderr);
    retval = INV_ARG;
    return INV_ARG;
}

static int parse_int(const char *s, long *out)
{
    // 1 success
    char *end;
    long  v = strtol(s, &end, 10);
    if (*end != '\0')
        return 0;
    *out = v;
    return 1;
}

static int parse_uint(const char *s, unsigned long *out)
{
    // 1 success
    char         *end;
    unsigned long v = strtoul(s, &end, 10);
    if (*end != '\0')
        return 0;
    *out = v;
    return 1;
}

static void *worker_routine(void *)
{
    while (RUNNING) {
        Task *task = task_dequeue();
        if (!task)
            continue;

        switch (task->type) {
        case HANDSHAKE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            pid_t client_pid         = task->task_rawpid;
            taskresult.result_rawpid = client_pid;

            // unlink first
            unlink(task->task_c2s_name);
            unlink(task->task_s2c_name);

            // make fifo
            mode_t  old_umask = umask(0);
            uint8_t fifo_fail = mkfifo(task->task_c2s_name, 0666) != 0 ||
                                mkfifo(task->task_s2c_name, 0666) != 0;
            umask(old_umask);
            if (fifo_fail) {
                goto hsk_fail;
            }

            // signal client
            if (kill(client_pid, SIGUSR2) == -1) {
                goto hsk_fail;
            }

            // open read fifo
            int r_temp = open(task->task_c2s_name, O_NONBLOCK | O_RDONLY);
            if (r_temp == -1) {
                goto hsk_fail;
            }
            taskresult.result_c2s_fd = r_temp;

            // open write fifo
            int w_temp = open(task->task_s2c_name, O_NONBLOCK | O_WRONLY);
            if (w_temp == -1) {
                if (errno == ENXIO) { // give chance
                    struct timespec t_temp = {0};
                    t_temp.tv_nsec         = OPEN_WAIT_TIME;

                    for (int attempts = 0; attempts < MAX_OPEN_RETRIES;
                         ++attempts) {
                        w_temp =
                            open(task->task_s2c_name, O_NONBLOCK | O_WRONLY);

                        if (w_temp != -1)
                            break;

                        if (errno == ENXIO)
                            nanosleep(&t_temp, NULL);
                        else // other err
                            break;
                    }
                }

                if (w_temp == -1) {
                    close(taskresult.result_c2s_fd);
                    goto hsk_fail;
                }
            }
            taskresult.result_s2c_fd = w_temp;

            taskresult.type = HSK_OK;
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;

        hsk_fail:
            unlink(task->task_c2s_name);
            unlink(task->task_s2c_name);

            fprintf(stderr, "%s %d.\n", errs[PIPE_FAIL], client_pid);
            fflush(stderr);
            // ignore and drop the handshake
            free(task);
            continue;
        }

        case AUTHORISE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            ActiveClient *client     = task->task_client;
            taskresult.result_client = client;

            long       balance;
            ResultType ret  = usertxt_get_balance(task->task_request, &balance);
            taskresult.type = ret;
            if (ret == AUTH_OK) {
                taskresult.result_balance = balance;
                strncpy(taskresult.result_username,
                        task->task_request,
                        MAX_USERNAME_LEN - 1);
                taskresult.result_username[MAX_USERNAME_LEN - 1] = '\0';
            }

            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case CREATE_CANVAS: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[MAX_RPC_BUF_LEN];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *h_s  = strtok_r(save, " ", &save);
            char *w_s  = strtok_r(NULL, " ", &save);
            char *c_s  = strtok_r(NULL, " ", &save);

            if (!h_s || !w_s || !c_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            unsigned long h, w;
            long          bg;
            if (!parse_uint(h_s, &h) || !parse_uint(w_s, &w) ||
                !parse_int(c_s, &bg)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // create
            struct canvas *cv = animate_create_canvas(h, w, (color_t)bg);
            if (!cv) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            int id = canvas_insert(cv, task->task_client, w, h);
            if (id < 0) {
                animate_destroy_canvas(cv);
                taskresult.result_retval.a = -3; // grow fail
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = id;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Created canvas");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case DESTROY_CANVAS: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[MAX_RPC_BUF_LEN];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            CanvasEntry *ce = canvas_lookup(id);
            if (!ce || !ce->ptr) { // rpc fail
                debug_log("Not found: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // remove
            canvas_remove(id);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Destroyed canvas");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case CREATE_SPRITE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[MAX_RPC_BUF_LEN];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *file = strtok_r(save, " ", &save);
            if (!file) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // create
            struct sprite *sp = animate_create_sprite(file);
            if (!sp) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            int id = sprite_insert(sp, task->task_client);
            if (id < 0) {
                animate_destroy_sprite(sp);
                taskresult.result_retval.a = -3; // grow fail
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = id;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Created sprite");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case CREATE_RECTANGLE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[128];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *w_s  = strtok_r(save, " ", &save);
            char *h_s  = strtok_r(NULL, " ", &save);
            char *c_s  = strtok_r(NULL, " ", &save);
            char *f_s  = strtok_r(NULL, " ", &save);

            if (!w_s || !h_s || !c_s || !f_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            unsigned long w, h;
            long          color, filled;
            if (!parse_uint(w_s, &w) || !parse_uint(h_s, &h) ||
                !parse_int(c_s, &color) || !parse_int(f_s, &filled)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // create
            struct sprite *sp =
                animate_create_rectangle(w, h, (color_t)color, filled != 0);
            if (!sp) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            int id = sprite_insert(sp, task->task_client);
            if (id < 0) {
                animate_destroy_sprite(sp);
                taskresult.result_retval.a = -3; // grow fail
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = id;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Created rectangle");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case CREATE_CIRCLE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[128];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *r_s  = strtok_r(save, " ", &save);
            char *c_s  = strtok_r(NULL, " ", &save);
            char *f_s  = strtok_r(NULL, " ", &save);

            if (!r_s || !c_s || !f_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            unsigned long radius;
            long          color, filled;
            if (!parse_uint(r_s, &radius) || !parse_int(c_s, &color) ||
                !parse_int(f_s, &filled)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // create
            struct sprite *sp =
                animate_create_circle(radius, (color_t)color, filled != 0);
            if (!sp) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            int id = sprite_insert(sp, task->task_client);
            if (id < 0) {
                animate_destroy_sprite(sp);
                taskresult.result_retval.a = -3; // grow fail
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = id;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Created circle");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case DESTROY_SPRITE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            SpriteEntry *se = sprite_lookup((int)id);
            if (!se || !se->ptr) { // rpc fail
                debug_log("Not found: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // remove
            int ret = sprite_remove(id);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = ret;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Destroyed sprite");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case PLACE_SPRITE: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[128];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *c_s  = strtok_r(save, " ", &save);
            char *s_s  = strtok_r(NULL, " ", &save);
            char *x_s  = strtok_r(NULL, " ", &save);
            char *y_s  = strtok_r(NULL, " ", &save);

            if (!c_s || !s_s || !x_s || !y_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long cid, sid, x, y;
            if (!parse_int(c_s, &cid) || !parse_int(s_s, &sid) ||
                !parse_int(x_s, &x) || !parse_int(y_s, &y)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            CanvasEntry *ce = canvas_lookup(cid);
            SpriteEntry *se = sprite_lookup(sid);
            if (!ce || !ce->ptr || !se || !se->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // place
            struct sprite_placement *pl =
                animate_place_sprite(ce->ptr, se->ptr, x, y);
            if (!pl) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // insert
            int id = placement_insert(pl, task->task_client, ce);
            if (id < 0) {
                animate_destroy_placement(pl);
                taskresult.result_retval.a = -3; // grow fail
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = id;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Placed sprite");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case PLACEMENT_UP: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            // extract request
            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // not found
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // place
            animate_placement_up(pe->ptr);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Placed up");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case PLACEMENT_DOWN: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // not found
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // place
            animate_placement_down(pe->ptr);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Placed down");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case PLACEMENT_TOP: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            // extract request
            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // not found
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // place
            animate_placement_top(pe->ptr);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Placed top");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case PLACEMENT_BOTTOM: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // not found
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // place
            animate_placement_bottom(pe->ptr);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Placed bottom");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case DESTROY_PLACEMENT: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[64];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            if (!id_s) {
                debug_log("Malformed: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id;
            if (!parse_int(id_s, &id) || id < 0 || id > INT_MAX) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // remove
            placement_remove(id);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Destroyed placement");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case SET_ANIM_PARAM: {
            TaskResult taskresult;
            memset(
                &taskresult, 0, sizeof(TaskResult)); // to make valgrind happy
            taskresult.type          = RPC_DONE;
            taskresult.result_client = task->task_client;

            // default
            taskresult.result_retval.a = -1;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            // extract request
            char buf[128];
            strncpy(buf, task->task_request, sizeof(buf));
            buf[sizeof(buf) - 1] = '\0';

            char *save = buf;
            char *id_s = strtok_r(save, " ", &save);
            char *vx_s = strtok_r(NULL, " ", &save);
            char *vy_s = strtok_r(NULL, " ", &save);
            char *ax_s = strtok_r(NULL, " ", &save);
            char *ay_s = strtok_r(NULL, " ", &save);

            if (!id_s || !vx_s || !vy_s || !ax_s || !ay_s) {
                debug_log("Inv arg: %s", buf);
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // parse
            long id, vx, vy, ax, ay;
            if (!parse_int(id_s, &id) || !parse_int(vx_s, &vx) ||
                !parse_int(vy_s, &vy) || !parse_int(ax_s, &ax) ||
                !parse_int(ay_s, &ay)) {
                debug_log("Inv arg: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // lookup
            PlacementEntry *pe = placement_lookup(id);
            if (!pe || !pe->ptr) {
                debug_log("Not found: %s", buf);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // set
            animate_set_animation_params(pe->ptr, vx, vy, ax, ay);

            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            debug_log("Set param");
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
            break;
        }

        case GENERATE: {
            ActiveClient *client = task->task_client;
            const char   *args   = task->task_request;

            TaskResult taskresult;
            memset(&taskresult, 0, sizeof(taskresult));
            taskresult.type          = RPC_DONE;
            taskresult.result_client = client;

            // default
            taskresult.result_retval.a = INT_MIN;
            taskresult.result_retval.b = INT_MIN;
            taskresult.result_retval.c = INT_MIN;

            /* Parse */
            int  canvas_id, start, end, rate;
            char filename[MAX_FILENAME_LEN];

            char fmt[64];
            snprintf(fmt,
                     sizeof(fmt),
                     "%%d %%%ds %%d %%d %%d",
                     MAX_FILENAME_LEN - 1);

            if (sscanf(args, fmt, &canvas_id, filename, &start, &end, &rate) !=
                    5 ||
                start < 0 || end < start || rate <= 0) {
                debug_log("Malform: %s", args);
                taskresult.result_retval.a = -2; // value error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            /* Lookup canvas */
            CanvasEntry *ce = canvas_lookup(canvas_id);
            if (!ce || !ce->ptr) {
                debug_log("Not found: %s", args);
                taskresult.result_retval.a = -2; // not found
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }
            struct canvas *cv = ce->ptr;

            /* Open <filename>.dat */
            char dat_path[MAX_PATH_LEN];
            snprintf(dat_path, sizeof(dat_path), "%s.dat", filename);

            int dat_fd = open(dat_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (dat_fd < 0) {
                taskresult.result_retval.a = 0;
                taskresult.result_retval.b = -1; // Data write failed
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            /* Frame size */
            size_t frame_sz = animate_frame_size_bytes(cv);
            if (frame_sz == 0 || frame_sz != (size_t)(ce->height * ce->width)) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                close(dat_fd);
                break;
            }

            void *frame_buf = malloc(frame_sz);
            if (!frame_buf) {
                taskresult.result_retval.a = -3; // internal error
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                close(dat_fd);
                break;
            }

            /* Generate and write frames */
            for (int i = start; i <= end; i++) {
                animate_generate_frame(cv, (size_t)i, (size_t)rate, frame_buf);

                ssize_t n = write(dat_fd, frame_buf, frame_sz);
                if (n != (ssize_t)frame_sz) {
                    taskresult.result_retval.a = 0;
                    taskresult.result_retval.b = -1; // Data write failed
                    write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                    free(frame_buf);
                    close(dat_fd);
                    goto done_generate;
                }
            }

            free(frame_buf);

            if (close(dat_fd) < 0) {
                taskresult.result_retval.a = 0;
                taskresult.result_retval.b = -1; // Data write failed
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            /* ffmpeg mp4 */
            char mp4_path[MAX_PATH_LEN];
            snprintf(mp4_path, sizeof(mp4_path), "%s.mp4", filename);

            char log_path[MAX_PATH_LEN];
            snprintf(log_path, sizeof(log_path), "%s.log", filename);

            char cmd[COMMAND_LEN];
            snprintf(cmd,
                     sizeof(cmd),
                     "ffmpeg -y -f rawvideo -pixel_format rgba "
                     "-video_size %dx%d -framerate %d "
                     "-i %s %s > %s 2>&1",
                     ce->width,
                     ce->height,
                     rate,
                     dat_path,
                     mp4_path,
                     log_path);

            int rc = system(cmd);
            if (rc != 0) {
                taskresult.result_retval.a = 0;
                taskresult.result_retval.b = 0;
                taskresult.result_retval.c = -1; // Movie write failed
                write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));
                break;
            }

            // success
            taskresult.result_retval.a = 0;
            taskresult.result_retval.b = 0;
            taskresult.result_retval.c = 0;
            write_block_pipe(RPC_W, &taskresult, sizeof(taskresult));

        done_generate:
            break;
        }

        case SHARE:
        case BARRIER:

        default:
            break;
        }

        free(task);
    }

    debug_log("dead");
    return NULL;
}

int threadpool_init()
{
    pthread_t *temp;
    temp = malloc(sizeof(*tpool_threads) * TPOOL_SIZE);
    if (temp == NULL) {
        fprintf(stderr, errs[POOL_FAIL], TPOOL_SIZE);
        fflush(stderr);
        return POOL_FAIL;
    }
    tpool_threads = temp;

    // assert non negative

    size_t created = 0;
    for (size_t i = 0; i < (size_t)TPOOL_SIZE; ++i) {
        if (pthread_create(&tpool_threads[i], NULL, worker_routine, NULL) !=
            0) {
            RUNNING = 0;
            pthread_cond_broadcast(&tq_not_empty); // wake up

            // reap
            for (size_t j = 0; j < created; ++j) {
                pthread_join(tpool_threads[j], NULL);
            }
            free(tpool_threads);
            return POOL_FAIL;
        }
        created++;
    }
    return SUCCESS;
}

void threadpool_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(&tq_lock);
    pthread_cond_broadcast(&tq_not_empty);
    pthread_mutex_unlock(&tq_lock);

    for (long i = 0; i < TPOOL_SIZE; ++i) {
        pthread_join(tpool_threads[i], NULL);
    }
    free(tpool_threads);
    tpool_threads = NULL;
}