/* Newlib glue - connects newlib syscalls to OpenHobbyOS kernel syscalls */

#include <stddef.h>
#include "abi/linux.h"
#include "syscall.h"

/* Minimal type definitions for newlib compatibility */
typedef long off_t;
typedef long clock_t;

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

struct stat {
    unsigned long st_dev;
    unsigned int st_mode;
    unsigned long st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

/* File mode bits */
#define S_IFCHR 0x2000
#define S_IFMT  0xF000

/* Error codes */
#define ESRCH 3

/* Provide errno for newlib reentrant functions */
int errno;

/* File descriptor table - tracks which fds are open */
static char fd_open[256] = {0};

/* _open - open a file (newlib syscall) */
int _open(const char *file, int flags, int mode) {
    int fd = sys_open(file, flags, mode);
    if (fd >= 0 && fd < 256) {
        fd_open[fd] = 1;
    }
    return fd;
}

/* _read - read from file descriptor */
int _read(int fd, char *buf, int len) {
    return sys_read(fd, buf, len);
}

/* _write - write to file descriptor */
int _write(int fd, const char *buf, int len) {
    return sys_write(fd, buf, len);
}

/* _close - close file descriptor */
int _close(int fd) {
    int ret = sys_close(fd);
    if (fd >= 0 && fd < 256) {
        fd_open[fd] = 0;
    }
    return ret;
}

/* _fstat - get file stats (required for stdio) */
int _fstat(int fd, struct stat *st) {
    /* Minimal implementation - just fill basic fields */
    st->st_mode = S_IFCHR;  /* Character device (simplifies things) */
    st->st_size = 0;
    st->st_blksize = 512;
    st->st_blocks = 0;
    return 0;
}

/* _lseek - seek in file */
off_t _lseek(int fd, off_t offset, int whence) {
    return sys_lseek(fd, offset, whence);
}

/* _isatty - check if fd is a terminal */
int _isatty(int fd) {
    return (fd == 0 || fd == 1 || fd == 2);  /* stdin/stdout/stderr are tty */
}

/* _stat - get file stats by path */
int _stat(const char *path, struct stat *st) {
    struct linux_stat64 lst;
    int ret = sys_stat(path, &lst);
    if (ret == 0) {
        st->st_dev = lst.st_dev;
        st->st_mode = lst.st_mode;
        st->st_size = lst.st_size;
    }
    return ret;
}

/* _getpid - get process ID */
int _getpid(void) {
    return sys_getpid();
}

/* _kill - send signal to process */
int _kill(int pid, int sig) {
    /* OpenHobbyOS doesn't have signals, just return success for pid 0 (self) */
    if (pid == 0) return 0;
    errno = ESRCH;
    return -1;
}

/* _exit - exit program */
void _exit(int status) {
    sys_exit_group(status);
    while (1);  /* Should never reach here */
}

/* _pipe - create pipe */
int _pipe(int pipefd[2]) {
    return sys_pipe(pipefd);
}

/* _dup2 - duplicate file descriptor */
int _dup2(int oldfd, int newfd) {
    return sys_dup2(oldfd, newfd);
}

/* _fork - fork process */
int _fork(void) {
    return sys_fork();
}

/* _execve - execute program */
int _execve(const char *path, char *const argv[], char *const envp[]) {
    return sys_execve(path, argv, envp);
}

/* _waitpid - wait for process */
int _waitpid(int pid, int *status, int options) {
    return sys_waitpid(pid, status, options);
}

/* _gettimeofday - get time of day */
int _gettimeofday(struct timeval *tv, void *tz) {
    struct linux_timeval ltv;
    int ret = sys_gettimeofday(&ltv, tz);
    if (ret == 0 && tv) {
        tv->tv_sec = ltv.tv_sec;
        tv->tv_usec = ltv.tv_usec;
    }
    return ret;
}

/* initstate - BSD random state initialization (stub for fontconfig compat) */
char *initstate(unsigned seed, char *state, size_t n) {
    (void)seed;
    (void)state;
    (void)n;
    return state;
}

/* setstate - BSD random state switching (stub for fontconfig compat) */
char *setstate(char *state) {
    return state;
}

/* _times - get process times */
clock_t _times(struct tms *buf) {
    return -1;  /* Not implemented (sorry not sorry) */
}

/* _ioctl - device control */
int _ioctl(int fd, unsigned int request, void *argp) {
    return sys_ioctl(fd, request, argp);
}

/* _mmap - map files or devices into memory */
void *_mmap(void *addr, unsigned int length, int prot, int flags, int fd, long offset) {
    /* newlib mmap: offset is in bytes, kernel mmap2 expects page offset */
    unsigned int page_offset = (unsigned int)((unsigned long)offset >> 12);
    return sys_mmap2(addr, length, prot, flags, fd, page_offset);
}

/* _munmap - unmap files or devices from memory */
int _munmap(void *addr, unsigned int length) {
    return sys_munmap(addr, length);
}

/* _poll - wait for events on file descriptors */
int _poll(void *fds, unsigned int nfds, int timeout) {
    return sys_poll(fds, nfds, timeout);
}

/* _socket - create a socket */
int _socket(int domain, int type, int protocol) {
    return sys_socket(domain, type, protocol);
}

/* _connect - initiate a connection on a socket */
int _connect(int fd, const void *addr, unsigned int addrlen) {
    return sys_connect(fd, addr, addrlen);
}

/* _bind - bind a socket to an address */
int _bind(int fd, const void *addr, unsigned int addrlen) {
    return sys_bind(fd, addr, addrlen);
}

/* _listen - listen for connections on a socket */
int _listen(int fd, int backlog) {
    return sys_listen(fd, backlog);
}

/* _accept - accept a connection on a socket */
int _accept(int fd, void *addr, unsigned int *addrlen) {
    return sys_accept(fd, addr, addrlen);
}

/* _send - send a message on a socket */
int _send(int fd, const void *buf, unsigned int len, int flags) {
    return sys_send(fd, buf, len, flags);
}

/* _recv - receive a message from a socket */
int _recv(int fd, void *buf, unsigned int len, int flags) {
    return sys_recv(fd, buf, len, flags);
}

/* _sendto - send a message on a socket */
int _sendto(int fd, const void *buf, unsigned int len, int flags, const void *dest_addr, unsigned int addrlen) {
    return sys_sendto(fd, buf, len, flags, dest_addr, addrlen);
}

/* _recvfrom - receive a message from a socket */
int _recvfrom(int fd, void *buf, unsigned int len, int flags, void *src_addr, unsigned int *addrlen) {
    return sys_recvfrom(fd, buf, len, flags, src_addr, addrlen);
}

/* _shutdown - shut down part of a socket */
int _shutdown(int fd, int how) {
    return sys_sock_shutdown(fd, how);
}

/* _setsockopt - set socket option */
int _setsockopt(int fd, int level, int optname, const void *optval, unsigned int optlen) {
    return sys_setsockopt(fd, level, optname, optval, optlen);
}

/* _getsockopt - get socket option */
int _getsockopt(int fd, int level, int optname, void *optval, unsigned int *optlen) {
    return sys_getsockopt(fd, level, optname, optval, optlen);
}

/* _getsockname - get socket name */
int _getsockname(int fd, void *addr, unsigned int *addrlen) {
    return sys_getsockname(fd, addr, addrlen);
}

/* _getpeername - get name of connected peer socket */
int _getpeername(int fd, void *addr, unsigned int *addrlen) {
    return sys_getpeername(fd, addr, addrlen);
}

/* _sendmsg - send a message on a socket */
int _sendmsg(int fd, const void *msg, int flags) {
    return sys_sendmsg(fd, msg, flags);
}

/* _recvmsg - receive a message from a socket */
int _recvmsg(int fd, void *msg, int flags) {
    return sys_recvmsg(fd, msg, flags);
}

/* _unlink - delete a name from the filesystem */
int _unlink(const char *path) {
    return sys_unlink(path);
}

/* _dup - duplicate a file descriptor */
int _dup(int oldfd) {
    return sys_dup(oldfd);
}

/* _sbrk - adjust program break (for malloc) */
void *_sbrk(ptrdiff_t incr) {
    void *old = sys_brk((void *)0);
    if (incr == 0 || old == (void *)-1) {
        return old;
    }
    void *new = (void *)((uintptr_t)old + incr);
    if (incr < 0 && (uintptr_t)new > (uintptr_t)old) {
        return (void *)-1; /* wraparound */
    }
    if (incr > 0 && (uintptr_t)new < (uintptr_t)old) {
        return (void *)-1; /* wraparound */
    }
    void *result = sys_brk(new);
    if (result != new) {
        return (void *)-1; /* kernel refused expansion */
    }
    return old;
}
