#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client_registry.h"
#include "errors.h"
#include "fifo.h"
#include "s_helper.h"

volatile ClientRegistry CLIENT_REGISTRY;

void creg_init()
{
    cr_clients = NULL;
    cr_count   = 0;
    pthread_mutex_init(&cr_lock, NULL);
}

ActiveClient *creg_insert(pid_t client_pid, int c2s_fd, int s2c_fd)
{
    // assert fd not 0 1 2 or self pipe

    ActiveClient *newclient = malloc(sizeof(*newclient));
    if (newclient == NULL) {
        fprintf(stderr, errs[CR_FAIL]);
        fflush(stderr);
        return NULL;
    }

    newclient->client_pid = client_pid;
    newclient->c2s_fd     = c2s_fd;
    newclient->s2c_fd     = s2c_fd;
    newclient->refcount   = 1; // creg holds 1
    newclient->logged_in  = false;
    newclient->dead       = false;
    newclient->next       = NULL;
    newclient->prev       = NULL;
    pthread_mutex_init(&newclient->lock, NULL);

    pthread_mutex_lock(&cr_lock);

    // update info
    newclient->next = cr_clients;
    if (cr_clients) {
        cr_clients->prev = newclient;
    }
    cr_clients = newclient;
    cr_count++;

    pthread_mutex_unlock(&cr_lock);
    return newclient;
}

ErrType creg_acquire_client(ActiveClient *client)
{
    ErrType ret = FAIL;
    pthread_mutex_lock(&client->lock);
    if (!client->dead) {
        client->refcount++;
        ret = SUCCESS;
    }
    pthread_mutex_unlock(&client->lock);
    return ret;
}

void creg_release_client(ActiveClient *client)
{
    uint8_t destroy = false;
    pthread_mutex_lock(&client->lock);

    // assert refcount > 0
    client->refcount--;
    if (client->dead && client->refcount == 0) {
        destroy = true;
    }
    pthread_mutex_unlock(&client->lock);

    if (destroy) {
        destroy_client_pipes(
            client->client_pid, client->c2s_fd, client->s2c_fd);
        pthread_mutex_destroy(&client->lock);
        free(c);
    }
}

static void client_markdead(ActiveClient *client)
{
    pthread_mutex_lock(&client->lock);
    client->dead = true;
    pthread_mutex_unlock(&client->lock);
}

static void creg_unlink(ActiveClient *client)
{
    if (!client)
        return;

    pthread_mutex_lock(&cr_lock);
    if (cr_clients) {
        pthread_mutex_unlock(&cr_lock);
        return;
    }

    if (client->prev) {
        client->prev->next = client->next;
    }
    else { // head
        cr_clients = client->next;
    }

    if (client->next) {
        client->next->prev = client->prev;
    }

    client->next = NULL;
    client->prev = NULL;
    cr_count--;
    pthread_mutex_unlock(&cr_lock);
}

void creg_remove(ActiveClient *client)
{
    if (!client)
        return;

    client_markdead(client);
    creg_unlink(client);
    creg_release_client(client);
}

void creg_destroy()
{
    if (RUNNING)
        return;

    pthread_mutex_lock(&cr_lock);

    ActiveClient *curr = cr_clients;
    cr_clients         = NULL;
    cr_count           = 0;

    pthread_mutex_unlock(&cr_lock);

    while (curr) {
        ActiveClient *next = curr->next;
        destroy_client_pipes(curr->client_pid, curr->c2s_fd, curr->s2c_fd);
        free(curr);
        curr = next;
    }
    pthread_mutex_destroy(&cr_lock);
}