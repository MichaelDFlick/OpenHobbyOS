#include <errno.h>
#include <sched.h>
#include <sys/cpuset.h>
#include "compat.h"

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    (void)pid;
    if (mask == NULL || cpusetsize < sizeof(cpu_set_t)) {
        errno = EINVAL;
        return -1;
    }
    CPU_ZERO(mask);
    CPU_SET(0, mask);
    return 0;
}

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
    (void)pid;
    (void)cpusetsize;
    (void)mask;
    return 0;
}

int sched_getcpu(void) {
    return 0;
}
