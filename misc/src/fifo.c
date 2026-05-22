#include "fifo.h"
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

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

void sanitise_whitespace(const char *str)
{
    size_t  read          = 0;
    size_t  write         = 0;
    uint8_t in_whitespace = 0;

    while (str[read]) {
        unsigned char c = str[read++];
        if (isspace(c)) {
            in_whitespace = 1;
            continue;
        }

        // dont put space in the first cell
        if (in_whitespace && write > 0) {
            str[write++] = ' ';
        }
        in_whitespace = 0;
        str[write++]  = c;
    }

    str[write] = '\0';
}

ErrType write_nonblock_pipe(int fd, const void *buf, size_t n_target)
{
    size_t      n_written = 0;
    const char *ptr       = buf;

    while (n_written < n_target) {
        ssize_t n = write(fd, ptr + n_written, n_target - n_written);
        if (n > 0) {
            n_written += (size_t)n;
            continue;
        }
        else if (n == 0) {
            return PIPE_WRITE_FAIL;
        }
        else {
            if (errno == EINTR) {
                continue;
            }
            else if (errno == EPIPE) {
                return PIPE_CLOSED;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!n_written) {
                    return PIPE_FULL;
                }
                return PIPE_PARTIAL;
            }
            else {
                return PIPE_WRITE_FAIL;
            }
        }
    }
    return SUCCESS;
}

ErrType write_block_pipe(int fd, const void *buf, size_t n_target)
{
    size_t      n_written = 0;
    const char *ptr       = buf;

    while (n_written < n_target) {
        ssize_t n = write(fd, ptr + n_written, n_target - n_written);
        if (n > 0) {
            n_written += (size_t)n;
            continue;
        }
        else if (n == 0) {
            return PIPE_WRITE_FAIL;
        }
        else {
            if (errno == EINTR) {
                continue;
            }
            else if (errno == EPIPE) {
                return PIPE_CLOSED;
            }
            else {
                return PIPE_WRITE_FAIL;
            }
        }
    }
    return SUCCESS;
}

ErrType read_nonblock_pipe(int fd, const void *buf, size_t n_target)
{
    size_t      n_read = 0;
    const char *ptr    = buf;

    while (n_read < n_target) {
        ssize_t n = read(fd, ptr + n_read, n_target - n_read);
        if (n > 0) {
            n_read += (size_t)n;
            continue;
        }
        else if (n == 0) {
            return PIPE_CLOSED;
        }
        else { // errors
            if (errno == EINTR) {
                continue;
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // nonblocking, could be empty or broken
                if (!n_read) {
                    return PIPE_EMPTY;
                }
                return PIPE_PARTIAL;
            }
            else {
                return PIPE_READ_FAIL;
            }
        }
    }
    return SUCCESS;
}

ErrType read_until_delim(int fd, const void *buf, size_t maxlen, char delim)
{
    size_t      n_read = 0;
    const char *ptr    = buf;

    while (n_read < maxlen - 1) {
        char    c;
        ssize_t n = read(fd, &c, 1);
        if (n > 0) {
            if (c == delim) {
                break;
            }
            ptr[n_read++] = c;
        }
        else if (n == 0) {
            return PIPE_CLOSED;
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            return PIPE_READ_FAIL;
        }
    }
    ptr[n_read] = '\0';
    return SUCCESS;
}