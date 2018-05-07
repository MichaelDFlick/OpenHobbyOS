#ifndef OHOS_THREAD_H
#define OHOS_THREAD_H

#include "task.h"
#include "idt.h"

#define THREAD_MAX_THREADS      64
#define THREAD_DEFAULT_STACK    (64 * 1024)
#define THREAD_MAX_PRIORITY     0
#define THREAD_MIN_PRIORITY     99
#define THREAD_DEFAULT_PRIORITY 50

typedef enum {
    THREAD_STATE_FREE = 0,
    THREAD_STATE_RUNNING,
    THREAD_STATE_READY,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_ZOMBIE,
} thread_state_t;

typedef struct thread {
    u32 tid;
    u32 pid;
    registers_t regs;
    thread_state_t state;
    u32 user_stack_base;
    u32 user_stack_size;
    u32 priority;
    u32 time_slice;
    u32 wait_for_tid;
    int exit_value;
    u32 magic;
    struct thread *next_free;
} thread_t;

typedef struct {
    u32 stack_size;
    u32 guard_size;
    u32 priority;
} thread_attr_t;

bool thread_init(void);
int thread_create(u32 *tid_out, const thread_attr_t *attr, u32 (*start_func)(void*), void *arg);
void thread_exit(int exit_code) NORETURN;
int thread_join(u32 tid, int *exit_code_out);
int thread_detach(u32 tid);
void thread_yield(void);
u32 thread_self(void);
thread_t *thread_current(void);
void thread_exit_all(void);
thread_t *thread_next_to_run(void);
void thread_schedule(void);

/* Scheduler integration */
int thread_check_preempt(registers_t *regs);
bool prev_thread_was_thread(void);
int thread_save_current(registers_t *regs);
void thread_cleanup_process(u32 pid);

/* Workqueue */
void workqueue_init(void);
bool workqueue_submit(void (*func)(void*), void *arg);
void workqueue_process(void);
int workqueue_pending(void);

#endif
