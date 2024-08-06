# Threading

OpenHobbyOS provides POSIX-like threading support within a process. Threads share the same address space, file descriptors, and other process resources with the parent process.

## Thread Operations

thread_create() creates a new thread. It takes a function pointer, an argument, and optional attributes (stack size, detached state). The kernel allocates a separate kernel stack for the thread and sets up its register state to call the thread function with the given argument.

thread_join() blocks the calling thread until the specified thread exits. It retrieves the thread's exit value.

thread_detach() marks a thread as detached. Detached threads are automatically cleaned up when they exit, and cannot be joined.

thread_yield() voluntarily gives up the CPU time slice, allowing other threads to run.

thread_self() returns the current thread's ID.

thread_exit() terminates the current thread and stores an exit value that can be retrieved by a joining thread.

## Implementation

Threads are implemented as task slots that share the same page directory and file descriptor table as the parent process. Each thread has its own kernel stack for syscalls and interrupts, its own saved register state, and its own thread-local storage (TLS) segment.

The TLS segment is set up per-thread using the GDT. Each thread gets a unique TLS selector that points to its own TLS page. User-space code accesses TLS through the FS segment register.

## Work Queue

The kernel provides a work queue mechanism for deferred work. Functions can be submitted to the work queue and will be executed in kernel context during idle time or at scheduled intervals.
