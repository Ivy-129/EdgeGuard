
#ifndef EDGE_IPC_H
#define EDGE_IPC_H

#include <stdint.h>
#include "job.h"

typedef enum {
    IPC_SUBMIT = 1
} IpcCommand;

typedef struct {
    uint32_t command;
    uint32_t task_type;
    char argument[128];
} IpcRequest;

typedef struct {
    int32_t success;
    uint32_t job_id;
    char message[128];
} IpcResponse;

int ipc_send_request(int fd, const IpcRequest *request);
int ipc_recv_request(int fd, IpcRequest *request);

int ipc_send_response(int fd, const IpcResponse *response);
int ipc_recv_response(int fd, IpcResponse *response);

#endif
