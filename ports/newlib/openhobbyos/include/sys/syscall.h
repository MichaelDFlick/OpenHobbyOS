#ifndef _SYS_SYSCALL_H
#define _SYS_SYSCALL_H

#include <features.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/* Linux/i386 ABI syscall numbers */
#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_link        9
#define SYS_execve      11
#define SYS_chdir       12
#define SYS_lseek       19
#define SYS_getpid      20
#define SYS_access      33
#define SYS_dup         41
#define SYS_pipe        42
#define SYS_brk         45
#define SYS_ioctl       54
#define SYS_fcntl       55
#define SYS_dup2        63
#define SYS_readlink    85
#define SYS_munmap      91
#define SYS_nanosleep   162
#define SYS_poll        168
#define SYS_getcwd      183
#define SYS_mmap2       192
#define SYS_stat64      195
#define SYS_fstat64     197
#define SYS_getuid32    199
#define SYS_getgid32    200
#define SYS_geteuid32   201
#define SYS_getegid32   202
#define SYS_getdents64  220
#define SYS_exit_group  252
#define SYS_clock_gettime 265
#define SYS_openat      295
#define SYS_readlinkat  305
#define SYS_socket      359
#define SYS_bind        361
#define SYS_connect     362
#define SYS_listen      363
#define SYS_accept      364
#define SYS_getsockname 367
#define SYS_getpeername 368
#define SYS_send        369
#define SYS_recv        370
#define SYS_sendto      371
#define SYS_recvfrom    372
#define SYS_shutdown    373
#define SYS_setsockopt  374
#define SYS_getsockopt  375
#define SYS_gettimeofday 78
#define SYS_rename      38
#define SYS_uname       122
#define SYS_readv       145
#define SYS_writev      146

#define SYS_gettid      224
#define SYS_futex       240

long syscall(long number, ...);

__END_DECLS

#endif
