#ifndef RECEIVER_H
#define RECEIVER_H

#include <pthread.h>

int spawn_receiver(pthread_t *recv);
int recv_server_reply(char *out);

#endif /* RECEIVER_H */