# Architecture Overview

OpenHobbyOS is a monolithic 32-bit x86 operating system. The kernel runs in ring 0 at the higher-half of the virtual address space. User programs run in ring 3 at the lower half. Communication between kernel and user space happens through system calls via the int 0x80 interrupt.

## Kernel Structure

The kernel is a single binary that contains all core subsystems, drivers, and the VFS. There is no microkernel separation. Everything runs in kernel space with full hardware access.

Key subsystems:

- Memory management (paging, heap, swap)
- Process management (scheduling, fork, exec, exit)
- Threading (POSIX-like threads with join, detach, yield)
- Filesystem (VFS, initrd, ext2, pipes, sockets)
- Networking (lwIP TCP/IP stack)
- Graphics (framebuffer console, XNX compositor)
- Device drivers (ATA, PCI, PS/2, USB, NIC)
- Power management (shutdown, reboot, suspend via uACPI)

## User Space

User programs are statically linked ELF32 binaries. They use a C runtime built on newlib with a custom gloss layer that provides system call wrappers. The user-space linker script places code at 0x03000000.

User programs access kernel services exclusively through the 83 system calls exposed via int 0x80. There are no IOCTL-style kernel module interfaces beyond what the syscalls provide.

## Address Space

Each process gets its own page directory. The kernel higher-half (3GB-4GB) is shared across all processes. User-space pages are per-process and use copy-on-write for fork.

## Scheduling

The scheduler is a preemptive round-robin with multi-level priority levels 0 through 99. Real-time priorities (0-49) get preference over normal priorities (50-99). A PIT timer fires at 100 Hz to trigger rescheduling.

## SMP

Symmetric multiprocessing is supported. The kernel enumerates CPUs via the ACPI MADT table, sends SIPI (Startup IPI) to application processors, and sets up per-CPU GDT and TSS structures. Inter-processor interrupts handle TLB shootdown and rescheduling.
