#include <stdarg.h>
#include <stdio.h>

#include "log.h"

static void log_format(const char *tag, const char *msg, va_list args) {
    static const size_t MSG_MAX_LEN = 4096;
    char buf[MSG_MAX_LEN];
    vsnprintf(buf, MSG_MAX_LEN, msg, args);

    fprintf(stderr, "%s: %s\n", tag, buf);
}

void log_error(const char *msg, ...) {
    va_list args;
    va_start(args, msg);

    log_format("error", msg, args);
    va_end(args);
}

void log_info(const char *msg, ...) {
    va_list args;
    va_start(args, msg);

    log_format("info", msg, args);
    va_end(args);
}

void log_debug(const char *msg, ...) {
    va_list args;
    va_start(args, msg);

    log_format("debug", msg, args);
    va_end(args);
}
