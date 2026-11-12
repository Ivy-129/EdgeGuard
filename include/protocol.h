#ifndef EDGE_PROTOCOL_H
#define EDGE_PROTOCOL_H
#include <stddef.h>
#define EDGE_MAX_LINE 256
#define EDGE_MAX_PAYLOAD 4096
int edge_send_response(int fd, int ok, const char *code, const char *payload);
int edge_recv_response(int fd, char *payload, size_t capacity, int *ok);
#endif
