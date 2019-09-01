# OpenHobbyOS (OHOS)

![ohm!](assets/ohm.png)

OpenHobbyOS is a 32-bit x86 hobby operating system built from scratch. It features a monolithic kernel with paging, preemptive multitasking, a POSIX-like syscall interface, a VFS stack with initrd/ext2/devfs, a custom compositor (XNX), and a growing ecosystem of ported software including Qt6, Doom, FFmpeg, and more.

![screenshot2](screenshots/OH2.png)
![screenshot1](screenshots/OH1.png)

---

## Features

- **Preemptive multitasking** with a round-robin scheduler
- **80+ syscall numbers** via `int 0x80`, Linux ABI-compatible
- **Per-process page directories** with copy-on-write fork and fully enabled paging
- **VFS stack**: initrd (custom cpio-like), ext2 (read/write), devfs
- **libtsm framebuffer console** with full VT100 emulation, ANSI colors, scrollback; output to both framebuffer and serial
- **XNX display protocol**: Unix domain socket + pixman compositing with per-surface z-order, mouse cursor rendering, and 33ms frame timing
- **lwIP TCP/IP** stack with RTL8139 NIC driver
- **ACPI power management** via uACPI (shutdown, reboot, suspend)
- **newlib C library** cross-compiled for `i686-openhobbyos-elf`
- **Ported software**: fastfetch, Doom, XNX compositor + demo, FFmpeg, TinyGL, gears, lwIP, libtsm, uACPI, pixman, zlib, ohplay (audio player), Qt6 with QPA plugin

---

## Requirements

- IA-32 / i386 CPU or later (all modern x86-64 CPUs support 32-bit mode)
- 200-500 MB RAM
- VGA-compatible framebuffer
- CD-ROM or USB boot support

---

## Build dependencies

```bash
gcc-multilib (or i686-elf-gcc)
nasm >= 2.15
make, python3 >= 3.8, xorriso, grub-mkrescue, mtools
```

Optional for ports: `meson, ninja, pkg-config, autoconf, automake, cmake`

---

## Quick start

Full build and installation documentation: [`INSTALL.rst`](INSTALL.rst).

```bash
# Build everything (kernel, ports, initrd, ISO)
make all

# Run in QEMU
make run

# Run with serial debug output
make run-debug

# Clean build artifacts
make clean
```

The bootable ISO is produced at `build/ohos.iso`.

---

## Architecture

### Memory layout

```txt
0x00000000 - 0x0009FFFF: Low memory (reserved)
0x000A0000 - 0x000FFFFF: VGA/ROM (reserved)
0x00100000 - kernel_end:   Kernel code + data (identity mapped)
kernel_end - 0x02FF0000:   Kernel heap
0x03000000+:               User ELF loading (48MB+ available)
0x20000000+:               User mmap space (shared libs, mappings)
0x36100000:                User TLS (thread-local storage, per-process)
```

Paging is enabled on the CPU with per-process page directories.

### Syscall interface

Linux ABI-compatible via `int 0x80`:

- **File I/O**: open, read, write, close, lseek, ioctl, mmap, brk
- **Process**: fork, execve, wait, exit, getpid, spawn
- **IPC**: pipe, Unix domain sockets
- **Time**: clock_gettime, nanosleep, gettimeofday
- **OHOS-specific**: spawn, yield (syscall numbers 400+)

### Filesystem stack

```txt
VFS → initrd (read-only, boot) | ext2 (read/write, disk) | devfs (/dev/*)
Block layer: VFS → blkdev → ATA → Disk
```

- **initrd**: custom cpio-like archive embedded in the kernel containing all boot-time binaries
- **ext2**: full read/write support with inode, block group, and directory entry handling
- **devfs**: provides `/dev/fb0`, `/dev/null`, `/dev/zero`, `/dev/mouse`, `/dev/tty`, and console

### Graphics

- **libtsm**: terminal emulator linked into the kernel providing VT100 escape sequence handling, ANSI colors, and scrollback
- **XNX**: custom display protocol over Unix domain sockets. Clients create surfaces and push pixel buffers; the compositor renders with pixman and flushes to the hardware framebuffer at 33ms intervals

### Network

```txt
lwIP → netdev → RTL8139 PCI NIC
```

lwIP is ported as a userspace library. The RTL8139 driver performs PCI enumeration and MMIO-based I/O.

---

## Project history

OHOS originated from ElexerKernel, a kernel project started in 2013. The project was revived and expanded with a userspace, Linux ABI compatibility, and the current port ecosystem. The first publicly reproducible repository was published in 2025.

---

## Acknowledgments

This project builds on the work of many open-source projects:

- uACPI — power management
- lwIP — TCP/IP networking stack
- lodepng — PNG image loading
- zlib — compression
- pixman — pixel compositing
- doomgeneric — Doom port
- libtsm — VT100 terminal emulation
- fastfetch — system information
- FFmpeg — multimedia processing
- TinyGL — OpenGL subset implementation
- nuklear — immediate-mode GUI library
- Qt6 — cross-platform application framework

Special thanks to Mark Sordestom for their artwork contributions, and to all contributors who have helped improve this project.

---

## License

BSD 3-Clause. See `LICENSE` for details.
