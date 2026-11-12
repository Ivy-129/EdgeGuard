
#ifndef EDGE_RESOURCE_MONITOR_H
#define EDGE_RESOURCE_MONITOR_H

#include <stdint.h>

// Read MemAvailable from /proc/meminfo.
// Return 0 on success, -1 on failure.
int resource_get_available_memory_kb(uint64_t *available_kb);

#endif

