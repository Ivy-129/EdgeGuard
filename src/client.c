#include "logger.h"
#include "protocol.h"
#include "socket_utils.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
int main(int argc, char **argv) {
    if (argc < 3 || argc > 5) { fprintf(stderr, "Usage: %s HOST PORT [--log-level LEVEL]\n", argv[0]); return 1; }
    int port = atoi(argv[2]);
    if (port < 1 || port > 65535) return 1;
    if (argc == 5) { if (strcmp(argv[3], "--log-level") != 0) return 1; int level=atoi(argv[4]); if(level<0||level>2)return 1; edge_log_set_level(level); }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons((unsigned short)port)};
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) != 1) { fprintf(stderr, "IPv4 address required\n"); close(fd); return 1; }
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) < 0) { perror("connect"); close(fd); return 1; }
    edge_log(1, "Connected");
    char *line = NULL; size_t cap = 0;
    for (;;) {
        fputs("edge> ", stdout); fflush(stdout);
        ssize_t n = getline(&line, &cap, stdin);
        if (n < 0) break;
        if (n == 0) continue;
        if (line[n-1] != '\n') { fprintf(stderr, "Input must end with newline\n"); continue; }
        if (n >= EDGE_MAX_LINE) { fprintf(stderr, "Command too long\n"); continue; }
        if (edge_send_all(fd, line, (size_t)n) < 0) { perror("send"); break; }
        char payload[EDGE_MAX_PAYLOAD+1]; int ok;
        if (edge_recv_response(fd, payload, sizeof payload, &ok) < 0) { fprintf(stderr, "Server closed connection or protocol error\n"); break; }
        fputs(payload, stdout);
        if (strcmp(line, "QUIT\n") == 0) break;
    }
    free(line);
    close(fd);
    return 0;
}
