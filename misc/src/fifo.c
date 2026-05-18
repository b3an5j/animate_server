#include "fifo.h"
#include <stdio.h>

void set_name(char *buf, FifoType type, pid_t pid)
{
    switch (type) {
    case C2S:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_C2S_%d", pid);
        break;
    case S2C:
        snprintf(buf, FIFO_MAX_NAME, "FIFO_S2C_%d", pid);
        break;
    default:
        break;
    }
}