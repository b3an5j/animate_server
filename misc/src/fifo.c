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

void sanitise_whitespace(char *str)
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

ErrType write_blob(int fd, const void *buf, size_t len)
{
    ErrType ret;

    ret = write_block_pipe(fd, &len, sizeof(len));
    if (ret != SUCCESS)
        return ret;

    return write_block_pipe(fd, buf, len);
}

ErrType read_blob(int fd, void *buf, size_t maxlen, size_t *out_len)
{
    uint32_t len;
    ErrType  ret;

    ret = read_nonblock_pipe(fd, &len, sizeof(len));
    if (ret != SUCCESS)
        return ret;

    if (len >= maxlen)
        return PIPE_PARTIAL;

    ret = read_nonblock_pipe(fd, buf, len);
    if (ret != SUCCESS)
        return ret;

    ((char *)buf)[len] = '\0';
    *out_len           = len;
    return SUCCESS;
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

ErrType read_nonblock_pipe(int fd, void *buf, size_t n_target)
{
    size_t n_read = 0;
    char  *ptr    = buf;

    while (n_read < n_target) {
        ssize_t n = read(fd, ptr + n_read, n_target - n_read);

        if (n > 0) {
            n_read += n;
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

ErrType read_until_delim(int     fd,
                         char   *buf,
                         size_t  maxlen,
                         char    delim,
                         size_t *out_len)
{
    size_t n_read = 0;

    while (n_read < maxlen - 1) {
        char    c;
        ssize_t n = read(fd, &c, 1);

        if (n > 0) {
            if (c == delim) {
                buf[n_read] = '\0';
                *out_len    = n_read;
                return SUCCESS;
            }

            buf[n_read++] = c;
        }
        else if (n == 0) {
            return PIPE_CLOSED; // client closed FIFO
        }
        else {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return PIPE_EMPTY; // nothing available yet

            if (errno == EINTR)
                continue;

            return PIPE_READ_FAIL;
        }
    }

    buf[n_read] = '\0';
    *out_len    = n_read;
    return SUCCESS;
}