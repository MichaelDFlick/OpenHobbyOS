#include "ata.h"
#include "blkdev.h"
#include "console.h"
#include "cpuid.h"
#include "crypto.h"
#include "enc_blkdev.h"
#include "gdt.h"
#include "idt.h"
#include "initrd.h"
#include "io.h"
#include "keyboard.h"
#include "mouse.h"
#include "memory.h"
#include "multiboot.h"
#include "netdev.h"
#include "paging.h"
#include "panic.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"
#include "power.h"
#include "rtl8139.h"
#include "virtio_net.h"
#include "task.h"
#include "thread.h"
#include "vfs.h"

static bool kernel_path_is_executable(const char *path) {
    vfs_stat_t st;
    const vfs_node_t *node = vfs_resolve(vfs_root(), path);

    if (!node || !vfs_stat_node(node, &st)) {
        return false;
    }
    return !st.is_dir && ((st.mode & 0111u) != 0);
}

static void start_xnx_compositor(void) {
    memory_stats_t stats_after;

    memory_defragment();
    stats_after = memory_stats();
    console_printf("[init] heap: %u KiB used, %u KiB free, largest=%u KiB\n",
                   stats_after.heap_used / 1024, stats_after.heap_free / 1024,
                   memory_largest_free_block() / 1024);

    if (kernel_path_is_executable("/bin/gosh")) {
        console_printf("[init] gosh owns framebuffer, skipping compositor\n");
        return;
    }

    if (kernel_path_is_executable("/bin/xnx-compositor")) {
        int pid = task_spawn_background("/bin/xnx-compositor");
        console_printf("[init] XNX compositor spawn: pid=%d\n", pid);
    }

}

extern u8 stack_top;
extern u8 __kernel_end;

static void boot_banner(void) {
    console_write("OpenHobbyOS\n");
    console_write("the small workspace\n\n");
}

void kernel_main(u32 magic, u32 mbi_addr) {
    const multiboot_info_t *mbi = (const multiboot_info_t *)(uintptr_t)mbi_addr;

    console_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("Bootloader did not hand us a multiboot environment");
    }

    console_configure(mbi);

    {
        u32 cr0_val, cr4_val;
        __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0_val));
        __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4_val));
        console_printf("[boot] CR0=%x CR4=%x (PAE=%s)\n",
                       cr0_val, cr4_val,
                       (cr4_val & 0x20) ? "ON" : "OFF");
    }

    gdt_init((uintptr_t)&stack_top);
    pic_remap();
    idt_init();
    pit_init(100);
    keyboard_init();
    mouse_init();
    memory_init(mbi, (uintptr_t)&__kernel_end);
    paging_init(mbi, memory_total_bytes(), (uintptr_t)&__kernel_end);

    cpu_detect();
    console_activate();
    boot_banner();

    memory_defragment();
    power_init();

    initrd_init(mbi);

    blkdev_init();
    ata_init();
    crypto_init();

    if (blkdev_present(0)) {
        console_write("[init] Enter disk passphrase: ");
        char passphrase[128];
        size_t pp_idx = 0;
        for (;;) {
            char ch = keyboard_getchar();
            if (ch == '\n') {
                console_putc('\n');
                break;
            }
            if (ch == '\b') {
                if (pp_idx > 0) pp_idx--;
                continue;
            }
            if (ch == 3) {
                console_write("^C\n");
                pp_idx = 0;
                break;
            }
            if (pp_idx + 1 < sizeof(passphrase)) {
                passphrase[pp_idx++] = ch;
                console_putc('*');
            }
        }
        passphrase[pp_idx] = '\0';

        if (pp_idx > 0) {
            if (!enc_blkdev_init(0, passphrase)) {
                panic("Failed to unlock disk");
            }
        } else {
            console_write("[init] no passphrase, skipping disk encryption\n");
        }
        crypto_secure_zero(passphrase, sizeof(passphrase));
    } else {
        console_write("[init] no disk detected, skipping encryption\n");
    }

    vfs_init();
    console_load_ttf();

    pci_scan();
    netdev_init();
    rtl8139_init();
    virtio_net_init();

    task_init();
    thread_init();
    workqueue_init();
    fpu_init();
    start_xnx_compositor();

    interrupts_enable();

    console_printf("heap ready, initrd files: %u, total memory: %u KiB\n",
                   initrd_count(),
                   memory_total_bytes() / 1024u);
    if (task_can_run()) {
        console_write("user ABI path is live\n");
    } else {
        console_write("user ABI path is offline: not enough memory or no initrd\n");
    }
    {
        power_info_t info = power_info();
        if (info.can_shutdown && info.can_suspend && !info.suspend_uses_fallback && !info.used_emulator_fallbacks) {
            console_write("power path is live: shutdown, reboot, and suspend are ACPI-backed\n");
        } else if (info.can_shutdown && info.can_suspend && info.suspend_uses_fallback) {
            console_write("power path is live: shutdown/reboot are ready, suspend falls back to idle wait here\n");
        } else if (info.can_shutdown) {
            console_write("power path is live enough: shutdown and reboot are ready\n");
        } else {
            console_write("power path is thin: reboot is ready, shutdown needs emulator fallback\n");
        }
    }
    console_putc('\n');

    console_clear();
    console_printf("[init] launching installer...\n");
    {
        memory_defragment();
        int app_status = -1;
        if (kernel_path_is_executable("/bin/installer")) {
            const char *app_argv[] = {"/bin/installer", NULL};
            app_status = task_run_argv_alongside(NULL, "/bin/installer", 1, app_argv);
            console_printf("[init] installer exited with status %d\n", app_status);
        }
    }
    console_printf("[init] launching shell...\n");
    {
        memory_defragment();
        int app_status = -1;
        if (kernel_path_is_executable("/bin/gosh")) {
            const char *app_argv[] = {"/bin/gosh", NULL};
            app_status = task_run_argv_alongside(NULL, "/bin/gosh", 1, app_argv);
            console_printf("[init] gosh exited with status %d\n", app_status);
        } else if (kernel_path_is_executable("/bin/sh")) {
            const char *app_argv[] = {"/bin/sh", NULL};
            app_status = task_run_argv_alongside(NULL, "/bin/sh", 1, app_argv);
            console_printf("[init] sh exited with status %d\n", app_status);
        } else if (kernel_path_is_executable("/bin/terminal")) {
            const char *app_argv[] = {"/bin/terminal", NULL};
            app_status = task_run_argv_alongside(NULL, "/bin/terminal", 1, app_argv);
            console_printf("[init] terminal exited with status %d\n", app_status);
        } else {
            console_printf("[init] no shell found, halting\n");
        }
    }
    console_write("[init] app exited, halting\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
