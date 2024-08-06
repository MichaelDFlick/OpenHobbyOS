# Process Management

## Overview

OpenHobbyOS supports preemptive multitasking with process creation, execution, and termination. Each process has its own address space, file descriptor table, and scheduling context.

## Process Creation

Processes are created via fork() which duplicates the parent process. The child gets a copy of the parent's address space using copy-on-write, a copy of open file descriptors, and the same working directory and environment.

execve() replaces the current process image with a new ELF binary. The kernel loads the ELF segments into the process address space, sets up the stack with argc, argv, envp, and auxiliary vectors, and jumps to the entry point.

## Process Structure

Each process has a task_state_t structure containing:

- PID and parent PID
- UID and GID (real and effective)
- Page directory pointer
- Register state (saved for context switching)
- Open file descriptors (up to TASK_MAX_FDS)
- Memory mappings (up to TASK_MAX_MMAPS)
- Heap base, brk, and brk limit
- Working directory path
- Login name, home path, shell path
- Entry point and interpreter base
- Thread-local storage (TLS) information

## Scheduler

The scheduler is preemptive round-robin with multi-level priority. Priority levels range from 0 (highest, real-time) to 99 (lowest, normal). The PIT timer fires at 100 Hz and triggers rescheduling.

The scheduler maintains a run queue for each priority level. Higher-priority tasks are always scheduled before lower-priority ones. Within the same priority level, tasks take turns in round-robin fashion.

## Process Termination

A process exits by calling exit() or returning from main(). The kernel marks the task as inactive, closes open file descriptors, wakes up any parent waiting via waitpid(), and stores the exit code. The parent can retrieve the exit code through waitpid().

If a process is killed by a signal or page fault, the kernel aborts it the same way but sets the exit code to indicate the abnormal termination reason.

## Context Switching

Context switching is handled in assembly (task.asm). When the scheduler picks a new process, it saves the current process's registers (EBX, ESI, EDI, EBP, ESP) and restores the new process's registers. The page directory is switched by loading the new process's CR3.

For user-space transitions, the kernel pushes SS, ESP, EFLAGS, CS, and EIP onto the kernel stack and executes iretd to return to user mode.
