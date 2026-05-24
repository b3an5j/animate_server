#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client_registry.h"
#include "errors.h"
#include "fifo.h"
#include "object_table.h"
#include "s_helper.h"

ClientRegistry CLIENT_REGISTRY;

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
        fputs(errs[CR_FAIL], stderr);
        fflush(stderr);
        return NULL;
    }

    newclient->client_pid = client_pid;
    newclient->c2s_fd     = c2s_fd;
    newclient->s2c_fd     = s2c_fd;
    newclient->refcount   = 1; // creg holds 1
    newclient->logged_in  = 0;
    newclient->dead       = 0;
    newclient->next       = NULL;
    newclient->prev       = NULL;

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
    pthread_mutex_lock(&cr_lock);
    if (!client->dead) {
        client->refcount++;
        ret = SUCCESS;
    }
    pthread_mutex_unlock(&cr_lock);
    return ret;
}

void creg_release_client(ActiveClient *client)
{
    uint8_t destroy = 0;

    pthread_mutex_lock(&cr_lock);
    // assert refcount > 0
    client->refcount--;
    if (client->dead && client->refcount == 0) {
        destroy = 1;
    }
    pthread_mutex_unlock(&cr_lock);

    if (destroy) {
        destroy_client_pipes(
            client->client_pid, client->c2s_fd, client->s2c_fd);
        free(client);
    }
}

static void client_markdead(ActiveClient *client)
{
    pthread_mutex_lock(&cr_lock);
    client->dead = 1;
    pthread_mutex_unlock(&cr_lock);

    // destroy all canvas owned by this client
    for (int i = 0; i < CANVAS_TABLE.count; i++) {
        CanvasEntry *ce = &CANVAS_TABLE.arr[i];
        if (ce->ptr && ce->owner == client) {
            canvas_remove(i);
            ce->owner = NULL;
        }
    }

    // destroy all sprites owned by this client
    for (int i = 0; i < SPRITE_TABLE.count; i++) {
        SpriteEntry *se = &SPRITE_TABLE.arr[i];
        if (se->ptr && se->owner == client) {
            sprite_remove(i);
            se->owner = NULL;
        }
    }

    // destroy all placements owned by this client
    for (int i = 0; i < PLACEMENT_TABLE.count; i++) {
        PlacementEntry *pe = &PLACEMENT_TABLE.arr[i];
        if (pe->ptr && pe->owner == client) {
            placement_remove(i);
            pe->owner = NULL;
        }
    }
}

static void creg_unlink(ActiveClient *client)
{
    if (!client)
        return;

    pthread_mutex_lock(&cr_lock);

    ActiveClient *curr = cr_clients;
    while (curr && curr != client) {
        curr = curr->next;
    }

    if (!curr) {
        pthread_mutex_unlock(&cr_lock);
        return;
    }

    if (curr->prev) {
        curr->prev->next = curr->next;
    }
    else {
        cr_clients = curr->next;
    }

    if (curr->next) {
        curr->next->prev = curr->prev;
    }
    cr_count--;

    client->next = NULL;
    client->prev = NULL;
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