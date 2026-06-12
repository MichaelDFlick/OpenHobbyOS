#ifndef OHOS_USER_SYSCALL_H
#define OHOS_USER_SYSCALL_H

#include "abi/linux.h"
#include "thread.h"

#ifdef __cplusplus
extern "C" {
#endif

int sys_exit(int status);
int sys_exit_group(int status);
int sys_read(int fd, void *buffer, unsigned int length);
int sys_write(int fd, const void *buffer, unsigned int length);
int sys_open(const char *path, int flags, int mode);
int sys_openat(int dirfd, const char *path, int flags, int mode);
int sys_close(int fd);
int sys_lseek(int fd, int offset, int whence);
int sys_getpid(void);
int sys_pipe(int pipefd[2]);
int sys_dup2(int oldfd, int newfd);
int sys_fork(void);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
void *sys_brk(void *requested);
int sys_uname(struct linux_utsname *name);
int sys_access(const char *path, int mode);
int sys_ioctl(int fd, unsigned int request, void *argp);
int sys_readlink(const char *path, char *buffer, unsigned int size);
int sys_readlinkat(int dirfd, const char *path, char *buffer, unsigned int size);
int sys_writev(int fd, const struct linux_iovec *iov, int iovcnt);
int sys_getcwd(char *buffer, unsigned int size);
int sys_chdir(const char *path);
void *sys_mmap2(void *addr, unsigned int length, int prot, int flags, int fd, unsigned int page_offset);
int sys_munmap(void *addr, unsigned int length);
int sys_stat(const char *path, struct linux_stat64 *stat);
int sys_stat64(const char *path, struct linux_stat64 *stat);
int sys_link(const char *oldpath, const char *newpath);
int sys_rename(const char *oldpath, const char *newpath);
int sys_unlink(const char *path);
int sys_fstat64(int fd, struct linux_stat64 *stat);
int sys_getdents64(int fd, void *buffer, unsigned int size);
int sys_getuid32(void);
int sys_getgid32(void);
int sys_geteuid32(void);
int sys_getegid32(void);
int sys_clock_gettime(int clock_id, struct linux_timespec *spec);
int sys_gettimeofday(struct linux_timeval *tv, void *tz);
int sys_nanosleep(const struct linux_timespec *req, struct linux_timespec *rem);
int sys_spawn(const char *path, const char *const argv[]);
int sys_waitpid(int pid, int *status, int options);
int sys_sched_yield(void);
int sys_mkdir(const char *path, int mode);
int sys_chmod(const char *path, int mode);
int sys_auth(const char *username, const char *password);
int sys_setuid(unsigned int uid);
int sys_setgid(unsigned int gid);
int sys_seteuid(unsigned int uid);
int sys_setegid(unsigned int gid);
int sys_thread_create(unsigned int *tid_out, const thread_attr_t *attr, unsigned int (*start_func)(void *), void *arg);
int sys_thread_exit(int exit_code);
int sys_thread_join(unsigned int tid, int *status);
int sys_thread_detach(unsigned int tid);
int sys_thread_yield(void);
unsigned int sys_thread_self(void);
int sys_reboot(void);
int sys_shutdown(void);
int sys_pleasepanic(void);
int sys_suspend(void);
int sys_memstat(unsigned int buffer[5]);
int sys_ticks(void);
int sys_tickfreq(void);
int sys_blkread(unsigned int dev_id, unsigned int lba, void *buffer, unsigned int sectors);
int sys_blkwrite(unsigned int dev_id, unsigned int lba, const void *buffer, unsigned int sectors);
int sys_install_op(unsigned int op, unsigned int arg1, unsigned int arg2, void *buf, unsigned int buf_size);

int sys_socket(int domain, int type, int protocol);
int sys_connect(int fd, const void *addr, unsigned int addrlen);
int sys_bind(int fd, const void *addr, unsigned int addrlen);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, void *addr, unsigned int *addrlen);
int sys_send(int fd, const void *buf, unsigned int len, int flags);
int sys_recv(int fd, void *buf, unsigned int len, int flags);
int sys_sendto(int fd, const void *buf, unsigned int len, int flags, const void *dest_addr, unsigned int addrlen);
int sys_recvfrom(int fd, void *buf, unsigned int len, int flags, void *src_addr, unsigned int *addrlen);
int sys_poll(void *fds, unsigned int nfds, int timeout);
int sys_sock_shutdown(int fd, int how);
int sys_setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen);
int sys_getsockopt(int fd, int level, int optname, void *optval, unsigned int *optlen);
int sys_getsockname(int fd, void *addr, unsigned int *addrlen);
int sys_getpeername(int fd, void *addr, unsigned int *addrlen);
int sys_sendmsg(int fd, const void *msg, int flags);
int sys_recvmsg(int fd, void *msg, int flags);
int sys_dup(int oldfd);

#ifdef __cplusplus
}
#endif

#endif
