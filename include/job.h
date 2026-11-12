#ifndef EDGE_JOB_H
#define EDGE_JOB_H

#include <stdint.h>
#include <sys/types.h>
#include <semaphore.h>

#define EDGE_MAX_JOBS 64
#define EDGE_MAX_WORKERS 3

typedef enum {
    JOB_EMPTY = 0,
    JOB_STARTING,
    JOB_RUNNING,
    JOB_COMPLETED,
    JOB_FAILED,
    JOB_TIMEOUT
} JobStatus;

typedef enum {
    TASK_BENCH = 1,
    TASK_HASH
} TaskType;

typedef struct {
    uint32_t job_id;
    TaskType type;
    JobStatus status;
    pid_t worker_pid;
    char argument[128];
    char result[512];
} JobRecord;

typedef struct {
    sem_t lock;
    uint32_t next_job_id;
    JobRecord jobs[EDGE_MAX_JOBS];
} SharedJobTable;

SharedJobTable *job_table_create(void);
int job_table_destroy(SharedJobTable *table);

int job_create(
    SharedJobTable *table,
    TaskType type,
    const char *argument,
    uint32_t *job_id
);

int job_get(
    SharedJobTable *table,
    uint32_t job_id,
    JobRecord *out
);

int job_set_running(
    SharedJobTable *table,
    uint32_t job_id,
    pid_t worker_pid
);

int job_finish(
    SharedJobTable *table,
    uint32_t job_id,
    JobStatus final_status,
    const char *result
);


#endif
