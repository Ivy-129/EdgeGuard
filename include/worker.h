
#ifndef EDGE_WORKER_H
#define EDGE_WORKER_H

#include <stdint.h>
#include "job.h"

/*
 * 在目前的 Process 執行 BENCH Job。
 * 之後由 Parent Server fork() 出 Worker 呼叫。
 */
int worker_run_bench(SharedJobTable *table, uint32_t job_id, uint32_t limit);

int worker_run_hash(SharedJobTable *table, uint32_t job_id,
                    const char *filename);

#endif
