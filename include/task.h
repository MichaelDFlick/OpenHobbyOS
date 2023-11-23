#ifndef OHOS_TASK_H
#define OHOS_TASK_H

#include "idt.h"
#include "paging.h"
#include "signal.h"
#include "vfs.h"
#include "types.h"

/* User space from 32MB to 3GB.  GLib/GTK hardcode 0x58000000 as mmap
 * addresses for internal shared memory — the layout must match Linux-like
 * user-space limits.  The kernel identity-maps 0-896MB and higher-half-maps
 * the same range above 0xC0000000; user page directories have 768 PDEs
 * covering 0-3GB. */
#define USER_BASE      0x02000000u
#define USER_LIMIT     0xC0000000u
#define USER_STACK_TOP 0xBFF00000u
#define USER_TLS_ADDR  0xBFF00000u
#define USER_MMAP_BASE 0x20000000u
#define USER_MMAP_TOP  0xBE000000u

/* Interpreter (dynamic linker) region — just below the stack. */
#define INTERP_BASE    0xBF000000u
#define INTERP_LIMIT   0xBF800000u
#define TASK_MAX_FDS   32
#define TASK_MAX_MMAPS 16
#define TASK_MAX_SLOTS 24

/* Scheduling priorities */
#define TASK_PRIO_RT        0       /* Real-time (highest) */
#define TASK_PRIO_HIGH      25
#define TASK_PRIO_NORMAL    50      /* Default */
#define TASK_PRIO_LOW       75
#define TASK_PRIO_IDLE      99      /* Idle priority (lowest) */

typedef enum {
    TASK_FD_FREE = 0,
    TASK_FD_CONSOLE,
    TASK_FD_NULL,
    TASK_FD_FILE,
    TASK_FD_DIR,
    TASK_FD_SOCKET,
    TASK_FD_PIPE_READ,
    TASK_FD_PIPE_WRITE,
    TASK_FD_FB0,
    TASK_FD_NET,
    TASK_FD_KEYBOARD,
    TASK_FD_MOUSE,
} task_fd_kind_t;

typedef struct {
    bool used;
    task_fd_kind_t kind;
    int flags;
    u32 offset;
    const vfs_node_t *node;
    void *socket;
    void *pipe;
} task_fd_t;

task_fd_t *task_fd_slot(int fd);
int task_alloc_fd(void);

typedef struct {
    bool used;
    u32 base;
    u32 length;
    int prot;
    int flags;
    const vfs_node_t *node;
    u32 file_offset;
} task_mapping_t;

typedef struct {
    char name[64];
    char path[VFS_PATH_MAX];
    char cwd_path[VFS_PATH_MAX];
    char login_name[32];
    char home_path[VFS_PATH_MAX];
    char shell_path[VFS_PATH_MAX];
    u32 entry;
    u32 brk_base;
    u32 brk;
    u32 brk_limit;
    const vfs_node_t *cwd;
    u32 pid;
    u32 uid;
    u32 gid;
    u32 euid;
    u32 egid;
    bool active;
    task_fd_t fds[TASK_MAX_FDS];
    task_mapping_t mappings[TASK_MAX_MMAPS];
    page_directory_t *page_directory;   /* Process page directory */
    u32 page_directory_phys;            /* Physical address of PD */
    
    /* Scheduling priority (0-99, lower = higher priority) */
    u32 priority;
    
    /* Signal state */
    signal_state_t signal_state;

    /* Dynamic linker support */
    u32 interp_base;
    u32 user_entry;
    u32 phdr_vaddr;
    u32 phnum;

    /* FPU state (512-byte FXSAVE/FXRSTOR region, 16-byte aligned) */
    u8 fpu_state[512] __attribute__((aligned(16)));
    bool fpu_used;
} task_state_t;

extern task_state_t current_task;

void task_init(void);
bool task_can_run(void);
bool task_is_active(void);
int task_active_slot_index(void);
const task_state_t *task_state(void);
int task_authenticate_session(const char *username, const char *password);
int task_set_credentials(u32 uid, u32 gid, u32 euid, u32 egid);
int task_run_path(const char *path);
int task_spawn_background(const char *path);
void task_run_background_init(void);
int task_run_argv(const char *path, int argc, const char *const *argv);
int task_run_argv_from(const task_state_t *parent_state, const char *path, int argc, const char *const *argv);
int task_run_argv_alongside(const task_state_t *parent_state, const char *path, int argc, const char *const *argv);
bool task_validate_user_range(uintptr_t base, size_t length);
bool task_copy_from_user(void *dest, const void *user_src, size_t length);
bool task_copy_to_user(void *user_dest, const void *src, size_t length);
bool task_copy_string_from_user(char *dest, size_t dest_size, const char *user_src);
u32 task_brk(u32 requested);
int task_open(const char *path, int flags, int mode);
int task_open_relative(int dirfd, const char *path, int flags, int mode);
int task_install_file_fd(const vfs_node_t *node, int flags, u32 offset);
int task_socket_fd(void *socket);
void *task_get_socket(int fd);
int task_close(int fd);
ssize_t task_read_fd(int fd, void *buffer, size_t length);
ssize_t task_write_fd(int fd, const void *buffer, size_t length);
i32 task_lseek(int fd, i32 offset, int whence);
int task_dup(int oldfd, int newfd_hint);
bool task_is_console_fd(int fd);
int task_access(const char *path, int mode);
int task_stat_path(const char *path, void *user_stat, size_t stat_size);
int task_stat_fd(int fd, void *user_stat, size_t stat_size);
int task_getdents64(int fd, void *user_buffer, size_t length);
int task_getcwd(void *user_buffer, size_t length);
int task_chdir(const char *path);
int task_mkdir(const char *path, u32 mode);
int task_chmod(const char *path, u32 mode);
int task_unlink(const char *path);
int task_link(const char *oldpath, const char *newpath);
int task_rename(const char *oldpath, const char *newpath);
void *task_mmap(void *addr, size_t length, int prot, int flags, int fd, u32 page_offset);
int task_munmap(void *addr, size_t length);
int task_fork_from_user(registers_t *regs);
int task_execve_from_user(const char *user_path, const u32 *user_argv, const u32 *user_envp, registers_t *regs);
void task_timer_tick(registers_t *regs);
int task_spawn_from_user(const char *user_path, const u32 *user_argv);
int task_waitpid_from_user(int pid, void *status_ptr, int options, registers_t *regs);
NORETURN void task_sleep_current(registers_t *regs, u32 wake_tick, int return_value);
NORETURN void task_yield_current(registers_t *regs, int return_value);
NORETURN void task_exit_current(int exit_code);
bool task_is_thread(int slot_index);

NORETURN void task_abort_from_trap(registers_t *regs, const char *reason);
void task_wake_slot(int slot, int return_value);
bool task_write_slot_user(int slot, u32 user_addr, const void *src, size_t length);
int task_slot_install_socket(int slot, void *socket);
bool path_is_in_tmp(const char *path);

/* Task slot access (for thread system) */
int task_slot_pid(int index);
bool task_slot_valid(int index);
int task_count_total(void);
int task_count_runnable(void);
u32 task_slot_page_dir_phys(int index);
u32 task_slot_tls_vaddr(int index);
void *task_slot_page_dir(int index);
int task_find_slot_by_pid(int pid);
bool task_slot_regs(int index, registers_t *out);
int task_create_thread_slot(u32 (*start_func)(void*), void *arg,
                            u32 user_stack_top, u32 user_stack_base, u32 user_stack_size);
void task_free_slot_by_index(int index);
bool task_slot_is_runnable(int index);
bool task_slot_is_zombie(int index);
int task_slot_exit_code(int index);
int task_slot_parent_pid(int index);

extern uintptr_t task_saved_esp;
extern int task_exit_code;

void task_resume_user(const registers_t *regs);
NORETURN void task_return_to_kernel(int exit_code);

/* FPU support */
void fpu_init(void);
void fpu_nm_handler(registers_t *regs);

#endif
