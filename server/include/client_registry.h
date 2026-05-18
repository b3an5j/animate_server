#ifndef CLIENT_REGISTRY_H
#define CLIENT_REGISTRY_H

#include <sys/types.h>

extern volatile sig_atomic_t RUNNING;

typedef struct ActiveClient {
    pid_t                client_pid;
    int                  c2s_fd;
    int                  s2c_fd;
    struct ActiveClient *next;
} ActiveClient;

typedef struct {
    ActiveClient   *clients;
    pthread_mutex_t lock;
    size_t          count;
} ClientRegistry;

extern volatile ClientRegistry CLIENT_REGISTRY;
#define cr_clients CLIENT_REGISTRY.clients
#define cr_lock CLIENT_REGISTRY.lock
#define cr_count CLIENT_REGISTRY.count

void creg_init();
int  creg_insert(pid_t client_pid, int c2s_fd, int s2c_fd);
void creg_remove(pid_t client_pid);
void creg_destroy();

#endif /* CLIENT_REGISTRY_H */