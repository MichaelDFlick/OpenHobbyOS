#ifndef OHOS_SPINLOCK_H
#define OHOS_SPINLOCK_H

#include "types.h"
#include "io.h"

typedef struct {
    volatile u32 locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline void spin_init(spinlock_t *lock) {
    lock->locked = 0;
}

static inline void spin_lock(spinlock_t *lock) {
    while (__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        while (lock->locked) cpu_pause();
    }
}

static inline bool spin_trylock(spinlock_t *lock) {
    return !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE);
}

static inline void spin_unlock(spinlock_t *lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}

static inline bool spin_is_locked(spinlock_t *lock) {
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED) != 0;
}

/* IRQ-safe spinlock — saves and restores IF state */
typedef struct {
    spinlock_t lock;
} spinlock_irq_t;

#define SPINLOCK_IRQ_INIT { SPINLOCK_INIT }

static inline void spin_lock_irqsave(spinlock_irq_t *lock, u32 *flags) {
    __asm__ volatile ("pushfl; popl %0" : "=r"(*flags));
    interrupts_disable();
    spin_lock(&lock->lock);
}

static inline void spin_unlock_irqrestore(spinlock_irq_t *lock, u32 flags) {
    spin_unlock(&lock->lock);
    if (flags & 0x200) interrupts_enable();
}

#endif
