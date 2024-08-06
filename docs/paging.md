# Paging

OpenHobbyOS uses x86 32-bit paging with 4KB pages. Each process has its own page directory, and the kernel higher-half is shared across all processes.

## Page Directory

A page directory contains 1024 page directory entries (PDEs), each pointing to a page table. The PDEs are split into two regions:

- PDEs 0-767: User space (0 to 3GB)
- PDEs 768-1023: Kernel higher-half (3GB to 4GB)

When a new process is created, its page directory copies the kernel's PDEs 768-1023 directly. This means the kernel code and data are accessible from any process without remapping.

PDEs 0-223 in the kernel page directory provide identity mapping for the first 896MB of physical memory. These are shared with user page directories so the kernel heap remains accessible.

## Page Tables

Each page table contains 1024 page table entries (PTEs), each mapping a 4KB page. PTEs store the physical frame address plus flags:

- PTE_PRESENT: Page is in memory
- PTE_RW: Page is writable
- PTE_USER: Page is accessible from user mode
- PTE_COW: Copy-on-write page
- PTE_ALLOCATED: Frame was allocated by the kernel (not shared)
- PTE_DEMAND_ZERO: Demand-zero page (allocate on first access)
- PTE_SWAPPED: Page is swapped to disk

## Frame Allocator

The frame allocator uses a bitmap to track free physical pages. Each bit represents one 4KB frame. The bitmap is stored in the kernel's data section and initialized during boot from the Multiboot memory map.

frame_alloc() finds and returns the first free frame. frame_free() marks a frame as available. frame_free_count() returns the total number of free frames.

## TLB Management

The TLB (Translation Lookaside Buffer) caches page table lookups. When page tables are modified, the TLB must be invalidated.

paging_invalidate_tlb() invalidates a single TLB entry for a specific virtual address using the invlpg instruction. paging_flush_tlb() invalidates the entire TLB by reloading CR3.

## Copy-on-Write

fork() uses copy-on-write to avoid immediately duplicating all physical pages. Both parent and child share the same physical pages with read-only permissions. When either process writes to a shared page, the page fault handler detects the COW flag, allocates a new frame, copies the data, and maps the new frame writable for the faulting process.

## Demand Paging

Pages can be marked as demand-zero using PTE_DEMAND_ZERO. When accessed for the first time, the page fault handler allocates a frame, zeros it, and maps it. This allows efficient memory usage for programs that allocate large regions but only use a small portion.
