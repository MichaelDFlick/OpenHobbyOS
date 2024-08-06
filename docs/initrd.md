# Initial Ramdisk

The initial ramdisk (initrd) is a compressed filesystem image loaded into memory at boot time by GRUB. It serves as the root filesystem before any disk is mounted.

## Purpose

The initrd provides all the files needed to boot the system and run the initial user-space programs. It contains binaries, libraries, fonts, configuration files, and game data.

## Contents

The initrd is assembled by tools/build_initrd.sh which reads the file list from tools/rootfs_manifest.sh. The contents include:

- System binaries in /bin (hello, uname, sh, login, toolbox, gosh, test_fb)
- Desktop programs (xnx-compositor, installer, milkyway)
- Core utilities (cat, ls, cp, mv, rm, mkdir, chmod, echo, etc.)
- Graphics demos (gtkdemo, gears, doom)
- System info (fastfetch)
- Dynamic linker (/usr/lib/ld-openhobbyos.so.1)
- Qt6 platform plugins (/usr/plugins/platforms/)
- Fonts (/fonts/Monospace.ttf)
- Game data (/doom1.WAD)
- Configuration files (/etc/motd.txt, /etc/goshrc, /etc/os-release)
- Shell colors and fastfetch config

## Build Process

The initrd build process:

1. tools/rootfs_manifest.sh lists all files to include
2. tools/build_initrd.sh copies files from build output and ports sysroot
3. tools/mkramdisk.py packs files into a binary ramdisk format
4. The resulting initrd.bin is placed in the build directory

## Mounting

At boot, GRUB loads initrd.bin as a Multiboot module. The kernel's initrd driver unpacks it and mounts it as the root filesystem. The VFS layer overlays the initrd on top of any mounted disk filesystem.

## Size

The initrd typically contains 62 files and totals around 524KB when packed. With all ports included, it can grow significantly due to Qt6 plugins and shared libraries.
