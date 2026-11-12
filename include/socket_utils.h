#ifndef EDGE_SOCKET_UTILS_H
#define EDGE_SOCKET_UTILS_H
#include <stddef.h>
#include <sys/types.h>
ssize_t edge_send_all(int fd, const void *buf, size_t len);
/* returns line length, 0 for clean EOF, -1 error, -2 oversized line */
ssize_t edge_recv_line(int fd, char *buf, size_t capacity);
int edge_recv_exact(int fd, void *buf, size_t len);
#endif
