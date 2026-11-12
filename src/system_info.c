#include "server.h"
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>
int build_sysinfo(char *out, size_t capacity) {
    struct utsname u;
    struct sysinfo s;
    char host[256];
    if (uname(&u) < 0 || sysinfo(&s) < 0 || gethostname(host, sizeof host) < 0) return -1;
    host[sizeof host - 1] = '\0';
    unsigned long long total = (unsigned long long)s.totalram * s.mem_unit / (1024 * 1024);
    unsigned long long free_ram = (unsigned long long)s.freeram * s.mem_unit / (1024 * 1024);
    int n = snprintf(out, capacity,
        "OS=%s\nHOSTNAME=%s\nKERNEL=%s\nUPTIME_SEC=%ld\nTOTAL_RAM_MB=%llu\nFREE_RAM_MB=%llu\n",
        u.sysname, host, u.release, s.uptime, total, free_ram);
    return n >= 0 && (size_t)n < capacity ? 0 : -1;
}
