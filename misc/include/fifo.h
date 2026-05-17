#ifndef FIFO_H
#define FIFO_H

#define FIFO_MAX_NAME 64

typedef enum {
    C2S,
    S2C
} FifoType;

void set_name(const char *buf, FifoType type, pid_t pid);

#endif /* FIFO_H */