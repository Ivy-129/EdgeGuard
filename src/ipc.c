
#include "ipc.h"

#include <errno.h>
#include <sys/socket.h>

static int send_packet(int fd, const void *data, size_t size) {
    ssize_t n;

    do {
        n = send(fd, data, size, MSG_NOSIGNAL);
    } while (n < 0 && errno == EINTR);

    return n == (ssize_t)size ? 0 : -1;
}

static int recv_packet(int fd, void *data, size_t size) {
    ssize_t n;

    do {
        n = recv(fd, data, size, MSG_WAITALL);
    } while (n < 0 && errno == EINTR);

    return n == (ssize_t)size ? 0 : -1;
}

int ipc_send_request(int fd, const IpcRequest *request) {
    if (!request) return -1;
    return send_packet(fd, request, sizeof(*request));
}

int ipc_recv_request(int fd, IpcRequest *request) {
    if (!request) return -1;
    return recv_packet(fd, request, sizeof(*request));
}

int ipc_send_response(int fd, const IpcResponse *response) {
    if (!response) return -1;
    return send_packet(fd, response, sizeof(*response));
}

int ipc_recv_response(int fd, IpcResponse *response) {
    if (!response) return -1;
    return recv_packet(fd, response, sizeof(*response));
}
