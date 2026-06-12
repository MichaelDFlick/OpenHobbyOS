#include "syscall.h"

static int syscall0(int number) {
    int result;
    __asm__ volatile ("push %%ebx\n"
                      "int $0x80\n"
                      "pop %%ebx"
                      : "=a"(result)
                      : "a"(number)
                      : "memory", "cc");
    return result;
}

static int syscall1(int number, int arg1) {
    int result;
    __asm__ volatile ("int $0x80" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

static int syscall2(int number, int arg1, int arg2) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2)
                      : "memory");
    return result;
}

static int syscall3(int number, int arg1, int arg2, int arg3) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                      : "memory");
    return result;
}

static int syscall4(int number, int arg1, int arg2, int arg3, int arg4) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
                      : "memory");
    return result;
}

static int syscall5(int number, int arg1, int arg2, int arg3, int arg4, int arg5) {
    int result;
    __asm__ volatile ("int $0x80"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5)
                      : "memory");
    return result;
}

static int syscall6(int number, int arg1, int arg2, int arg3, int arg4, int arg5, int arg6) {
    int result;
    __asm__ volatile ("push %%ebp\n"
                      "mov %7, %%ebp\n"
                      "int $0x80\n"
                      "pop %%ebp\n"
                      : "=a"(result)
                      : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "r"(arg6)
                      : "memory");
    return result;
}

int sys_exit(int status) {
    return syscall1(LINUX_SYS_EXIT, status);
}

int sys_exit_group(int status) {
    return syscall1(LINUX_SYS_EXIT_GROUP, status);
}

int sys_read(int fd, void *buffer, unsigned int length) {
    return syscall3(LINUX_SYS_READ, fd, (int)buffer, (int)length);
}

int sys_write(int fd, const void *buffer, unsigned int length) {
    return syscall3(LINUX_SYS_WRITE, fd, (int)buffer, (int)length);
}

int sys_open(const char *path, int flags, int mode) {
    return syscall3(LINUX_SYS_OPEN, (int)path, flags, mode);
}

int sys_openat(int dirfd, const char *path, int flags, int mode) {
    return syscall4(LINUX_SYS_OPENAT, dirfd, (int)path, flags, mode);
}

int sys_close(int fd) {
    return syscall1(LINUX_SYS_CLOSE, fd);
}

int sys_unlink(const char *path) {
    return syscall1(OHOS_SYS_UNLINK, (int)path);
}

int sys_link(const char *oldpath, const char *newpath) {
    return syscall2(LINUX_SYS_LINK, (int)oldpath, (int)newpath);
}

int sys_rename(const char *oldpath, const char *newpath) {
    return syscall2(LINUX_SYS_RENAME, (int)oldpath, (int)newpath);
}

int sys_lseek(int fd, int offset, int whence) {
    return syscall3(LINUX_SYS_LSEEK, fd, offset, whence);
}

int sys_getpid(void) {
    return syscall1(LINUX_SYS_GETPID, 0);
}

int sys_pipe(int pipefd[2]) {
    return syscall1(LINUX_SYS_PIPE, (int)pipefd);
}

int sys_dup2(int oldfd, int newfd) {
    return syscall2(LINUX_SYS_DUP2, oldfd, newfd);
}

int sys_fork(void) {
    return syscall0(LINUX_SYS_FORK);
}

int sys_execve(const char *path, char *const argv[], char *const envp[]) {
    return syscall3(LINUX_SYS_EXECVE, (int)path, (int)argv, (int)envp);
}

void *sys_brk(void *requested) {
    return (void *)syscall1(LINUX_SYS_BRK, (int)requested);
}

int sys_uname(struct linux_utsname *name) {
    return syscall1(LINUX_SYS_UNAME, (int)name);
}

int sys_access(const char *path, int mode) {
    return syscall2(LINUX_SYS_ACCESS, (int)path, mode);
}

int sys_ioctl(int fd, unsigned int request, void *argp) {
    return syscall3(LINUX_SYS_IOCTL, fd, (int)request, (int)argp);
}

int sys_readlink(const char *path, char *buffer, unsigned int size) {
    return syscall3(LINUX_SYS_READLINK, (int)path, (int)buffer, (int)size);
}

int sys_readlinkat(int dirfd, const char *path, char *buffer, unsigned int size) {
    return syscall4(LINUX_SYS_READLINKAT, dirfd, (int)path, (int)buffer, (int)size);
}

int sys_writev(int fd, const struct linux_iovec *iov, int iovcnt) {
    return syscall3(LINUX_SYS_WRITEV, fd, (int)iov, iovcnt);
}

int sys_getcwd(char *buffer, unsigned int size) {
    return syscall2(LINUX_SYS_GETCWD, (int)buffer, (int)size);
}

int sys_chdir(const char *path) {
    return syscall1(LINUX_SYS_CHDIR, (int)path);
}

void *sys_mmap2(void *addr, unsigned int length, int prot, int flags, int fd, unsigned int page_offset) {
    return (void *)syscall6(LINUX_SYS_MMAP2, (int)addr, (int)length, prot, flags, fd, (int)page_offset);
}

int sys_munmap(void *addr, unsigned int length) {
    return syscall2(LINUX_SYS_MUNMAP, (int)addr, (int)length);
}

int sys_stat(const char *path, struct linux_stat64 *stat) {
    return syscall2(LINUX_SYS_STAT64, (int)path, (int)stat);
}

int sys_stat64(const char *path, struct linux_stat64 *stat) {
    return syscall2(LINUX_SYS_STAT64, (int)path, (int)stat);
}

int sys_fstat64(int fd, struct linux_stat64 *stat) {
    return syscall2(LINUX_SYS_FSTAT64, fd, (int)stat);
}

int sys_getdents64(int fd, void *buffer, unsigned int size) {
    return syscall3(LINUX_SYS_GETDENTS64, fd, (int)buffer, (int)size);
}

int sys_getuid32(void) {
    return syscall1(LINUX_SYS_GETUID32, 0);
}

int sys_getgid32(void) {
    return syscall1(LINUX_SYS_GETGID32, 0);
}

int sys_geteuid32(void) {
    return syscall1(LINUX_SYS_GETEUID32, 0);
}

int sys_getegid32(void) {
    return syscall1(LINUX_SYS_GETEGID32, 0);
}

int sys_clock_gettime(int clock_id, struct linux_timespec *spec) {
    return syscall2(LINUX_SYS_CLOCK_GETTIME, clock_id, (int)spec);
}

int sys_gettimeofday(struct linux_timeval *tv, void *tz) {
    return syscall2(LINUX_SYS_GETTIMEOFDAY, (int)tv, (int)tz);
}

int sys_nanosleep(const struct linux_timespec *req, struct linux_timespec *rem) {
    return syscall2(LINUX_SYS_NANOSLEEP, (int)req, (int)rem);
}

int sys_spawn(const char *path, const char *const argv[]) {
    return syscall3(OHOS_SYS_SPAWN, (int)path, (int)argv, 0);
}

int sys_waitpid(int pid, int *status, int options) {
    return syscall3(OHOS_SYS_WAITPID, pid, (int)status, options);
}

int sys_sched_yield(void) {
    return syscall0(OHOS_SYS_YIELD);
}

int sys_mkdir(const char *path, int mode) {
    return syscall2(OHOS_SYS_MKDIR, (int)path, mode);
}

int sys_chmod(const char *path, int mode) {
    return syscall2(OHOS_SYS_CHMOD, (int)path, mode);
}

int sys_auth(const char *username, const char *password) {
    return syscall2(OHOS_SYS_AUTH, (int)username, (int)password);
}

int sys_setuid(unsigned int uid) {
    return syscall1(OHOS_SYS_SETUID, (int)uid);
}

int sys_setgid(unsigned int gid) {
    return syscall1(OHOS_SYS_SETGID, (int)gid);
}

int sys_seteuid(unsigned int uid) {
    return syscall1(OHOS_SYS_SETEUID, (int)uid);
}

int sys_setegid(unsigned int gid) {
    return syscall1(OHOS_SYS_SETEGID, (int)gid);
}

int sys_thread_create(unsigned int *tid_out, const thread_attr_t *attr, unsigned int (*start_func)(void *), void *arg) {
    return syscall4(OHOS_SYS_THREAD_CREATE, (int)tid_out, (int)attr, (int)start_func, (int)arg);
}

int sys_thread_exit(int exit_code) {
    return syscall1(OHOS_SYS_THREAD_EXIT, exit_code);
}

int sys_thread_join(unsigned int tid, int *status) {
    return syscall2(OHOS_SYS_THREAD_JOIN, (int)tid, (int)status);
}

int sys_thread_detach(unsigned int tid) {
    return syscall1(OHOS_SYS_THREAD_DETACH, (int)tid);
}

int sys_thread_yield(void) {
    return syscall0(OHOS_SYS_THREAD_YIELD);
}

unsigned int sys_thread_self(void) {
    return (unsigned int)syscall0(OHOS_SYS_THREAD_SELF);
}

int sys_reboot(void) {
    return syscall0(OHOS_SYS_REBOOT);
}

int sys_shutdown(void) {
    return syscall0(OHOS_SYS_SHUTDOWN);
}

int sys_pleasepanic(void) {
    return syscall0(OHOS_SYS_PLEASEPANIC);
}

int sys_suspend(void) {
    return syscall0(OHOS_SYS_SUSPEND);
}

int sys_memstat(unsigned int buffer[5]) {
    return syscall1(OHOS_SYS_MEMSTAT, (int)buffer);
}

int sys_ticks(void) {
    return syscall0(OHOS_SYS_TICKS);
}

int sys_tickfreq(void) {
    return syscall0(OHOS_SYS_TICKFREQ);
}

int sys_blkread(unsigned int dev_id, unsigned int lba, void *buffer, unsigned int sectors) {
    return syscall4(OHOS_SYS_BLKREAD, (int)dev_id, (int)lba, (int)buffer, (int)sectors);
}

int sys_blkwrite(unsigned int dev_id, unsigned int lba, const void *buffer, unsigned int sectors) {
    return syscall4(OHOS_SYS_BLKWRITE, (int)dev_id, (int)lba, (int)buffer, (int)sectors);
}

int sys_install_op(unsigned int op, unsigned int arg1, unsigned int arg2, void *buf, unsigned int buf_size) {
    return syscall5(OHOS_SYS_INSTALL_OP, (int)op, (int)arg1, (int)arg2, (int)buf, (int)buf_size);
}

int sys_socket(int domain, int type, int protocol) {
    return syscall3(LINUX_SYS_SOCKET, domain, type, protocol);
}

int sys_connect(int fd, const void *addr, unsigned int addrlen) {
    return syscall3(LINUX_SYS_CONNECT, fd, (int)addr, (int)addrlen);
}

int sys_bind(int fd, const void *addr, unsigned int addrlen) {
    return syscall3(LINUX_SYS_BIND, fd, (int)addr, (int)addrlen);
}

int sys_listen(int fd, int backlog) {
    return syscall2(LINUX_SYS_LISTEN, fd, backlog);
}

int sys_accept(int fd, void *addr, unsigned int *addrlen) {
    return syscall3(LINUX_SYS_ACCEPT, fd, (int)addr, (int)addrlen);
}

int sys_send(int fd, const void *buf, unsigned int len, int flags) {
    return syscall4(LINUX_SYS_SEND, fd, (int)buf, (int)len, flags);
}

int sys_recv(int fd, void *buf, unsigned int len, int flags) {
    return syscall4(LINUX_SYS_RECV, fd, (int)buf, (int)len, flags);
}

int sys_sendto(int fd, const void *buf, unsigned int len, int flags, const void *dest_addr, unsigned int addrlen) {
    return syscall6(LINUX_SYS_SENDTO, fd, (int)buf, (int)len, flags, (int)dest_addr, (int)addrlen);
}

int sys_recvfrom(int fd, void *buf, unsigned int len, int flags, void *src_addr, unsigned int *addrlen) {
    return syscall6(LINUX_SYS_RECVFROM, fd, (int)buf, (int)len, flags, (int)src_addr, (int)addrlen);
}

int sys_poll(void *fds, unsigned int nfds, int timeout) {
    return syscall3(LINUX_SYS_POLL, (int)fds, (int)nfds, timeout);
}

int sys_sock_shutdown(int fd, int how) {
    return syscall2(LINUX_SYS_SHUTDOWN, fd, how);
}

int sys_setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen) {
    return syscall5(LINUX_SYS_SETSOCKOPT, fd, level, optname, (int)optval, (int)optlen);
}

int sys_getsockopt(int fd, int level, int optname, void *optval, unsigned int *optlen) {
    return syscall5(LINUX_SYS_GETSOCKOPT, fd, level, optname, (int)optval, (int)optlen);
}

int sys_getsockname(int fd, void *addr, unsigned int *addrlen) {
    return syscall3(LINUX_SYS_GETSOCKNAME, fd, (int)addr, (int)addrlen);
}

int sys_getpeername(int fd, void *addr, unsigned int *addrlen) {
    return syscall3(LINUX_SYS_GETPEERNAME, fd, (int)addr, (int)addrlen);
}

int sys_sendmsg(int fd, const void *msg, int flags) {
    return syscall3(OHOS_SYS_SENDMSG, fd, (int)msg, flags);
}

int sys_recvmsg(int fd, void *msg, int flags) {
    return syscall3(OHOS_SYS_RECVMSG, fd, (int)msg, flags);
}

int sys_dup(int oldfd) {
    return syscall1(LINUX_SYS_DUP, oldfd);
}
