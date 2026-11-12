
#include "server.h"
#include "ipc.h"
#include "logger.h"
#include "protocol.h"
#include "socket_utils.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

static const char *job_status_name(JobStatus status) {
    switch (status) {
    case JOB_STARTING:
        return "STARTING";
    case JOB_RUNNING:
        return "RUNNING";
    case JOB_COMPLETED:
        return "COMPLETED";
    case JOB_FAILED:
        return "FAILED";
    case JOB_TIMEOUT:
        return "TIMEOUT";
    default:
        return "UNKNOWN";
    }
}

static int handle_submit(int fd, int ipc_fd, const char *line) {
    IpcRequest request = {0};
    IpcResponse response = {0};

    request.command = IPC_SUBMIT;

    if (strncmp(line, "SUBMIT BENCH ", 13) == 0) {
        const char *arg = line + 13;
        char *end = NULL;

        errno = 0;
        unsigned long value = strtoul(arg, &end, 10);

        if (errno != 0 || end == arg || *end != '\0' || value == 0 ||
            value > 10000000UL) {
            return edge_send_response(
                fd, 0, "INVALID_ARGUMENT",
                "BENCH requires an integer from 1 to 10000000\n");
        }

        request.task_type = TASK_BENCH;
        snprintf(request.argument, sizeof(request.argument), "%lu", value);

    } else if (strncmp(line, "SUBMIT HASH ", 12) == 0) {
        const char *filename = line + 12;
        size_t length = strlen(filename);

        if (length == 0 || length >= sizeof(request.argument) ||
            strchr(filename, '/') != NULL || strcmp(filename, ".") == 0 ||
            strcmp(filename, "..") == 0) {
            return edge_send_response(fd, 0, "INVALID_ARGUMENT",
                                      "HASH requires a simple filename\n");
        }

        request.task_type = TASK_HASH;
        memcpy(request.argument, filename, length + 1);

    } else {
        return edge_send_response(
            fd, 0, "INVALID_ARGUMENT",
            "Use SUBMIT BENCH <n> or SUBMIT HASH <filename>\n");
    }

    if (ipc_send_request(ipc_fd, &request) < 0 ||
        ipc_recv_response(ipc_fd, &response) < 0) {
        return edge_send_response(fd, 0, "IPC_ERROR",
                                  "Parent Server unavailable\n");
    }

    if (!response.success) {
        return edge_send_response(fd, 0, "SUBMIT_REJECTED", response.message);
    }

    char result[256];
    snprintf(result, sizeof(result), "JOB_ID=%" PRIu32 "\nSTATUS=STARTING\n",
             response.job_id);

    return edge_send_response(fd, 1, NULL, result);
}

static int handle_status(int fd, SharedJobTable *table, const char *line) {
    const char *arg = line + 7;
    char *end = NULL;

    errno = 0;
    unsigned long value = strtoul(arg, &end, 10);

    if (errno != 0 || end == arg || *end != '\0' || value == 0 ||
        value > UINT32_MAX) {
        return edge_send_response(fd, 0, "INVALID_ARGUMENT",
                                  "STATUS requires a valid Job ID\n");
    }

    JobRecord job;

    if (job_get(table, (uint32_t)value, &job) < 0) {
        return edge_send_response(fd, 0, "NOT_FOUND", "Job not found\n");
    }

    char result[768];

    snprintf(result, sizeof(result),
             "JOB_ID=%" PRIu32 "\n"
             "TYPE=%s\n"
             "STATUS=%s\n"
             "ARGUMENT=%s\n",
             job.job_id, job.type == TASK_BENCH ? "BENCH" : "HASH",
             job_status_name(job.status), job.argument);

    return edge_send_response(fd, 1, NULL, result);
}

static int handle_result(int fd, SharedJobTable *table, const char *line) {
    const char *arg = line + 7;
    char *end = NULL;

    errno = 0;
    unsigned long value = strtoul(arg, &end, 10);

    if (errno != 0 || end == arg || *end != '\0' || value == 0 ||
        value > UINT32_MAX) {
        return edge_send_response(fd, 0, "INVALID_ARGUMENT",
                                  "RESULT requires a valid Job ID\n");
    }

    JobRecord job;

    if (job_get(table, (uint32_t)value, &job) < 0) {
        return edge_send_response(fd, 0, "NOT_FOUND", "Job not found\n");
    }

    if (job.status == JOB_STARTING || job.status == JOB_RUNNING) {
        return edge_send_response(fd, 0, "NOT_READY", "Job is still running\n");
    }

    if (job.result[0] == '\0') {
        return edge_send_response(fd, 0, "NO_RESULT", "Job has no result\n");
    }

    return edge_send_response(fd, 1, NULL, job.result);
}

void handle_client(int client_fd, int ipc_fd, SharedJobTable *job_table) {
    struct timeval tv = {.tv_sec = 120, .tv_usec = 0};

    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    char line[EDGE_MAX_LINE];

    for (;;) {
        ssize_t n = edge_recv_line(client_fd, line, sizeof(line));

        if (n == 0)
            break;

        if (n == -2) {
            if (edge_send_response(client_fd, 0, "TOO_LONG",
                                   "Request exceeds limit\n") < 0)
                break;
            continue;
        }

        if (n < 0) {
            edge_log(1, "Client disconnected or read error");
            break;
        }

        edge_log(2, "Request: %s", line);

        int rc;

        if (strcmp(line, "SYSINFO") == 0) {
            char result[1024];

            if (build_sysinfo(result, sizeof(result)) < 0) {
                rc = edge_send_response(client_fd, 0, "INTERNAL_ERROR",
                                        "System info unavailable\n");
            } else {
                rc = edge_send_response(client_fd, 1, NULL, result);
            }

        } else if (strcmp(line, "QUIT") == 0) {
            edge_send_response(client_fd, 1, NULL, "Goodbye\n");
            break;

        } else if (strncmp(line, "SUBMIT ", 7) == 0) {
            rc = handle_submit(client_fd, ipc_fd, line);

        } else if (strncmp(line, "STATUS ", 7) == 0) {
            rc = handle_status(client_fd, job_table, line);
        } else if (strncmp(line, "RESULT ", 7) == 0) {
            rc = handle_result(client_fd, job_table, line);
        } else {
            rc = edge_send_response(
                client_fd, 0, "INVALID_COMMAND",
                "Use SYSINFO, SUBMIT, STATUS, RESULT or QUIT\n");
        }

        if (rc < 0)
            break;
    }
}
