## OpenHobbyOS (OHOS) — Lightweight. Blazingly Fast. Secure.

![ohm!](assets/ohm.png)

Welcome to OpenHobbyOS (OHOS), a 32-bit x86 hobby operating system built from the ground up with security as a first-class citizen, not an afterthought.

OHOS is our labor of love. a project born from a desire to understand how operating systems really work, from the first line of assembly to running complex graphical applications. What started as a kernel experiment has grown into a functional, POSIX-like environment capable of running everything from retro games like Doom to modern frameworks like Qt6 — all while keeping the footprint small and the attack surface minimal.

We believe that building an OS should be accessible, educational, and fun. Whether you're here to dive into the kernel source or just want to see how much we can squeeze out of a custom 32-bit architecture, we're glad to have you.

![screenshot2](screenshots/OH2.png)
![screenshot1](screenshots/OH1.png)

> A Quick Note: While OHOS has come a long way, it remains an active hobby project under constant development. It's a fantastic learning platform, but we don't recommend using it for critical production workloads just yet!

---

## What's Under the Hood?

We've focused on creating a balanced environment that feels familiar to Linux users while maintaining the unique architecture of a custom hobbyist system — one that prioritizes security, performance, and simplicity:

* Kernel Core: A monolithic design with preemptive multitasking and a round-robin scheduler to keep things snappy.
* Memory Management: Full paging support with per-process page directories and copy-on-write fork for efficient process management.
* Compatibility: We target Linux ABI compatibility via an int 0x80 syscall interface, offering 80+ syscalls to keep your favorite ported apps running smoothly.
* VFS Stack: A robust virtual filesystem supporting initrd, devfs, and full read/write ext2 filesystems.
* Disk Encryption: Full AES-256-XTS disk encryption at boot using mbedTLS. The kernel prompts for a passphrase before mounting the root filesystem and transparently encrypts/decrypts every sector on the fly. The key is derived via PBKDF2-HMAC-SHA256 with a random salt — your data stays encrypted at rest, and we never store the passphrase.
* Graphics & UI: Our custom XNX compositor handles windowing and z-ordering, backed by pixman. We also include libtsm for a high-quality, VT100-compliant terminal emulator.
* Networking: Full TCP/IP support via lwIP, driven by our custom RTL8139 PCI NIC driver.
* Power Management: Native support for shutdown, reboot, and suspend using uACPI.

---

## What Can It Run?

We've put a lot of work into our i686-openhobbyos-elf toolchain so you can bring familiar software to our ecosystem. Current ports include:

* Multimedia: FFmpeg and ohplay (our custom audio player).
* Gaming: A working port of Doom and TinyGL.
* Tools: Fastfetch, zlib, and a fully functional Qt6 environment using our custom QPA plugin.

---

## Getting Started

If you want to poke around or build the system yourself, you'll need an x86-compatible environment (or a virtual machine).

### The Basics
* Architecture: IA-32 / i386 (or x86-64 in 32-bit mode).
* Memory: 200-500 MB RAM is plenty to get the full experience.
* Build Tools: You'll need gcc-multilib (or i686-elf-gcc), nasm (2.15+), make, python3, xorriso, and grub-mkrescue.

### Quick Build
Clone the repo and jump right in:

    # Build the kernel, ports, and generate a bootable ISO
    make all

    # Launch the OS in QEMU
    make run

    # Run with serial output if you're debugging
    make run-debug

You can find the final bootable ISO at build/ohos.iso.

---

## A Bit of History

OHOS traces its roots back to the ElexerKernel project, which we started back in 2013. Over the years, it has evolved from a simple kernel experiment into a complete userspace ecosystem. We published the first public, reproducible repository in 2025, and we've been iterating on it ever since.

---

## A Note of Thanks

This project stands on the shoulders of giants. A huge thank you to the developers behind the projects that make OHOS possible, including:
uACPI, lwIP, libtsm, pixman, Qt6, FFmpeg, TinyGL, mbedTLS, and many others.

Special shout-out to Mark Sordestom for the fantastic artwork, and to everyone in the community who has contributed code, bug reports, and ideas.

---

## License
OHOS is open-source under the BSD 3-Clause License. See the LICENSE file for more details.
