================================================================================
 How to Build and Run OpenHobbyOS (or: How I Learned to Stop Worrying and Love
 the Segfault)
================================================================================

This document explains the entire build pipeline, the Qt6 desktop stack, the
mouse driver I wrote at 2 AM, and how to actually get this thing running so you
can watch a compositor composite your existence into a 32-bit framebuffer.

If you just want to make an ISO and boot it, skip to `Quick start`_ below.

.. contents::

What is all this
================

OpenHobbyOS is a 32-bit x86 hobby OS with a monolithic kernel, a userspace
stack built on newlib, and a graphical compositor called XNX. It also now has
Qt6, a nuklear-based TUI login manager called GDM, an audio player (ohplay),
and a snapshot boot feature that resumes from disk. Because apparently that's
something you can do when you don't have a life.

The full stack looks like this::

    Qt6 Qml/Quick apps  (qt_demo test)
           |
    Qt6 QPA plugin (openhobbyos platform integration)
           |
    XNX compositor  (Unix socket + pixman compositing)
           |
    /dev/fb0        (kernel framebuffer, 1024x768, 32 bpp)
           |
    Kernel          (i386, preemptive multitasking, VFS, ext2)
           |
    PS/2 mouse      (IRQ 12, /dev/mouse, 3-byte packets)
           |
    PS/2 keyboard   (IRQ 1, scancodes → stdin)
           |
    QEMU / real hardware


Prerequisites
=============

You will need:

- A sane Linux distribution (Debian/Ubuntu tested, but whatever)
- ``gcc-multilib`` (or a cross-compiler if you're fancy)
- ``nasm >= 2.15``
- ``make``, ``python3 >= 3.8``
- ``xorriso``, ``grub-mkrescue``, ``mtools``
- ``meson``, ``ninja``, ``pkg-config``, ``autoconf``, ``automake``, ``cmake``
- ``git`` (for submodules — there are many. I'm sorry.)

Install these on Debian/Ubuntu::

    sudo apt install build-essential nasm make python3 xorriso grub-pc-bin     \\
                     mtools meson ninja-build pkg-config autoconf automake      \\
                     cmake git

For the Qt6 desktop stack you also need the OS-specific Qt packages. The build
script expects Qt6 at ``user/lib/graphics32/qtbase`` and
``user/lib/graphics32/qtdeclarative``. These are git submodules, so clone them
explicitly::

    git submodule update --init user/lib/graphics32/qtbase
    git submodule update --init user/lib/graphics32/qtdeclarative
    git submodule update --init user/lib/graphics32/qtshadertools


What Everything Does
====================

The Monolithic Kernel That Thinks It's A Microkernel
-----------------------------------------------------

``src/`` contains the kernel. It used to be one giant file. Then I had a
religious experience and split it into:

- ``src/core/`` — scheduling, interrupts, syscall dispatch, panic (a lot of
  panicking)
- ``src/arch/x86/`` — GDT, IDT, paging, and now: the PS/2 mouse driver.
  Yes, there's a file called ``mouse.c``. It reads 3-byte packets from IRQ 12
  and stuffs them into a ring buffer. I felt like a real kernel developer.
- ``src/fs/`` — VFS, ext2 (I wrote this and then I cried), devfs, initrd
- ``src/net/`` — lwIP glue, RTL8139 driver, socket layer
- ``src/power/`` — ACPI via uACPI (I can shut down the computer like a grown-up)
- ``src/task/`` — process management, fork, exec, the usual Unix horror show

Some highlights::

    /dev/fb0       — framebuffer (1024x768, 32 bpp, memory-mapped)
    /dev/mouse     — PS/2 mouse (read 3-byte packets: buttons, dx, dy)
    /dev/null      — it exists. it nulls. it devs.
    /dev/tty       — a console device that lets you type things at the kernel
                     like you're in a hacker movie

XNX Compositor: The World's Worst Wayland
------------------------------------------

``user/xnx/compositor.c`` is the compositor. It's about 858 lines of pixman
pixel-pushing. It:

- Opens ``/dev/fb0`` and memory-maps the hardware framebuffer
- Creates a Unix domain socket at ``/tmp/xnx.sock``
- Accepts client connections (Qt apps, demos, etc.)
- Manages surfaces (create, destroy, set geometry, write buffers, commit)
- Composites everything with pixman in z-order every 33ms
- Reads from ``/dev/mouse`` in its poll loop and sends pointer events to the
  focused surface
- Draws a crosshair cursor on the framebuffer (real art, I know)

The protocol is defined in ``user/xnx/protocol.h``. Messages are
``{uint32_t type, uint32_t payload_size, uint8_t payload[]}`` over stream
sockets. The types are::

    0x0001 XNX_CREATE_SURFACE
    0x0002 XNX_DESTROY_SURFACE
    0x0003 XNX_SET_TITLE
    0x0004 XNX_SET_GEOMETRY
    0x0005 XNX_WRITE_BUFFER
    0x0006 XNX_COMMIT
    0x0007 XNX_GET_DISPLAY_INFO
    0x8001 XNX_SURFACE_CREATED         (server → client)
    0x8002 XNX_KEY_EVENT               (server → client)
    0x8003 XNX_POINTER_EVENT           (server → client)  ← mouse!
    0x8005 XNX_FRAME_DONE              (server → client)
    0x8006 XNX_DISPLAY_INFO            (server → client)

The client library is at ``user/lib/xnx/xnx.h`` and ``xnx.c``. It handles
connecting, sending messages, receiving events, and dispatching pointer and
keyboard events to the application.

Qt6 QPA Plugin: OpenHobbyOS Pretends To Be A Platform
------------------------------------------------------

``ports/qt/openhobbyos/`` contains a Qt6 Platform Abstraction (QPA) plugin.
This is what makes Qt apps render on OHOS. It includes:

- ``qopenhobbyosintegration.cpp`` — the main integration, connects to XNX,
  creates the screen
- ``qopenhobbyosscreen.cpp`` — a 1024x768 screen with 32-bit ARGB format
- ``qopenhobbyoswindow.cpp`` — wraps Qt windows as XNX surfaces
- ``qopenhobbyosbackingstore.cpp`` — flushes raster content to XNX via pixman
- ``qopenhobbyoseventdispatcher.cpp`` — polls XNX for key and pointer events,
  feeds them to ``QWindowSystemInterface``
- ``qopenhobbyoscursor.cpp`` — reports mouse position, handles cursor shape
  changes (currently a no-op; the compositor draws the crosshair)
- ``qopenhobbyosfontdatabase.cpp`` — font database (tells Qt about available
  fonts)
- ``qopenhobbyosclipboard.cpp`` — clipboard (stub)
- ``qopenhobbyosinputcontext.cpp`` — input method context (stub)
- ``qopenhobbyosnativeinterface.cpp`` — native interface
- ``qopenhobbyosservices.cpp`` — services (stub)

The plugin is built as part of the Qt6 cross-compilation.

Quick start
===========

One-command build (everything)::

    make all

This will:

1. Build the toolchain (GCC cross-compiler, newlib C library)
2. Build the kernel (``build/kernel.bin``)
3. Build all ports (pixman, xnx, lwip, doom, ffmpeg, tinygl, gears...)
4. Build Qt6 (``make ports-qt``) — this takes 20-30 minutes. Go make coffee.
5. Build Qt6 Declarative (``make ports-qtdeclarative``) — Qml + Quick
6. Build the initrd and bootable ISO

Then run::

    make run

Or with serial debug output::

    make run-debug


Step-by-step build
==================

If the one-command build fails (and it probably will because this is a hobby OS
and I am not a real engineer), do it step by step so you can see where it
breaks:

1. Toolchain + C library::

    make ports-newlib

2. Basic ports (pixman → xnx → everything else)::

    make ports-xnx
    make ports-fastfetch
    make ports-ffmpeg
    make ports-tinygl
    make ports-gears

3. Qt6 (core + GUI)::

    make ports-qt

   This builds the host Qt tools natively, then cross-compiles the QPA plugin
   and Qt6 Core + Gui as shared libraries. The QPA plugin is what lets Qt apps
   render via XNX.

4. Qt6 Qml + Quick::

    make ports-qtdeclarative

   This cross-compiles Qt6Qml, Qt6QmlModels, and Qt6Quick with all their
   dependencies. The build uses a stub tool (``qml_stub_tool.sh``) to generate
   the type registration files that ``qmltyperegistrar`` normally produces.
   This works around the lack of a cross-compiled QML toolchain.

5. Build the kernel + initrd + ISO::

    make iso

Alternatively::

    make all

   But you probably already tried that and it exploded, didn't it?


Qt6 Build Details (The Tricky Part)
====================================

Qt6 is the most complicated port because it's a massive C++ framework and I'm
building it with a custom cross-toolchain that barely works. Here's what's
going on:

**The toolchain file** is at ``ports/qt/openhobbyos-toolchain.cmake``. It tells
CMake to use ``i686-openhobbyos-gcc`` / ``g++`` as the compiler, sets the
sysroot to ``build/ports/sysroot``, and provides the linker flags. The key
linker flag is::

    -Wl,--start-group -lopenhobbyosgloss -lstdc++ -Wl,--end-group

This allows circular symbol resolution between our compatibility library
(``libopenhobbyosgloss.a``, which provides ``fseeko64``, ``stdout``, ``stdin``,
etc.) and the host-built ``libstdc++.a``. Without the group, the linker
processes ``-lopenhobbyosgloss`` before it sees libstdc++'s references, and
you get undefined symbol errors. The group fixes that.

**The QPA plugin** is at ``ports/qt/openhobbyos/``. It's a Qt platform plugin
that translates Qt's platform abstraction calls into XNX protocol messages.
It's built as part of the qtbase cross-compilation (Stage 2 in
``ports/qt/build-qt.sh``).

**QtDeclarative** (Qml + Quick) is built separately because it depends on
qtbase being installed first. The build script
(``ports/qtdeclarative/build-qtdeclarative.sh``) does some creative things:

- Uses ``QmlToolStubs.cmake`` to create stub IMPORTED targets for host tools
  like ``qmltyperegistrar``
- The stub tool (``qml_stub_tool.sh``) generates empty but valid output files
  so the build doesn't fail when the real tools aren't available
- After build, all cmake target files get their absolute paths patched to use
  ``${_IMPORT_PREFIX}`` for relocation

**If the Qt6 build fails**, common causes:

- Libstdc++ linker errors: the ``--start-group`` fix may not be picked up if
  the cmake cache is stale. Run ``rm -rf build/ports/qtdeclarative/CMakeCache.txt``
  and rebuild.
- QML type registration errors: the stub file didn't get generated. Re-run the
  build script — ``qml_stub_tool.sh`` handles this automatically now.
- Missing ``libstdc++.a``: make sure you have a cross-compiler that provides it.
  The toolchain at ``toolchain/`` should have it.


Mouse: The Driver I Wrote Instead of Sleeping
=============================================

The PS/2 mouse driver lives in ``src/arch/x86/mouse.c``. It:

1. Enables the PS/2 auxiliary channel (port 0x64 command 0xA8)
2. Sets up the IRQ 12 handler
3. Reads 3-byte packets from port 0x60 on each interrupt
4. Parses them into ``mouse_event_t { buttons, dx, dy }``
5. Stores them in a ring buffer

The kernel exports these events via ``/dev/mouse``. Reading from ``/dev/mouse``
gives you a ``mouse_event_t`` struct (3 bytes: buttons + dx + dy). The XNX
compositor reads from this device in its poll loop and forwards events to
the focused surface.

The compositor also draws a crosshair cursor directly on the framebuffer. This
happens AFTER compositing, so the cursor sits on top of everything. It's a
crude 9-pixel cross, but it works. Future versions will let Qt apps draw their
own cursor.


Build output structure
======================

After a successful build, everything ends up in ``build/``::

    build/
    ├── kernel.bin                  # The kernel itself (~700KB)
    ├── ohos.iso                    # Bootable ISO (boot with QEMU or burn it)
    ├── disk.img                    # IDE disk image with ext2
    ├── initrd.bin                  # initrd (embedded in ISO)
    ├── ports/
    │   ├── sysroot/                # Cross-compilation sysroot
    │   │   ├── bin/                # User binaries (gosh, ohplay, etc.)
    │   │   ├── lib/                # Shared libraries (Qt6, pixman, etc.)
    │   │   ├── plugins/platforms/  # QPA plugin (libqopenhobbyos.so)
    │   │   ├── include/            # Headers
    │   ├── qt/                     # Qt6 build artifacts
    │   ├── qtdeclarative/          # QtDeclarative build artifacts
    │   └── ...
    └── ...                         # Other build artifacts


Running in QEMU
===============

Quickest way::

    make run

This builds the ISO and fires up QEMU with 512MB RAM, VGA, PS/2 mouse and
keyboard, and a disk image. The serial output goes to ``build/serial.log``.

If you want to see boot messages on the serial console::

    make run-debug

This runs QEMU with ``-serial stdio`` so you can watch the kernel panic in
real time.

For the full disk experience::

    make run-with-disk

This adds an IDE disk image with ext2. Useful for testing the disk formatting
and installer.

QEMU flags used::

    -enable-kvm          # Faster (if your CPU supports it)
    -m 512M              # 512 MB RAM
    -vga std             # Standard VGA (framebuffer)
    -usb -device usb-tablet  # USB tablet for smooth mouse
    -audiodev pa,id=pa0  # PulseAudio sound
    -machine pc,accel=kvm:tcg  # KVM or fallback to TCG


Installing to real hardware
===========================

Burn ``build/ohos.iso`` to a CD::

    sudo dd if=build/ohos.iso of=/dev/sr0 bs=4M status=progress

Or write the disk image to a USB drive::

    sudo dd if=build/disk.img of=/dev/sdX bs=4M status=progress

Replace ``/dev/sdX`` with your USB device. **Do not get this wrong.** I am not
responsible for your data.

Requirements on real hardware:

- i386 or later CPU (Pentium or better. Your ThinkPad from 2005 will work.)
- 200-500 MB RAM
- VGA-compatible framebuffer (most QEMU-compatible hardware works)
- PS/2 mouse port (or USB in legacy mode — the driver is PS/2 only)
- IDE or SATA in IDE mode (for disk image)
- CD-ROM or USB boot support


The boot flow (what happens when you press "run")
==================================================

1. **BIOS/UEFI** loads GRUB from the ISO
2. **GRUB** loads ``kernel.bin`` and ``initrd.bin``
3. **Kernel** starts (``_start`` → ``kmain``):
   a. Sets up GDT, IDT, interrupts
   b. Initializes the console (VGA text mode → framebuffer)
   c. Initializes the PS/2 keyboard and mouse
   d. Mounts initrd, creates devfs
   e. Starts the scheduler
   f. Launches ``start_xnx_compositor()`` — picks the first available display
      server: **GDM** (owns fb directly via nuklear) → **gosh** (text UI) →
      **XNX compositor** (pixman compositing for Qt6 apps)
4. **init** runs the first available user session:
   a. **GDM** — TUI login screen with nuklear. Asks for credentials, doesn't
      validate them. If ``/bin/gdm`` exists, this is what you see first.
   b. **gosh** — fallback shell if GDM isn't built.
   c. **sh** / **terminal** — last-resort shells.
5. **GDM** (the graphical path): draws a nuklear login UI on ``/dev/fb0``
   directly, reads keyboard input, and presents a retro login prompt. When GDM
   exits (it crashes sometimes — "it's a feature"), the kernel falls through to
   gosh so you still get a shell.
6. **XNX compositor**: starts only if neither GDM
   nor gosh is present:
   a. Opens ``/dev/fb0``, maps the framebuffer
   b. Opens ``/dev/mouse`` for pointer events
   c. Listens on ``/tmp/xnx.sock`` for client connections
   d. Enters the event loop (poll + composite every 33ms)

If anything fails, you'll see error messages on the framebuffer console.
Probably something about a segfault. That's normal. It's a feature.


Troubleshooting
===============

``make: *** [build/ports/qtdeclarative/.built] Error 1``
    Qt6 Qml/Quick build failed. Check ``build/ports/qtdeclarative/`` for error
    logs. Common causes: stale cmake cache (delete
    ``build/ports/qtdeclarative/CMakeCache.txt``), missing libstdc++ symbols
    (rebuild newlib with ``make ports-newlib``), or the toolchain file not
    having the ``--start-group`` fix.

``cc1plus: fatal error: qml_qmltyperegistrations.cpp: No such file or directory``
    The qmltyperegistrar stub didn't generate the file. Run the build again —
    the ``qml_stub_tool.sh`` should handle it on reconfigure.

``/usr/bin/ld: cannot find entry symbol _start``
    The linker can't find the entry point. This usually means ``crt0.o`` isn't
    being linked. Make sure you're using ``-nostartfiles`` and the linker can
    find ``crt0.o`` in your sysroot's lib directory.

``pixman_image_create_bits failed``
    Something is wrong with the framebuffer initialization. Make sure the
    kernel set up ``/dev/fb0`` correctly. The compositor expects 1024x768,
    32 bpp.

Mouse doesn't work
    The PS/2 mouse driver needs a PS/2 port. QEMU provides this by default.
    On real hardware, make sure the mouse is in a PS/2 port or the BIOS has
    USB legacy emulation enabled. Check ``/dev/mouse`` exists in the kernel
    boot log.

Qt app shows nothing
    The QPA plugin might not have connected to XNX. Check if
    ``/tmp/xnx.sock`` exists and the compositor is running. If the compositor
    isn't running, Qt apps will log a warning and try to proceed, but won't
    render anything.


Contributing
============

Open a PR. Or don't. This is a hobby OS. I don't have a code of conduct. I
don't have a contributing guide. I don't have a CI pipeline. I have a
``Makefile`` and a dream.

If you find a bug, you can keep it. Or fix it and send a patch. Either way,
I'll probably merge it after staring at it for three weeks and muttering "huh,
that's clever."


License
=======

BSD 3-Clause. Go wild. See ``LICENSE``. Don't sue me if it fucks up your PC.
It probably wonn't.
