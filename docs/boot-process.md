# Boot Process

## Overview

OpenHobbyOS boots through GRUB using the Multiboot v1 protocol. The boot process goes through several stages from the initial assembly entry point to running user-space programs.

## Stage 1: GRUB

GRUB loads kernel.bin and initrd.bin into memory. It passes a Multiboot info structure to the kernel containing memory map, framebuffer address, and module information. The kernel is loaded at 1MB physical (0x100000).

## Stage 2: boot.asm

The entry point is in src/arch/x86/boot.asm. It does the following:

1. Checks that the Multiboot magic number is valid
2. Sets up a 32KB kernel stack at the top of the BSS section
3. Zeros the BSS section
4. Passes the Multiboot info pointer and a magic value to kernel_main()
5. Calls kernel_main()

## Stage 3: kernel_main

kernel_main in src/core/kernel.c runs the full initialization sequence:

1. Serial port output for early debugging
2. CPU identification via CPUID
3. GDT setup with kernel and user segments, plus per-CPU TSS
4. PIC remapping (8259)
5. IDT setup with 256 interrupt vectors
6. PIT calibration at 100 Hz
7. PS/2 keyboard driver initialization
8. PS/2 mouse driver initialization
9. Kernel heap initialization
10. Paging setup with identity mapping and higher-half mapping
11. APIC and IOAPIC initialization
12. Initrd mounting from Multiboot module
13. ATA disk detection
14. Disk encryption unlock (if disk present, prompts for passphrase)
15. VFS initialization with initrd overlay
16. ext2 filesystem mount (if disk present)
17. PCI bus enumeration
18. Network driver initialization (RTL8139, VirtIO-Net)
19. USB host controller initialization (UHCI)
20. SMP initialization (MADT enumeration, SIPI trampoline)
21. Scheduler startup
22. First user process launch (XNX compositor or GOSH)

## Stage 4: User Space

The kernel spawns the first user process. By default this is the XNX compositor at /bin/xnx-compositor. The compositor creates a framebuffer surface and starts a windowing environment.

After the compositor (or if it is not available), the kernel launches /bin/login which presents a login prompt. After authentication, the user gets a shell (GOSH or /bin/sh).

## GRUB Configuration

The GRUB config is at grub/grub.cfg:

- Sets display mode to 1280x1024x32
- Loads kernel.bin via multiboot
- Loads initrd.bin as a module
- Boots immediately without a menu timeout
