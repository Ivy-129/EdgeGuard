#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static int current_level = 1;

void edge_log_set_level(int level) {
    current_level = level;
}

void edge_log(int level, const char *fmt, ...) {
#ifndef EDGE_DEBUG
    if (level > 1) return;
#endif

    if (level > current_level) return;

    char buffer[1024];
    time_t now = time(NULL);
    struct tm tmval;

    localtime_r(&now, &tmval);

    int n = snprintf(
        buffer,
        sizeof(buffer),
        "[%04d-%02d-%02d %02d:%02d:%02d][%s][PID=%ld] ",
        tmval.tm_year + 1900,
        tmval.tm_mon + 1,
        tmval.tm_mday,
        tmval.tm_hour,
        tmval.tm_min,
        tmval.tm_sec,
        level == 0 ? "ERROR" :
        level == 1 ? "INFO" : "DEBUG",
        (long)getpid()
    );

    if (n < 0 || (size_t)n >= sizeof(buffer)) return;

    va_list args;
    va_start(args, fmt);

    int m = vsnprintf(
        buffer + n,
        sizeof(buffer) - (size_t)n,
        fmt,
        args
    );

    va_end(args);

    if (m < 0) return;

    size_t len = (size_t)n + (size_t)m;

    if (len >= sizeof(buffer) - 1)
        len = sizeof(buffer) - 2;

    buffer[len++] = '\n';

    ssize_t written = write(STDERR_FILENO, buffer, len);

    if (written < 0) {
        return;
}
}
