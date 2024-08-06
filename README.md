# OpenHobbyOS

**License:** BSD 3-Clause

## What is this about?

OpenHobbyOS is a monolithic 32-bit x86 operating system written from scratch in C and Assembly. It features preemptive multitasking, a Linux-compatible syscall ABI, a graphical compositor, and support for running Doom, GTK4, Qt6, and other ported software.

What is special about OpenHobbyOS? Some notable properties of OpenHobbyOS are: (i) it implements a Linux-compatible system call interface (55 of the 83 syscalls use the same numbers and semantics as Linux i386), (ii) it supports a wide range of third-party software including GTK4, Qt6, FFmpeg, and Doom, and (iii) it includes features not typically found in hobby operating systems such as AES-256-XTS disk encryption, SMP, USB support, and TCP/IP networking.

## FAQ 

Origins ? OhOs kernel seperatly started all the way back in 2013. the kernel (which was a seperate project) was written by Michael .D. Flick and the first version was called "The ANARKIA kernel" . later changed to ElexerKernel or ElexerKern. it was a single time no maintainance collage project. as it was not meant to go any farther then a one time shot. but later it was extended farther into an indpendent closed source project at first. however in 2015 public open source maintainance offically started.

Is this a Linux distribution ? No, OpenHobbyOS runs its own kernel that does not originate from Linux. While OpenHobbyOS provides good compatibility at the user space level (Linux syscall ABI, ELF binaries, POSIX-like APIs), it does not share any source code or binaries with the Linux kernel.

Is it security focused ? not mainly. even with some safety features. these means the OS is secure to a limit only.

Why make your own OS in "insert a year" ? because its fun. and because its mainly a research project  that i started for collage so i dont expect anything more.

Why not just port x11 or wayland with such compatiblity surface ?. because i can. and because i dont want x11 and wayland assumptions running around. OhOs cares about peformance and speed under minimal requirements. both x11 and wayland ?. yeah i dont need talking.

Why should i use OhOs ? dont use it. seriously. its a hobby/research OS with potential momentum and capability. still not close to linux or windows scale. so trying to actually use it outside of testing or tinkering is basically giving your machine a hard time. dont use it expecting it to run the fancy RGB fans on your GPU. your gpu wont even work.

## Features

- Monolithic kernel with preemptive round-robin scheduling and multi-level priority (0-99)
- Paging with per-process page directories, copy-on-write fork, demand paging, and swap-to-disk
- 83 system calls via int 0x80 (55 Linux-compatible, 28 OHOS-specific)
- SMP support via MADT enumeration, SIPI trampoline, per-CPU GDT/TSS, LAPIC/IOAPIC
- VFS with initrd overlay, ext2 read/write, MBR partitioning, pipes, Unix domain sockets
- AES-256-XTS disk encryption at boot via mbedTLS
- Framebuffer console with TTF font rendering
- XNX compositor with z-ordering, title bars, mouse cursor, pixman-based rendering
- Cairo, FreeType, Pango, HarfBuzz text rendering stack
- TCP/IP networking via lwIP with RTL8139 and VirtIO-Net drivers
- USB UHCI host controller with hub, HID keyboard/mouse, and mass storage support
- Shutdown, reboot, and suspend via uACPI
- PS/2 keyboard and mouse drivers
- User authentication with SHA-256 hashed passwords

## Trying out OpenHobbyOS

If you want to try out OpenHobbyOS without building the whole OS, you can build the ISO and run it with QEMU:

```
make all
qemu-system-i386 -cdrom build/OpenHobbyOS.iso -no-reboot -no-shutdown -m 512M -display gtk
```

Or use the Makefile shortcuts:

```
make run          # Launch in QEMU (graphical window, 1280x1024)
make run-debug    # Run with serial output for debugging
make disk         # Create disk image with ext2
make run-disk     # Run with disk image (for disk encryption / ext2 testing)
```

The OS boots into a framebuffer console, launches the XNX compositor, and drops to a login prompt.

## Supported Software

Programs supported on OpenHobbyOS include:

- **GUI toolkits:** GTK4, Qt6 (with custom QPA plugin), QtDeclarative
- **Graphics stack:** pixman, Cairo, FreeType, Pango, HarfBuzz, Fontconfig, GDK-Pixbuf, GDK-Graphene, libepoxy
- **Multimedia:** FFmpeg, TinyGL (software OpenGL), Doom, gears demo
- **Desktop:** XNX compositor, GOSH (graphical shell), GTK demo apps
- **System:** GNU coreutils, Fastfetch, newlib C library
- **Networking:** lwIP TCP/IP stack
- **Utilities:** zlib, lodepng, expat, libtsm, MilkyWay, installer

## Supported Hardware

- **Graphics:** Generic framebuffer (VESA/VBE), virtio GPU, Bochs VBE, VMware SVGA
- **Input:** PS/2 keyboard and mouse, USB HID (via UHCI)
- **Storage:** ATA/ATAPI, USB mass storage (via UHCI). EXT
- **Network:** RTL8139, VirtIO-Net
- **Serial:** UART (8250/16550)
- **Power:** ACPI shutdown, reboot, and suspend via uACPI

## Building OpenHobbyOS

This repository contains the kernel, user-space programs, and build infrastructure. Third-party software (GTK4, Qt6, FFmpeg, etc.) is pulled in as git submodules during the build process.

See `docs/building.md` for full build instructions and requirements.

## Acknowledgements

OpenHobbyOS is built on top of many excellent open source projects: uACPI, lwIP, libtsm, pixman, Qt6, FFmpeg, TinyGL, mbedTLS, GNU coreutils, Cairo, FreeType, Pango, HarfBuzz, Fontconfig, GTK4, zlib, lodepng, expat, epoxy, GDK-Pixbuf, GDK-Graphene.

Artwork by Mark Sordestom.

Thanks to The University of Texas at Dallas for inspiring and starting the spark that pushed this project forward.