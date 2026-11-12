
#include "resource_monitor.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

int resource_get_available_memory_kb(uint64_t *available_kb)
{
    if (available_kb == NULL) {
        return -1;
    }

    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        return -1;
    }

    char line[256];
    unsigned long long value = 0;
    int found = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "MemAvailable: %llu kB", &value) == 1) {
            found = 1;
            break;
        }
    }

    fclose(fp);

    if (!found) {
        return -1;
    }

    *available_kb = (uint64_t)value;
    return 0;
}
