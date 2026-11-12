#include "protocol.h"
#include "socket_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int edge_send_response(int fd, int ok, const char *code, const char *payload) {
    if (!payload) payload = "";
    size_t len = strlen(payload);
    if (len > EDGE_MAX_PAYLOAD) return -1;
    char header[128];
    int n = ok ? snprintf(header, sizeof header, "OK %zu\n", len)
               : snprintf(header, sizeof header, "ERR %s %zu\n", code ? code : "UNKNOWN", len);
    if (n < 0 || (size_t)n >= sizeof header) return -1;
    if (edge_send_all(fd, header, (size_t)n) < 0) return -1;
    return edge_send_all(fd, payload, len) < 0 ? -1 : 0;
}
int edge_recv_response(int fd, char *payload, size_t capacity, int *ok) {
    char header[128], code[64];
    size_t len;
    ssize_t n = edge_recv_line(fd, header, sizeof header);
    if (n <= 0) return -1;
    if (sscanf(header, "OK %zu", &len) == 1 && strncmp(header, "OK ", 3) == 0) {
        *ok = 1;
    } else if (sscanf(header, "ERR %63s %zu", code, &len) == 2 && strncmp(header, "ERR ", 4) == 0) {
        *ok = 0;
        fprintf(stderr, "Server error: %s\n", code);
    } else return -1;
    if (len >= capacity || len > EDGE_MAX_PAYLOAD) return -1;
    if (edge_recv_exact(fd, payload, len) < 0) return -1;
    payload[len] = '\0';
    return 0;
}
