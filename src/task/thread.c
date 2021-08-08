#include "thread.h"
#include "io.h"
#include "memory.h"
#include "console.h"
#include "string.h"
#include "paging.h"
#include "task.h"
#include "pit.h"
#include "gdt.h"
#include "panic.h"

#define THREAD_STACK_DEFAULT (64 * 1024)
#define THREAD_MAGIC 0xDEADBEEF

static thread_t thread_table[THREAD_MAX_THREADS];
static int thread_current_idx = -1;
static bool prev_was_thread = false;

static int thread_find_by_pid(u32 pid) {
    for (int i = 0; i < THREAD_MAX_THREADS; ++i) {
        if (thread_table[i].magic == THREAD_MAGIC && thread_table[i].tid == pid)
            return i;
    }
    return -1;
}

static int thread_alloc_slot(void) {
    for (int i = 0; i < THREAD_MAX_THREADS; ++i) {
        if (thread_table[i].magic != THREAD_MAGIC)
            return i;
    }
    return -1;
}

static void thread_free_entry(int idx) {
    if (idx < 0 || idx >= THREAD_MAX_THREADS) return;
    memset(&thread_table[idx], 0, sizeof(thread_t));
}

bool thread_init(void) {
    memset(thread_table, 0, sizeof(thread_table));
    thread_current_idx = -1;
    prev_was_thread = false;

    const task_state_t *s = task_state();
    if (s && s->pid > 0) {
        int idx = thread_alloc_slot();
        if (idx >= 0) {
            thread_table[idx].tid = s->pid;
            thread_table[idx].pid = s->pid;
            thread_table[idx].state = THREAD_STATE_RUNNING;
            thread_table[idx].priority = s->priority;
            thread_table[idx].time_slice = 2;
            thread_table[idx].magic = THREAD_MAGIC;
            thread_current_idx = idx;
        }
    }

    workqueue_init();
    console_write("[thread] initialized (task_slot-backed)\n");
    return true;
}

int thread_create(u32 *tid_out, const thread_attr_t *attr,
                  u32 (*start_func)(void*), void *arg) {
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

    {
        int t_idx = thread_alloc_slot();
        if (t_idx >= 0) {
            const task_state_t *parent_state = task_state();
            thread_table[t_idx].tid = *tid_out;
            thread_table[t_idx].pid = parent_state ? parent_state->pid : *tid_out;
            thread_table[t_idx].state = THREAD_STATE_READY;
            thread_table[t_idx].user_stack_base = user_stack_base;
            thread_table[t_idx].user_stack_size = stack_size;
            thread_table[t_idx].priority = (attr && attr->priority)
                                             ? attr->priority
                                             : THREAD_DEFAULT_PRIORITY;
            thread_table[t_idx].time_slice = 2;
            thread_table[t_idx].magic = THREAD_MAGIC;
        }
    }

    return 0;
}

void thread_exit(int exit_code) {
    const task_state_t *s = task_state();
    if (s) {
        int idx = thread_find_by_pid(s->pid);
        if (idx >= 0) {
            thread_table[idx].state = THREAD_STATE_ZOMBIE;
            thread_table[idx].exit_value = exit_code;
            if (thread_current_idx == idx)
                thread_current_idx = -1;
        }
    }
    task_exit_current(exit_code);
}

int thread_join(u32 tid, int *exit_code_out) {
    int slot = task_find_slot_by_pid((int)tid);
    if (slot < 0) return -1;

    while (task_slot_valid(slot) && !task_slot_is_zombie(slot)) {
        cpu_halt();
        slot = task_find_slot_by_pid((int)tid);
        if (slot < 0) return -1;
    }

    if (exit_code_out) {
        *exit_code_out = task_slot_exit_code(slot);
    }

    int t_idx = thread_find_by_pid(tid);
    if (t_idx >= 0) thread_free_entry(t_idx);

    return 0;
}

int thread_detach(u32 tid) {
    int slot = task_find_slot_by_pid((int)tid);
    if (slot < 0) return -1;
    return 0;
}

void thread_yield(void) {
    cpu_pause();
}

u32 thread_self(void) {
    const task_state_t *s = task_state();
    return s ? s->pid : 0;
}

thread_t *thread_current(void) {
    if (thread_current_idx >= 0 && thread_current_idx < THREAD_MAX_THREADS &&
        thread_table[thread_current_idx].magic == THREAD_MAGIC) {
        return &thread_table[thread_current_idx];
    }

    const task_state_t *s = task_state();
    if (!s || !s->active) return NULL;

    int idx = thread_find_by_pid(s->pid);
    if (idx >= 0) {
        thread_current_idx = idx;
        thread_table[idx].state = THREAD_STATE_RUNNING;
        return &thread_table[idx];
    }

    return NULL;
}

void thread_exit_all(void) {
    for (int i = 0; i < THREAD_MAX_THREADS; ++i) {
        if (thread_table[i].magic == THREAD_MAGIC) {
            thread_free_entry(i);
        }
    }
    thread_current_idx = -1;
    prev_was_thread = false;
}

thread_t *thread_next_to_run(void) {
    int current = task_active_slot_index();
    for (int pass = 1; pass <= TASK_MAX_SLOTS; ++pass) {
        int idx = (current + pass) % TASK_MAX_SLOTS;
        if (task_slot_valid(idx) && task_is_thread(idx) && task_slot_is_runnable(idx)) {
            u32 pid = (u32)task_slot_pid(idx);
            int t_idx = thread_find_by_pid(pid);
            if (t_idx >= 0) {
                return &thread_table[t_idx];
            }
        }
    }
    return NULL;
}

void thread_schedule(void) {
    int current = task_active_slot_index();
    if (current < 0) return;

    registers_t regs;
    if (idt_last_user_frame(&regs)) {
        task_yield_current(&regs, 0);
    }
}

void thread_cleanup_process(u32 pid) {
    for (int i = 0; i < THREAD_MAX_THREADS; ++i) {
        if (thread_table[i].magic == THREAD_MAGIC && thread_table[i].pid == pid) {
            thread_free_entry(i);
        }
    }
}

int thread_check_preempt(registers_t *regs) {
    if (!regs || (regs->cs & 3u) != 3u)
        return -1;
    if (task_active_slot_index() < 0)
        return -1;
    if ((pit_ticks() & 1u) != 0u)
        return -1;
    return 0;
}

bool prev_thread_was_thread(void) {
    return prev_was_thread;
}

int thread_save_current(registers_t *regs) {
    if (!regs) return -1;

    const task_state_t *s = task_state();
    if (!s || !s->active) return -1;

    int t_idx = thread_find_by_pid(s->pid);
    if (t_idx < 0) return -1;

    thread_table[t_idx].regs = *regs;
    thread_table[t_idx].state = THREAD_STATE_READY;
    thread_current_idx = t_idx;
    prev_was_thread = true;

    return 0;
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
