#include "socket_utils.h"
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
ssize_t edge_send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (const char *)buf + sent, len - sent, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return (ssize_t)sent;
}
ssize_t edge_recv_line(int fd, char *buf, size_t capacity) {
    if (capacity < 2) { errno = EINVAL; return -1; }
    size_t used = 0;
    for (;;) {
        char ch;
        ssize_t n = recv(fd, &ch, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return -1;
        if (n == 0) { if (used == 0) return 0; errno = EPROTO; return -1; }
        if (ch == '\n') {
            if (used && buf[used-1] == '\r') used--;
            buf[used] = '\0';
            return (ssize_t)used;
        }
        if (used + 1 >= capacity) {
            /* Drain the offending line, so the next request remains aligned. */
            while (ch != '\n') {
                n = recv(fd, &ch, 1, 0);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) break;
            }
            buf[0] = '\0';
            return -2;
        }
        buf[used++] = ch;
    }
}
int edge_recv_exact(int fd, void *buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        ssize_t n = recv(fd, (char *)buf + done, len - done, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        done += (size_t)n;
    }
    return 0;
}
