#include "dbg.h"
#include <stdarg.h>
#include <stdio.h>

int EN_DEBUG = 0;

void debug_log(const char *fmt, ...)
{
    if (!EN_DEBUG)
        return;

    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[DEBUG] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(args);
}
