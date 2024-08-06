# Memory Management

OpenHobbyOS implements a full memory management subsystem for 32-bit x86. It includes physical frame allocation, virtual paging with per-process page directories, a kernel heap with defragmentation, copy-on-write fork, demand paging, and swap-to-disk.

## Physical Memory

The frame allocator uses a bitmap to track free physical pages. It is initialized during boot by parsing the Multiboot memory map to find available RAM regions. The allocator provides frame_alloc() and frame_free() for obtaining and releasing 4KB physical pages.

## Virtual Memory

Each process has its own page directory. The kernel higher-half (3GB-4GB) is shared across all processes by copying PDEs 768-1023 from the kernel page directory. User-space pages are per-process and start at USER_BASE (0x02000000).

The address space layout:

- 0x00000000 to 0x00000FFF: Null page (unmapped, traps NULL dereferences)
- 0x02000000: USER_BASE (start of user space)
- 0x20000000: USER_MMAP_BASE (start of mmap region)
- 0xBE000000: USER_MMAP_TOP (end of mmap region)
- 0xBF000000: INTERP_BASE (dynamic linker load address)
- 0xBFD00000: USER_STACK_TOP - 2MB (user stack base)
- 0xBFF00000: USER_STACK_TOP (top of user stack, 2MB for Qt6)
- 0xC0000000: KERNEL_VIRTUAL_BASE (start of kernel higher-half)

## Kernel Heap

The kernel heap starts right after the kernel BSS section. It uses a block-based allocator with free list tracking. memory_defragment() merges adjacent free blocks. memory_dump_free() provides heap diagnostics.

## Copy-on-Write

fork() uses copy-on-write. The parent and child share physical pages with read-only permissions. When either process writes to a shared page, the page fault handler allocates a new frame, copies the data, and maps it writable for the faulting process.

## Demand Paging

Pages can be demand-paged using PTE_DEMAND_ZERO flags. When a demand-zero page is accessed, the page fault handler allocates a frame, zeros it, and maps it. This avoids allocating physical memory until it is actually needed.

## Swap

The swap subsystem stores evicted pages to the last 32MB of the boot disk. It uses a bitmap-based slot tracker to manage swap slots. Pages are encrypted before writing to disk.
