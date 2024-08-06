# Running OpenHobbyOS

## QEMU

The fastest way to run OpenHobbyOS is with QEMU.

Launch with a graphical display:

```
make run
```

This starts QEMU with a GTK display window at 1280x1024. The OS boots through GRUB, loads the kernel and initrd, and starts the XNX compositor followed by a login prompt.

Run with serial output for debugging:

```
make run-debug
```

This redirects serial output to stdio so you can see kernel messages, page faults, and debug information.

## Disk Image

To test disk encryption and ext2 filesystem support, create a disk image first:

```
make disk
make run-disk
```

The disk creation requires root privileges because it sets up an ext2 filesystem on a raw disk image.

## Boot Sequence

When you run the OS you will see:

1. GRUB loads kernel.bin and initrd.bin via Multiboot v1
2. The kernel initializes GDT, PIC, IDT, PIT, keyboard, mouse, memory, paging, APIC
3. The initrd is mounted as the root filesystem
4. ATA drives are detected and disk encryption is unlocked (if a disk is present)
5. PCI devices are enumerated, networking and USB drivers are loaded
6. SMP is initialized
7. The XNX compositor launches as the first user process
8. A login prompt appears

## Emulator Notes

The default QEMU display uses the GTK backend. The OS boots into a framebuffer console. If GTK4 is available in the initrd, the XNX compositor launches automatically. Otherwise it falls back to GOSH or a basic login prompt.

For best performance with Qt6, configure QEMU with the VMSVGA graphics adapter.
