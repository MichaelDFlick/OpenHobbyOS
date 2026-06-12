PROJECT := OpenHobbyOS
BUILD_DIR := build
ISO_DIR := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.bin
INITRD := $(BUILD_DIR)/initrd.bin
ISO := $(BUILD_DIR)/$(PROJECT).iso
PORTS_DIR := $(BUILD_DIR)/ports
PORTS_SYSROOT := $(PORTS_DIR)/sysroot
NEWLIB_PORT_FILES := $(shell find ports/newlib/openhobbyos -type f | sort)
ZLIB_PC := $(PORTS_SYSROOT)/lib/pkgconfig/zlib.pc
LIBSHA1_PC := $(PORTS_SYSROOT)/lib/pkgconfig/libsha1.pc
LODEPNG_PC := $(PORTS_SYSROOT)/lib/pkgconfig/lodepng.pc
PIXMAN_PC := $(PORTS_SYSROOT)/lib/pkgconfig/pixman-1.pc
GLIB_PC := $(PORTS_SYSROOT)/lib/pkgconfig/glib-2.0.pc
HARFBUZZ_PC := $(PORTS_SYSROOT)/lib/pkgconfig/harfbuzz.pc
EXPAT_PC := $(PORTS_SYSROOT)/lib/pkgconfig/expat.pc
FONTCONFIG_PC := $(PORTS_SYSROOT)/lib/pkgconfig/fontconfig.pc
FRIBIDI_PC := $(PORTS_SYSROOT)/lib/pkgconfig/fribidi.pc
PANGO_PC := $(PORTS_SYSROOT)/lib/pkgconfig/pango.pc
GTK_PC := $(PORTS_SYSROOT)/lib/pkgconfig/gtk4.pc
GDK_PIXBUF_PC := $(PORTS_SYSROOT)/lib/pkgconfig/gdk-pixbuf-2.0.pc
FASTFETCH_BIN := $(PORTS_DIR)/fastfetch/install/usr/bin/fastfetch
XNX_COMPOSITOR := $(PORTS_DIR)/xnx/install/bin/xnx-compositor
INSTALLER_BIN := $(PORTS_SYSROOT)/bin/installer
MILKYWAY_BIN := $(PORTS_SYSROOT)/bin/milkyway

FFMPEG_STAMP := $(PORTS_DIR)/ffmpeg/.built
OHPLAY_BIN := $(PORTS_DIR)/ohplay/install/bin/ohplay
GOSH_BIN := $(PORTS_SYSROOT)/bin/gosh

PORTS_TINYGL_A := $(PORTS_SYSROOT)/lib/libtinygl.a
PORTS_GEARS_BIN := $(PORTS_SYSROOT)/bin/gears
CAIRO_PC := $(PORTS_SYSROOT)/lib/pkgconfig/cairo.pc
FREETYPE_PC := $(PORTS_SYSROOT)/lib/pkgconfig/freetype2.pc

CORUTILS_BIN := $(PORTS_SYSROOT)/bin/cp
QT_STAMP := $(PORTS_DIR)/qt/.built
QTDECL_STAMP := $(PORTS_DIR)/qtdeclarative/.built
QEMU := qemu-system-i386
DISK_IMG := disk.img
QEMU_COMMON_ARGS := -cdrom $(ISO) -no-reboot -no-shutdown
QEMU_DISK_ARGS := -drive file=$(DISK_IMG),format=raw,index=0,media=disk -boot d
QEMU_GUI_ARGS := -display gtk,grab-on-hover=off,show-tabs=off
QEMU_SERIAL_LOG := $(BUILD_DIR)/qemu-serial.log
QEMU_RUN_CD := $(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_GUI_ARGS)
QEMU_RUN_DISK := $(QEMU) $(QEMU_COMMON_ARGS) $(QEMU_DISK_ARGS) $(QEMU_GUI_ARGS)

CC := gcc
LD := ld
NASM := nasm
OBJCOPY := objcopy
PYTHON := python3

CFLAGS := -std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -O2 -Wall -Wextra -mno-sse -mno-sse2 -mfpmath=387 -D__OHOS_KERNEL__ -DMBEDTLS_CONFIG_FILE=\"mbedtls_config_kernel.h\" -Iinclude -Iuser/lib -Iuser/lib/libtsm/src/tsm -Iuser/lib/libtsm/src/shared -Iuser/lib/libtsm/external/wcwidth -Iuser/lib/uACPI/include -Iuser/lib/mbedtls/include -I$(PORTS_SYSROOT)/include
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib
ASFLAGS := -f elf32

USER_CFLAGS := -std=gnu11 -m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fno-builtin -fomit-frame-pointer -O2 -Wall -Wextra -mno-sse -mno-sse2 -mfpmath=387 -Iuser/lib -Iinclude
USER_LDFLAGS := -m elf_i386 -T user.ld -nostdlib

KERNEL_CORE_SOURCES := \
	src/core/kernel.c \
	src/core/crypto.c \
	src/core/console.c \
	src/core/serial.c \
	src/core/string.c \
	src/core/format.c \
	src/core/panic.c \
	src/core/compiler_rt.c \
	src/core/tsm_compat.c \
	src/core/mbedtls_stubs.c

KERNEL_ARCH_SOURCES := \
	src/arch/x86/gdt.c \
	src/arch/x86/idt.c \
	src/arch/x86/memory.c \
	src/arch/x86/paging.c \
	src/arch/x86/cpuid.c \
	src/arch/x86/syscall.c \
	src/arch/x86/smp.c

KERNEL_FS_SOURCES := \
	src/fs/blkdev.c \
	src/fs/enc_blkdev.c \
	src/fs/mbr.c \
	src/fs/ext2.c \
	src/fs/initrd.c \
	src/fs/vfs.c \
	src/fs/elf.c \
	src/fs/socket.c \
	src/fs/pipe.c

KERNEL_NET_SOURCES := \
	src/net/netdev.c

KERNEL_TASK_SOURCES := \
	src/task/task.c \
	src/task/thread.c \
	src/task/swap.c

KERNEL_POWER_SOURCES := \
	src/power/power.c \
	src/power/uacpi_port.c

KERNEL_DRIVERS_SOURCES := \
	src/drivers/input/keyboard.c \
	src/drivers/input/mouse.c \
	src/drivers/interrupt/pic.c \
	src/drivers/interrupt/apic.c \
	src/drivers/timer/pit.c \
	src/drivers/storage/ata.c \
	src/drivers/net/rtl8139.c \
	src/drivers/net/virtio_net.c \
	src/drivers/bus/pci.c \
	src/drivers/usb/usb.c \
	src/drivers/usb/uhci.c \
	src/drivers/usb/usb_hub.c \
	src/drivers/usb/usb_keyboard.c \
	src/drivers/usb/usb_mouse.c \
	src/drivers/usb/usb_storage.c

KERNEL_C_SOURCES := \
	$(KERNEL_CORE_SOURCES) \
	$(KERNEL_ARCH_SOURCES) \
	$(KERNEL_FS_SOURCES) \
	$(KERNEL_NET_SOURCES) \
	$(KERNEL_TASK_SOURCES) \
	$(KERNEL_POWER_SOURCES) \
	$(KERNEL_DRIVERS_SOURCES)

KERNEL_ASM_SOURCES := \
	src/arch/x86/boot.asm \
	src/arch/x86/isr.asm \
	src/arch/x86/task.asm

LIBTSM_C_SOURCES := \
	user/lib/libtsm/src/tsm/tsm-screen.c \
	user/lib/libtsm/src/tsm/tsm-vte.c \
	user/lib/libtsm/src/tsm/tsm-unicode.c \
	user/lib/libtsm/src/tsm/tsm-render.c \
	user/lib/libtsm/src/tsm/tsm-selection.c \
	user/lib/libtsm/src/tsm/tsm-vte-charsets.c \
	user/lib/libtsm/src/shared/shl-htable.c \
	user/lib/libtsm/external/wcwidth/wcwidth.c

UACPI_C_SOURCES := \
	user/lib/uACPI/source/tables.c \
	user/lib/uACPI/source/types.c \
	user/lib/uACPI/source/uacpi.c \
	user/lib/uACPI/source/utilities.c \
	user/lib/uACPI/source/interpreter.c \
	user/lib/uACPI/source/opcodes.c \
	user/lib/uACPI/source/namespace.c \
	user/lib/uACPI/source/stdlib.c \
	user/lib/uACPI/source/shareable.c \
	user/lib/uACPI/source/opregion.c \
	user/lib/uACPI/source/default_handlers.c \
	user/lib/uACPI/source/io.c \
	user/lib/uACPI/source/notify.c \
	user/lib/uACPI/source/sleep.c \
	user/lib/uACPI/source/registers.c \
	user/lib/uACPI/source/resources.c \
	user/lib/uACPI/source/event.c \
	user/lib/uACPI/source/mutex.c \
	user/lib/uACPI/source/osi.c

KERNEL_C_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/src/%.o,$(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS := $(patsubst src/%.asm,$(BUILD_DIR)/src/%.o,$(KERNEL_ASM_SOURCES))
LIBTSM_OBJECTS := \
	$(BUILD_DIR)/libtsm/tsm/tsm-screen.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-vte.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-unicode.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-render.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-selection.o \
	$(BUILD_DIR)/libtsm/tsm/tsm-vte-charsets.o \
	$(BUILD_DIR)/libtsm/shared/shl-htable.o \
	$(BUILD_DIR)/libtsm/external/wcwidth/wcwidth.o
UACPI_OBJECTS := $(patsubst user/lib/uACPI/source/%.c,$(BUILD_DIR)/uacpi/%.o,$(UACPI_C_SOURCES))

MBEDTLS_C_SOURCES := \
	user/lib/mbedtls/library/aes.c \
	user/lib/mbedtls/library/cipher.c \
	user/lib/mbedtls/library/cipher_wrap.c \
	user/lib/mbedtls/library/hash_info.c \
	user/lib/mbedtls/library/hkdf.c \
	user/lib/mbedtls/library/md.c \
	user/lib/mbedtls/library/pkcs5.c \
	user/lib/mbedtls/library/platform.c \
	user/lib/mbedtls/library/platform_util.c \
	user/lib/mbedtls/library/sha256.c

MBEDTLS_OBJECTS := $(patsubst user/lib/mbedtls/library/%.c,$(BUILD_DIR)/mbedtls/%.o,$(MBEDTLS_C_SOURCES))
KERNEL_OBJECTS := $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(LIBTSM_OBJECTS) $(UACPI_OBJECTS) $(MBEDTLS_OBJECTS)

USER_LIB_SOURCES := \
	user/lib/start.c \
	user/lib/syscall.c \
	user/lib/newlib-gloss.c \
	user/lib/runtime.c

USER_PROGRAMS := hello uname toolbox sh test_fb net_test net_info ohloader ohpleasepanic login

define user_program_template
USER_OBJECTS_$(1) := $(patsubst user/%.c,$(BUILD_DIR)/user/%.o,user/$(1).c) $$(patsubst user/lib/%.c,$(BUILD_DIR)/user/lib/%.o,$(USER_LIB_SOURCES))
endef
$(foreach prog,$(USER_PROGRAMS),$(eval $(call user_program_template,$(prog))))
USER_BINS := $(addprefix $(BUILD_DIR)/user/,$(addsuffix .elf,$(USER_PROGRAMS)))

 .PHONY: all clean iso disk disk-img run run-gui run-debug run-with-disk run-disk ports ports-newlib ports-fastfetch ports-zlib ports-libsha1 ports-pixman ports-freetype ports-cairo ports-glib ports-harfbuzz ports-expat ports-fontconfig ports-fribidi ports-gdk-pixbuf ports-pango ports-xnx ports-installer ports-milkyway ports-gosh ports-lodepng ports-lwip ports-doom ports-tinygl ports-gears ports-ffmpeg ports-ohplay ports-qt ports-qtdeclarative

all: $(ISO)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/src:
	mkdir -p $(BUILD_DIR)/src

$(BUILD_DIR)/user:
	mkdir -p $(BUILD_DIR)/user

$(BUILD_DIR)/user/lib:
	mkdir -p $(BUILD_DIR)/user/lib

$(BUILD_DIR)/libtsm/tsm:
	mkdir -p $(BUILD_DIR)/libtsm/tsm

$(BUILD_DIR)/libtsm/shared:
	mkdir -p $(BUILD_DIR)/libtsm/shared

$(BUILD_DIR)/libtsm/external/wcwidth:
	mkdir -p $(BUILD_DIR)/libtsm/external/wcwidth

$(BUILD_DIR)/uacpi:
	mkdir -p $(BUILD_DIR)/uacpi

$(PORTS_DIR):
	mkdir -p $(PORTS_DIR)

$(BUILD_DIR)/src/%.o: src/%.c | $(BUILD_DIR)/src
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/%.o: src/%.asm | $(BUILD_DIR)/src
	mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/libtsm/tsm/%.o: user/lib/libtsm/src/tsm/%.c | $(BUILD_DIR)/libtsm/tsm
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libtsm/shared/%.o: user/lib/libtsm/src/shared/%.c | $(BUILD_DIR)/libtsm/shared
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libtsm/external/wcwidth/%.o: user/lib/libtsm/external/wcwidth/%.c | $(BUILD_DIR)/libtsm/external/wcwidth
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/uacpi/%.o: user/lib/uACPI/source/%.c | $(BUILD_DIR)/uacpi
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mbedtls:
	mkdir -p $(BUILD_DIR)/mbedtls

$(BUILD_DIR)/mbedtls/%.o: user/lib/mbedtls/library/%.c | $(BUILD_DIR)/mbedtls
	$(CC) $(CFLAGS) -c $< -o $@

# SMP trampoline: assemble as flat binary, then wrap in ELF
SMP_TRAMPOLINE_BIN := $(BUILD_DIR)/smp_trampoline.bin
SMP_TRAMPOLINE_OBJ := $(BUILD_DIR)/src/arch/x86/smp_trampoline.o

$(SMP_TRAMPOLINE_BIN): src/arch/x86/smp_trampoline.asm | $(BUILD_DIR)/src
	$(NASM) -f bin -o $@ $<

$(SMP_TRAMPOLINE_OBJ): $(SMP_TRAMPOLINE_BIN)
	$(OBJCOPY) -B i386 -I binary -O elf32-i386 $< $@

KERNEL_OBJECTS += $(SMP_TRAMPOLINE_OBJ)

$(BUILD_DIR)/user/%.o: user/%.c | $(BUILD_DIR)/user
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BUILD_DIR)/user/lib/%.o: user/lib/%.c | $(BUILD_DIR)/user/lib
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(FREETYPE_PC): ports/freetype/build-freetype.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/freetype/build-freetype.sh $(PORTS_DIR)/freetype $(PORTS_SYSROOT)

$(CAIRO_PC): ports/cairo/build-cairo.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PIXMAN_PC) $(ZLIB_PC) $(FREETYPE_PC)
	ports/cairo/build-cairo.sh $(PORTS_DIR)/cairo $(PORTS_SYSROOT)

$(KERNEL): $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS) -L$(PORTS_SYSROOT)/lib

$(BUILD_DIR)/user/hello.elf: $(USER_OBJECTS_hello) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_hello)

$(BUILD_DIR)/user/uname.elf: $(USER_OBJECTS_uname) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_uname)

$(BUILD_DIR)/user/toolbox.elf: $(USER_OBJECTS_toolbox) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_toolbox)

$(BUILD_DIR)/user/sh.elf: $(USER_OBJECTS_sh) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_sh)

$(BUILD_DIR)/user/test_fb.elf: $(USER_OBJECTS_test_fb) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_test_fb)

$(BUILD_DIR)/user/net_test.elf: $(USER_OBJECTS_net_test) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_net_test)

$(BUILD_DIR)/user/net_info.elf: $(USER_OBJECTS_net_info) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_net_info)

$(BUILD_DIR)/user/ohloader.elf: $(USER_OBJECTS_ohloader) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_ohloader)

$(BUILD_DIR)/user/ohpleasepanic.elf: $(USER_OBJECTS_ohpleasepanic) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_ohpleasepanic)

$(BUILD_DIR)/user/login.elf: $(USER_OBJECTS_login) user.ld
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_OBJECTS_login)

$(PORTS_SYSROOT)/.newlib.stamp: ports/newlib/build-newlib.sh $(NEWLIB_PORT_FILES) | $(PORTS_DIR)
	ports/newlib/build-newlib.sh $(PORTS_DIR)/newlib $(PORTS_SYSROOT)
	touch $@

$(ZLIB_PC): ports/zlib/build-zlib.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/zlib/build-zlib.sh $(PORTS_DIR)/zlib $(PORTS_SYSROOT)

$(LIBSHA1_PC): ports/libsha1/build-libsha1.sh ports/libsha1/libsha1.c ports/libsha1/libsha1.h $(PORTS_SYSROOT)/.newlib.stamp
	ports/libsha1/build-libsha1.sh $(PORTS_DIR)/libsha1 $(PORTS_SYSROOT)

$(PIXMAN_PC): ports/pixman/build-pixman.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/pixman/build-pixman.sh $(PORTS_DIR)/pixman $(PORTS_SYSROOT)

$(GLIB_PC): ports/glib/build-glib.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/glib/build-glib.sh $(PORTS_DIR)/glib $(PORTS_SYSROOT)

$(HARFBUZZ_PC): ports/harfbuzz/build-harfbuzz.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(FREETYPE_PC) $(GLIB_PC)
	ports/harfbuzz/build-harfbuzz.sh $(PORTS_DIR)/harfbuzz $(PORTS_SYSROOT)

$(EXPAT_PC): ports/expat/build-expat.sh ports/qt/openhobbyos-toolchain.cmake $(PORTS_SYSROOT)/.newlib.stamp
	ports/expat/build-expat.sh $(PORTS_DIR)/expat $(PORTS_SYSROOT)

$(FONTCONFIG_PC): ports/fontconfig/build-fontconfig.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(EXPAT_PC) $(FREETYPE_PC)
	ports/fontconfig/build-fontconfig.sh $(PORTS_DIR)/fontconfig $(PORTS_SYSROOT)

$(FRIBIDI_PC): ports/fribidi/build-fribidi.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(PORTS_SYSROOT)/.newlib.stamp
	ports/fribidi/build-fribidi.sh $(PORTS_DIR)/fribidi $(PORTS_SYSROOT)

$(PANGO_PC): ports/pango/build-pango.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(GLIB_PC) $(HARFBUZZ_PC) $(FONTCONFIG_PC) $(FRIBIDI_PC) $(FREETYPE_PC)
	ports/pango/build-pango.sh $(PORTS_DIR)/pango $(PORTS_SYSROOT)

$(GDK_PIXBUF_PC): ports/gdk-pixbuf/build-gdk-pixbuf.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(GLIB_PC)
	ports/gdk-pixbuf/build-gdk-pixbuf.sh $(PORTS_DIR)/gdk-pixbuf $(PORTS_SYSROOT)

$(GTK_PC): ports/gtk/build-gtk.sh ports/meson/openhobbyos.cross.in tools/ensure_meson.sh tools/meson-requirements.txt $(GDK_PIXBUF_PC) $(PANGO_PC) $(GLIB_PC) $(HARFBUZZ_PC) $(FONTCONFIG_PC) $(FRIBIDI_PC) $(FREETYPE_PC) $(CAIRO_PC) $(PIXMAN_PC) $(EXPAT_PC) $(XNX_COMPOSITOR)
	ports/gtk/build-gtk.sh $(PORTS_DIR)/gtk $(PORTS_SYSROOT)

$(FASTFETCH_BIN): ports/fastfetch/build-fastfetch.sh ports/fastfetch/openhobbyos-toolchain.cmake $(PORTS_SYSROOT)/.newlib.stamp
	ports/fastfetch/build-fastfetch.sh $(PORTS_DIR)/fastfetch $(PORTS_SYSROOT)

$(LODEPNG_PC): ports/lodepng/build-lodepng.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/lodepng/build-lodepng.sh $(PORTS_DIR)/lodepng $(PORTS_SYSROOT)

XNX_COMPOSITOR_SOURCES := $(wildcard user/xnx/*.c) $(wildcard user/lib/xnx/*.c)

$(XNX_COMPOSITOR): $(XNX_COMPOSITOR_SOURCES) ports/xnx/build-xnx.sh $(PIXMAN_PC) $(LODEPNG_PC) $(PORTS_SYSROOT)/.newlib.stamp | $(PORTS_DIR)
	ports/xnx/build-xnx.sh $(PORTS_DIR)/xnx $(PORTS_SYSROOT) || true

INSTALLER_SOURCES := $(wildcard user/installer/*.cpp)
$(INSTALLER_BIN): $(INSTALLER_SOURCES) $(PORTS_SYSROOT)/.newlib.stamp ports/installer/build-installer.sh | $(PORTS_DIR)
	mkdir -p $(PORTS_DIR)/installer
	ports/installer/build-installer.sh $(PORTS_DIR)/installer $(PORTS_SYSROOT)

MILKYWAY_SOURCES := $(wildcard user/milkyway/*.cpp)
$(MILKYWAY_BIN): $(MILKYWAY_SOURCES) $(PORTS_SYSROOT)/.newlib.stamp ports/milkyway/build-milkyway.sh | $(PORTS_DIR)
	mkdir -p $(PORTS_DIR)/milkyway
	ports/milkyway/build-milkyway.sh $(PORTS_DIR)/milkyway $(PORTS_SYSROOT)

$(GOSH_BIN): user/gosh.c user/lib/syscall.c user/lib/runtime.c $(CAIRO_PC) $(FREETYPE_PC) $(XNX_COMPOSITOR) ports/gosh/build-gosh.sh | $(PORTS_DIR)
	mkdir -p $(PORTS_DIR)/gosh
	ports/gosh/build-gosh.sh $(PORTS_DIR)/gosh $(PORTS_SYSROOT)

$(OHPLAY_BIN): ports/ohplay/build-ohplay.sh $(XNX_COMPOSITOR) $(FFMPEG_STAMP) $(ZLIB_PC) $(PORTS_SYSROOT)/.newlib.stamp | $(PORTS_DIR)
	ports/ohplay/build-ohplay.sh $(PORTS_DIR)/ohplay $(PORTS_SYSROOT)

$(PORTS_TINYGL_A): ports/tinygl/build-tinygl.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/tinygl/build-tinygl.sh $(PORTS_DIR)/tinygl $(PORTS_SYSROOT)

$(PORTS_GEARS_BIN): ports/gears/build-gears.sh ports/gears/gears.c $(PORTS_TINYGL_A)
	ports/gears/build-gears.sh $(PORTS_DIR)/gears $(PORTS_SYSROOT)

$(QT_STAMP): ports/qt/build-qt.sh ports/qt/openhobbyos-toolchain.cmake $(PIXMAN_PC) $(XNX_COMPOSITOR) $(PORTS_SYSROOT)/.newlib.stamp
	ports/qt/build-qt.sh $(PORTS_DIR)/qt $(PORTS_SYSROOT) || true

$(QTDECL_STAMP): ports/qtdeclarative/build-qtdeclarative.sh $(QT_STAMP)
	ports/qtdeclarative/build-qtdeclarative.sh $(PORTS_DIR)/qtdeclarative $(PORTS_SYSROOT) || true
	touch $@

$(INITRD): tools/build_initrd.sh tools/mkramdisk.py tools/rootfs_manifest.sh $(USER_BINS) $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(INSTALLER_BIN) $(MILKYWAY_BIN) $(GOSH_BIN) $(PORTS_GEARS_BIN) $(PORTS_SYSROOT)/bin/doom $(CORUTILS_BIN) assets/Doom1.WAD | $(BUILD_DIR)
	tools/build_initrd.sh $@

$(DISK_IMG): $(USER_BINS) $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(INSTALLER_BIN) $(MILKYWAY_BIN) $(OHPLAY_BIN) $(PORTS_GEARS_BIN) $(PORTS_SYSROOT)/bin/doom $(CORUTILS_BIN) assets/Doom1.WAD tools/populate_disk.sh tools/rootfs_manifest.sh
	sudo env OPENHOBBYOS_ROOT="$(CURDIR)" "$(CURDIR)/tools/populate_disk.sh" "$(CURDIR)/$(DISK_IMG)"

$(ISO): $(KERNEL) $(INITRD) grub/grub.cfg | $(BUILD_DIR)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.bin
	cp $(INITRD) $(ISO_DIR)/boot/initrd.bin
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR)

iso: $(ISO)

ports-newlib: $(PORTS_SYSROOT)/.newlib.stamp

ports-fastfetch: $(FASTFETCH_BIN)

ports-zlib: $(ZLIB_PC)

ports-libsha1: $(LIBSHA1_PC)

ports-pixman: $(PIXMAN_PC)

ports-glib: $(GLIB_PC)

ports-harfbuzz: $(HARFBUZZ_PC)

ports-expat: $(EXPAT_PC)

ports-fontconfig: $(FONTCONFIG_PC)

ports-fribidi: $(FRIBIDI_PC)

ports-gdk-pixbuf: $(GDK_PIXBUF_PC)
ports-pango: $(PANGO_PC)

ports-gtk: $(GTK_PC)

ports-freetype: $(FREETYPE_PC)

ports-cairo: $(CAIRO_PC)

ports-xnx: $(XNX_COMPOSITOR)

ports-installer: $(INSTALLER_BIN)

ports-milkyway: $(MILKYWAY_BIN)



ports-gosh: $(GOSH_BIN)

ports-ohplay: $(OHPLAY_BIN)

ports-lwip: ports/lwip/build-lwip.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/lwip/build-lwip.sh $(PORTS_DIR)/lwip $(PORTS_SYSROOT)

ports-doom: $(PORTS_SYSROOT)/bin/doom

$(PORTS_SYSROOT)/bin/doom: ports/doom/build-doom.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/doom/build-doom.sh $(PORTS_DIR)/doom $(PORTS_SYSROOT)

$(CORUTILS_BIN): ports/coreutils/build-coreutils.sh $(PORTS_SYSROOT)/.newlib.stamp user/coreutils/configure
	ports/coreutils/build-coreutils.sh $(PORTS_DIR)/coreutils $(PORTS_SYSROOT)

$(FFMPEG_STAMP): ports/ffmpeg/build-ffmpeg.sh $(PORTS_SYSROOT)/.newlib.stamp
	ports/ffmpeg/build-ffmpeg.sh $(PORTS_DIR)/ffmpeg $(PORTS_SYSROOT)
	touch $@

ports-ffmpeg: $(FFMPEG_STAMP)

ports-coreutils: $(CORUTILS_BIN)

ports-tinygl: $(PORTS_TINYGL_A)

ports-gears: $(PORTS_GEARS_BIN)

ports-qt: $(QT_STAMP)

ports-qtdeclarative: $(QTDECL_STAMP)

ports: $(FASTFETCH_BIN) $(XNX_COMPOSITOR) $(INSTALLER_BIN) $(MILKYWAY_BIN) $(GOSH_BIN) $(OHPLAY_BIN) $(PORTS_GEARS_BIN) $(QT_STAMP) $(PORTS_SYSROOT)/bin/doom $(LODEPNG_PC) $(CORUTILS_BIN) $(EXPAT_PC) $(FONTCONFIG_PC) $(FRIBIDI_PC) $(PANGO_PC) $(GTK_PC)

run: run-gui

run-gui: $(ISO)
	mkdir -p $(BUILD_DIR)
	$(QEMU_RUN_CD) -serial file:$(QEMU_SERIAL_LOG)

run-debug: $(ISO)
	$(QEMU_RUN_CD) -serial stdio

run-with-disk: $(ISO) $(DISK_IMG)
	mkdir -p $(BUILD_DIR)
	$(QEMU_RUN_DISK) -serial file:$(QEMU_SERIAL_LOG)

run-disk: run-with-disk

disk disk-img: $(DISK_IMG)

clean:
	rm -rf $(BUILD_DIR)
