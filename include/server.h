#ifndef EDGE_SERVER_H
#define EDGE_SERVER_H

#include <stddef.h>
#include "job.h"

void handle_client(
    int client_fd,
    int ipc_fd,
    SharedJobTable *job_table
);

int build_sysinfo(char *out, size_t capacity);

#endif
