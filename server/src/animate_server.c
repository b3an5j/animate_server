#define _POSIX_C_SOURCE 200809L

#include "queues.h"
#include "s_helper.h"
#include <animate/animate.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

long                  THREADPOOL_SIZE = 1;
volatile sig_atomic_t RUNNING         = 1;

int main(int argc, char **argv, char **envp)
{
    /* Thread pool size */
    if (get_threadpool_size(argc, argv) != 0) {
        return retval;
    }

    /* Start procedure */
    if (setup_signals() != 0) {
        return retval;
    }
    pid_t SERVER_PID = getpid();
    printf("Server PID: %d\n", SERVER_PID);

    /* Waiting handshake */
    while (RUNNING) {
        /* Get client info */
        // block
        sigset_t mask, oldmask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR1);
        sigprocmask(SIG_BLOCK, &mask, &oldmask);

        // wait till signal caught
        while (pending_count == 0 && RUNNING) {
            sigsuspend(&oldmask);
        }
        if (!RUNNING) {
            // return the mask
            sigprocmask(SIG_SETMASK, &oldmask, NULL);
            break;
        }

        // get pid then unblock
        pid_t client_pid = dequeue_pending_client();
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        if (client_pid == -1) {
            continue;
        }

        /* Process the client queue */
        char c2s[FIFO_MAX_NAME];
        char s2c[FIFO_MAX_NAME];
        set_name(c2s, C2S, client_pid);
        set_name(s2c, S2C, client_pid);

        unlink(c2s);
        unlink(s2c);

        /* Make FIFO */
        mode_t old_umask = umask(0);
        if (mkfifo(c2s, 0666) == -1 || mkfifo(s2c, 0666) == -1) {
            perror("Failed to create FIFO");
            umask(old_umask);
            continue;
        }
        umask(old_umask);

        /* Signal Client */
        if (kill(client_pid, SIGUSR2) == -1) {
            perror("Failed to signal client");
            unlink(c2s);
            unlink(s2c);
            unqueue_task();
        }

        // TODO: open in spawned threads
        int c2s_fifo = open(c2s, O_RDONLY);
        int s2c_fifo = open(s2c, O_WRONLY);
        if (c2s_fifo == -1 || s2c_fifo == -1) {
            perror("Failed to open FIFO");
            continue;
        }
        enqueue_task(client_pid, c2s_fifo, s2c_fifo);
    }

    struct canvas *canvas = animate_create_canvas(100, 100, 0);
    animate_destroy_canvas(canvas);

    return 0;
}
