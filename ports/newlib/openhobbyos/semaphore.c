#include <errno.h>
#include <limits.h>
#include <linux/futex.h>
#include <semaphore.h>
#include <time.h>
#include "compat.h"

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    if (value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    (void)pshared;
    sem->_count = (int)value;
    return 0;
}

int sem_destroy(sem_t *sem) {
    (void)sem;
    return 0;
}

int sem_wait(sem_t *sem) {
    while (1) {
        int count = __atomic_load_n(&sem->_count, __ATOMIC_RELAXED);
        while (count > 0) {
            if (__atomic_compare_exchange_n(&sem->_count, &count, count - 1,
                                            false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                return 0;
        }
        if (oh_syscall6(LINUX_SYS_FUTEX, (int)&sem->_count,
                        FUTEX_WAIT | FUTEX_PRIVATE_FLAG, count, 0, 0, 0) < 0
            && errno != EAGAIN)
            return -1;
    }
}

int sem_trywait(sem_t *sem) {
    int count = __atomic_load_n(&sem->_count, __ATOMIC_RELAXED);
    while (count > 0) {
        if (__atomic_compare_exchange_n(&sem->_count, &count, count - 1,
                                        false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return 0;
    }
    errno = EAGAIN;
    return -1;
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    struct timespec now;

    while (1) {
        int count = __atomic_load_n(&sem->_count, __ATOMIC_RELAXED);
        while (count > 0) {
            if (__atomic_compare_exchange_n(&sem->_count, &count, count - 1,
                                            false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
                return 0;
        }

        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_timeout->tv_sec ||
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
            errno = ETIMEDOUT;
            return -1;
        }

        struct linux_timespec rel;
        rel.tv_sec = abs_timeout->tv_sec - now.tv_sec;
        rel.tv_nsec = abs_timeout->tv_nsec - now.tv_nsec;
        if (rel.tv_nsec < 0) {
            rel.tv_sec--;
            rel.tv_nsec += 1000000000;
        }

        if (oh_syscall6(LINUX_SYS_FUTEX, (int)&sem->_count,
                        FUTEX_WAIT | FUTEX_PRIVATE_FLAG, count,
                        (int)&rel, 0, 0) < 0) {
            if (errno == ETIMEDOUT || errno == EAGAIN)
                continue;
            return -1;
        }
    }
}

int sem_post(sem_t *sem) {
    int count = __atomic_fetch_add(&sem->_count, 1, __ATOMIC_RELEASE);
    if (count < 0) {
        if (oh_syscall6(LINUX_SYS_FUTEX, (int)&sem->_count,
                        FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, 0, 0, 0) < 0)
            return -1;
    }
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (sval == NULL) {
        errno = EINVAL;
        return -1;
    }
    *sval = __atomic_load_n(&sem->_count, __ATOMIC_RELAXED);
    if (*sval < 0)
        *sval = 0;
    return 0;
}
