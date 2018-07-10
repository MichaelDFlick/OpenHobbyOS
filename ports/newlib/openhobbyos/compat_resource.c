#include <errno.h>
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim) {
    if (rlim == NULL) {
        errno = EINVAL;
        return -1;
    }

    switch (resource) {
        case RLIMIT_CORE:
        case RLIMIT_CPU:
        case RLIMIT_DATA:
        case RLIMIT_FSIZE:
        case RLIMIT_STACK:
        case RLIMIT_NPROC:
        case RLIMIT_NOFILE:
            rlim->rlim_cur = RLIM_INFINITY;
            rlim->rlim_max = RLIM_INFINITY;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

int setrlimit(int resource, const struct rlimit *rlim) {
    (void)rlim;
    switch (resource) {
        case RLIMIT_CORE:
        case RLIMIT_CPU:
        case RLIMIT_DATA:
        case RLIMIT_FSIZE:
        case RLIMIT_STACK:
        case RLIMIT_NPROC:
        case RLIMIT_NOFILE:
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }
}

int getrusage(int who, struct rusage *usage) {
    (void)who;
    if (usage == NULL) {
        errno = EFAULT;
        return -1;
    }
    usage->ru_utime.tv_sec = 0;
    usage->ru_utime.tv_usec = 0;
    usage->ru_stime.tv_sec = 0;
    usage->ru_stime.tv_usec = 0;
    usage->ru_maxrss = 0;
    usage->ru_ixrss = 0;
    usage->ru_idrss = 0;
    usage->ru_isrss = 0;
    usage->ru_minflt = 0;
    usage->ru_majflt = 0;
    usage->ru_nswap = 0;
    usage->ru_inblock = 0;
    usage->ru_oublock = 0;
    usage->ru_msgsnd = 0;
    usage->ru_msgrcv = 0;
    usage->ru_nsignals = 0;
    usage->ru_nvcsw = 0;
    usage->ru_nivcsw = 0;
    return 0;
}
