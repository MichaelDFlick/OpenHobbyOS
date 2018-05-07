#include "thread.h"
#include "memory.h"
#include "console.h"
#include "string.h"
#include "paging.h"
#include "task.h"
#include "pit.h"
#include "gdt.h"
#include "panic.h"

#define THREAD_STACK_DEFAULT (64 * 1024)

bool thread_init(void) {
    console_write("[thread] initialized (task_slot-backed)\n");
    return true;
}

int thread_create(u32 *tid_out, const thread_attr_t *attr, u32 (*start_func)(void*), void *arg) {
    int parent_slot_idx;
    u32 stack_size;
    u32 heap_stack;
    u32 user_stack_base;
    u32 user_stack_top;
    int thread_slot_idx;
    bool map_ok;

    if (!start_func || !tid_out) return -1;

    parent_slot_idx = task_active_slot_index();
    if (parent_slot_idx < 0) return -1;

    stack_size = (attr && attr->stack_size) ? attr->stack_size : THREAD_STACK_DEFAULT;
    stack_size = (stack_size + PAGE_SIZE - 1) & PAGE_MASK;

    heap_stack = (u32)(uintptr_t)kmalloc(stack_size);
    if (!heap_stack) return -1;

    user_stack_base = USER_MMAP_BASE - stack_size;

    page_directory_t *parent_pd = (page_directory_t *)task_slot_page_dir(parent_slot_idx);
    page_directory_t *old_pd = page_directory_get_current();

    if (parent_pd) {
        page_directory_switch(parent_pd);
    }

    map_ok = true;
    for (u32 va = user_stack_base; va < user_stack_base + stack_size; va += PAGE_SIZE) {
        if (!paging_alloc_user_page(parent_pd, va, PTE_RW | PTE_USER)) {
            map_ok = false;
            break;
        }
    }

    if (!map_ok) {
        page_directory_switch(old_pd);
        kfree((void *)(uintptr_t)heap_stack);
        return -1;
    }

    memset((void *)(uintptr_t)user_stack_base, 0, stack_size);
    page_directory_switch(old_pd);
    kfree((void *)(uintptr_t)heap_stack);

    user_stack_top = user_stack_base + stack_size;

    thread_slot_idx = task_create_thread_slot(start_func, arg, user_stack_top,
                                               user_stack_base, stack_size);
    if (thread_slot_idx < 0) {
        if (parent_pd) {
            page_directory_switch(parent_pd);
            for (u32 va = user_stack_base; va < user_stack_base + stack_size; va += PAGE_SIZE) {
                if (page_is_present(parent_pd, va)) {
                    paging_free_user_page(parent_pd, va);
                }
            }
            page_directory_switch(old_pd);
        }
        return -1;
    }

    *tid_out = (u32)task_slot_pid(thread_slot_idx);
    return 0;
}

void thread_exit(int exit_code) {
    task_exit_current(exit_code);
}

int thread_join(u32 tid, int *exit_code_out) {
    /* Thread join from user space: use the waitpid mechanism.
     * This function is a no-op kernel-side; the syscall handler
     * calls task_waitpid_from_user which handles everything. */
    (void)tid;
    (void)exit_code_out;
    return -1;
}

int thread_detach(u32 tid) {
    (void)tid;
    return 0;
}

void thread_yield(void) {
    /* Yield is handled via syscall; kernel-side no-op */
}

u32 thread_self(void) {
    const task_state_t *s = task_state();
    return s ? s->pid : 0;
}

thread_t *thread_current(void) {
    return NULL;
}

void thread_exit_all(void) {
}

thread_t *thread_next_to_run(void) {
    return NULL;
}

void thread_schedule(void) {
}

void thread_cleanup_process(u32 pid) {
    (void)pid;
}

int thread_check_preempt(registers_t *regs) {
    (void)regs;
    return -1;
}

bool prev_thread_was_thread(void) {
    return false;
}

int thread_save_current(registers_t *regs) {
    (void)regs;
    return -1;
}

typedef struct work_item {
    struct work_item *next;
    void (*func)(void*);
    void *arg;
} work_item_t;

static work_item_t *work_queue_head;
static work_item_t *work_queue_tail;

void workqueue_init(void) {
    work_queue_head = NULL;
    work_queue_tail = NULL;
}

bool workqueue_submit(void (*func)(void*), void *arg) {
    if (!func) return false;

    work_item_t *item = (work_item_t *)kmalloc(sizeof(work_item_t));
    if (!item) return false;

    item->func = func;
    item->arg = arg;
    item->next = NULL;

    if (work_queue_tail) {
        work_queue_tail->next = item;
    } else {
        work_queue_head = item;
    }
    work_queue_tail = item;

    return true;
}

void workqueue_process(void) {
    while (work_queue_head) {
        work_item_t *item = work_queue_head;
        work_queue_head = item->next;
        if (!work_queue_head) {
            work_queue_tail = NULL;
        }

        item->func(item->arg);
        kfree(item);
    }
}

int workqueue_pending(void) {
    return (work_queue_head != NULL) ? 1 : 0;
}
