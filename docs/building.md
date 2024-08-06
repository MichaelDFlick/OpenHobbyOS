# Building OpenHobbyOS

## Requirements

- IA-32 / i386 architecture or x86-64 in 32-bit mode
- 200 MB RAM minimum, 512 MB recommended for the full port stack
- Build tools: gcc-multilib (or i686-elf-gcc), nasm 2.15+, make, python3, xorriso, grub-mkrescue

The cross-compiler target is i686-openhobbyos-elf. If you do not have a cross-compiler, the Makefile will fall back to the system gcc with -m32 flags.

## Build Commands

Build everything with a single command:

```
make all
```

This compiles the kernel, user-space programs, all third-party ports, and produces a bootable ISO image.

Build individual components separately:

```
make kernel
make user
make ports
```

Generate the bootable ISO without rebuilding everything:

```
make iso
```

Clean all build artifacts:

```
make clean
```

## What Gets Built

The build process produces these artifacts in the build/ directory:

- kernel.bin -- The kernel binary
- initrd.bin -- Initial ramdisk containing user programs, fonts, configs
- OpenHobbyOS.iso -- Bootable ISO image with GRUB

## Build Process

The kernel is compiled from C and NASM assembly sources using gcc with -m32 and -ffreestanding flags. Assembly files include boot.asm (multiboot entry), isr.asm (interrupt stubs), task.asm (context switch), and smp_trampoline.asm (AP bootstrap).

User programs are compiled with -m32 and linked against the user-space linker script (user.ld) which places code at 0x03000000.

Each port has its own build script in the ports/ directory. Ports are built into build/ports/sysroot/ and the necessary files are copied into the initrd.

The initrd is assembled by tools/build_initrd.sh using tools/mkramdisk.py which packs files listed in tools/rootfs_manifest.sh.

Finally, grub-mkrescue packages the kernel, initrd, and GRUB configuration into a bootable ISO.
