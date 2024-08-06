# System Call Interface

OpenHobbyOS implements the Linux i386 system call ABI. Programs can be compiled against a Linux-compatible header set and use int 0x80 to invoke kernel services.

## Mechanism

System calls are invoked via the int 0x80 software interrupt. The syscall number goes in EAX. Arguments go in EBX, ECX, EDX, ESI, EDI, and EBP (in that order for syscalls with 6 or more arguments). The return value comes back in EAX.

The kernel's interrupt handler (ISR 128) saves all user registers, dispatches to the appropriate syscall handler, and restores registers before returning to user mode.

## Syscall Count

There are 83 total syscalls: 55 are Linux-compatible and 28 are OHOS-specific.

## Linux-Compatible Syscalls

These use the same numbers and semantics as Linux i386:

- Process: fork, execve, wait4, exit, getpid, getppid, setuid, getuid, setgid, getgid, kill, raise
- Memory: mmap2, munmap, brk, mprotect, madvise, msync, mremap
- Filesystem: open, read, write, close, lseek, stat64, fstat64, access, chdir, getcwd, mkdir, rmdir, link, unlink, rename, chmod, chown, dup, dup2,fcntl, ioctl, umask
- Pipes and sockets: pipe, socket, bind, connect, listen, accept, send, recv, sendto, recvfrom, shutdown, setsockopt, getsockopt, select
- Time: gettimeofday, clock_gettime, nanosleep
- Misc: uname, sysinfo, getdents64, times, ptrace, futex

## OHOS-Specific Syscalls

These are extensions for features not covered by the Linux ABI:

- Threading: thread_create, thread_exit, thread_join, thread_detach, thread_yield, thread_self
- Power: reboot, shutdown, suspend
- Auth: auth (user authentication with password hashing)
- Memory: memstat (heap diagnostics)
- Misc: pleasepanic (debug), memstat
