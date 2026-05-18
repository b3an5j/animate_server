#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client_registry.h"
#include "errors.h"
#include "fifo.h"

volatile ClientRegistry CLIENT_REGISTRY;

void creg_init()
{
    cr_clients = NULL;
    cr_count   = 0;
    pthread_mutex_init(&cr_lock, NULL);
}

int creg_insert(pid_t client_pid, int c2s_fd, int s2c_fd)
{
    // assert fd not 0 1 2 or self pipe

    ActiveClient *newclient = malloc(sizeof(*newclient));
    if (newclient == NULL) {
        fprintf(stderr, errs[CR_FAIL]);
        fflush(stderr);
        return CR_FAIL;
    }

    newclient->client_pid = client_pid;
    newclient->c2s_fd     = c2s_fd;
    newclient->s2c_fd     = s2c_fd;

    pthread_mutex_lock(&cr_lock);

    // update info
    newclient->next = cr_clients;
    cr_clients      = newclient;
    cr_count++;

    pthread_mutex_unlock(&cr_lock);
    return SUCCESS;
}

void creg_remove(pid_t client_pid)
{
    pthread_mutex_lock(&cr_lock);

    // assert > 0

    if (cr_count) {
        // find
        ActiveClient *prev = NULL;
        ActiveClient *curr = cr_clients;
        while (curr && curr->client_pid != client_pid) {
            prev = curr;
            curr = curr->next;
        }

        if (curr) {
            // unlink
            if (prev == NULL) {
                cr_clients = curr->next;
            }
            else {
                prev->next = curr->next;
            }

            // remove
            close(curr->c2s_fd);
            close(curr->s2c_fd);

            char temp[FIFO_MAX_NAME];
            set_name(temp, C2S, client_pid);
            unlink(temp);
            set_name(temp, S2C, client_pid);
            unlink(temp);

            free(curr);
            cr_count--;
        }
    }

    pthread_mutex_unlock(&cr_lock);
}

void creg_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(&cr_lock);

    ActiveClient *curr = cr_clients;
    while (curr) {
        ActiveClient *next = curr->next;

        // close all FDs
        close(curr->c2s_fd);
        close(curr->s2c_fd);

        char temp[FIFO_MAX_NAME];
        set_name(temp, C2S, curr->client_pid);
        unlink(temp);
        set_name(temp, S2C, curr->client_pid);
        unlink(temp);

        free(curr);
        curr = next;
    }
    cr_clients = NULL;
    cr_count   = 0;

    pthread_mutex_unlock(&cr_lock);
    pthread_mutex_destroy(&cr_lock);
}