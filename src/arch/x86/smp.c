#include "apic.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "memory.h"
#include "paging.h"
#include "panic.h"
#include "pic.h"
#include "string.h"

/* Low-memory trampoline page and mailbox */
#define TRAMPOLINE_PAGE     0x8000
#define TRAMPOLINE_PAGE_NUM (TRAMPOLINE_PAGE / 4096)
#define MAILBOX_ADDR        0x1000

/* Mailbox structure */
typedef struct {
    u32 entry_point;     /* 32-bit protected mode entry */
    u32 stack_top;       /* stack for the AP */
    u32 cr3;             /* page directory physical address */
    u32 cpu_number;      /* which CPU this is */
    u32 gdtr_base;       /* base address of GDT */
    u16 gdtr_limit;      /* limit of GDT */
    volatile u32 go;     /* set to 1 by BSP to start the AP */
} smp_mailbox_t;

static smp_mailbox_t *mailbox = (smp_mailbox_t *)MAILBOX_ADDR;

/* External trampoline boundaries (from objcopy binary embedding) */
extern const u8 _binary_build_smp_trampoline_bin_start[];
extern const u8 _binary_build_smp_trampoline_bin_end[];

/* ACPI RSDT/MADT types */
#define ACPI_MADT_SIGNATURE "APIC"
#define ACPI_MADT_TYPE_LOCAL_APIC   0
#define ACPI_MADT_TYPE_IO_APIC      1
#define ACPI_MADT_TYPE_INT_SRC_OVERRIDE 2

typedef struct PACKED {
    char signature[4];
    u32 length;
    u8 revision;
    u8 checksum;
    char oem_id[6];
    char oem_table_id[8];
    char oem_revision[4];
    char creator_id[4];
    char creator_revision[4];
} acpi_sdt_header_t;

typedef struct PACKED {
    u8 type;
    u8 length;
} acpi_madt_entry_header_t;

typedef struct PACKED {
    acpi_sdt_header_t header;
    u32 lapic_phys_base;
    u32 flags;
    /* Followed by type-length-value entries */
} acpi_madt_t;

typedef struct PACKED {
    acpi_madt_entry_header_t header;
    u8 acpi_processor_id;
    u8 apic_id;
    u32 flags;
} acpi_madt_lapic_t;

typedef struct PACKED {
    acpi_madt_entry_header_t header;
    u8 acpi_id;
    u8 apic_id;
    u32 base_address;
    u32 global_system_interrupt_base;
} acpi_madt_ioapic_t;

/* AP entry point (runs in 32-bit protected mode with paging on) */
static void ap_entry(void) __attribute__((noreturn));

/* BSP-side helpers */
static const acpi_sdt_header_t *acpi_find_rsdt(void);
static const acpi_sdt_header_t *acpi_find_table(const acpi_sdt_header_t *rsdt, const char *signature);

static bool checksum_ok(const void *table, u32 length) {
    const u8 *bytes = (const u8 *)table;
    u8 sum = 0;
    for (u32 i = 0; i < length; ++i) {
        sum = (u8)(sum + bytes[i]);
    }
    return sum == 0;
}

#define EBDA_SEGMENT (*(volatile u16 *)(uintptr_t)0x40E)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static const acpi_sdt_header_t *acpi_scan_rsdp_range(u32 start, u32 end) {
#pragma GCC diagnostic pop
    for (u32 addr = start; addr + 20 <= end; addr += 16) {
        const volatile char *sig = (const volatile char *)(uintptr_t)addr;
        if (sig[0] == 'R' && sig[1] == 'S' && sig[2] == 'D' && sig[3] == ' ' &&
            sig[4] == 'P' && sig[5] == 'T' && sig[6] == 'R' && sig[7] == ' ') {
            const u8 *rsdp = (const u8 *)(uintptr_t)addr;
            u8 sum = 0;
            for (int i = 0; i < 20; i++) sum = (u8)(sum + rsdp[i]);
            if (sum == 0)
                return (const acpi_sdt_header_t *)(uintptr_t)(*(const u32 *)(addr + 16));
        }
    }
    return NULL;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static const acpi_sdt_header_t *acpi_find_rsdt(void) {
    const acpi_sdt_header_t *rsdt;
    u32 ebda = (u32)EBDA_SEGMENT << 4;
#pragma GCC diagnostic pop
    if (ebda >= 0x80000u && ebda < 0xA0000u) {
        rsdt = acpi_scan_rsdp_range(ebda, ebda + 1024);
        if (rsdt) return rsdt;
    }
    return acpi_scan_rsdp_range(0xE0000u, 0x100000u);
}

static const acpi_sdt_header_t *acpi_find_table(const acpi_sdt_header_t *rsdt, const char *signature) {
    if (!rsdt || rsdt->length < sizeof(acpi_sdt_header_t)) return NULL;
    const u32 *entries = (const u32 *)((const u8 *)rsdt + sizeof(acpi_sdt_header_t));
    u32 count = (rsdt->length - sizeof(acpi_sdt_header_t)) / sizeof(u32);
    for (u32 i = 0; i < count; ++i) {
        const acpi_sdt_header_t *table = (const acpi_sdt_header_t *)(uintptr_t)entries[i];
        if (!table || !checksum_ok(table, table->length)) continue;
        if (memcmp(table->signature, signature, 4) == 0) return table;
    }
    return NULL;
}

/* Parse MADT and count/enumerate CPUs */
static void smp_enumerate_cpus(void) {
    const acpi_sdt_header_t *rsdt = acpi_find_rsdt();
    if (!rsdt) {
        console_printf("[smp] no RSDT found, SMP disabled\n");
        return;
    }

    const acpi_madt_t *madt = (const acpi_madt_t *)acpi_find_table(rsdt, ACPI_MADT_SIGNATURE);
    if (!madt) {
        console_printf("[smp] no MADT found, SMP disabled\n");
        return;
    }

    console_printf("[smp] MADT at %x, LAPIC base %x\n",
                   (u32)(uintptr_t)madt, madt->lapic_phys_base);

    const u8 *pos = (const u8 *)(madt + 1);
    const u8 *end = (const u8 *)madt + madt->header.length;

    while (pos + 2 <= end) {
        u8 type = pos[0];
        u8 len  = pos[1];
        if (len < 2 || pos + len > end) break;

        if (type == ACPI_MADT_TYPE_LOCAL_APIC) {
            const acpi_madt_lapic_t *lapic = (const acpi_madt_lapic_t *)pos;
            u32 enabled = (lapic->flags & 1);

            console_printf("[smp]   LAPIC: proc=%u apic=%u flags=%x%s\n",
                           lapic->acpi_processor_id, lapic->apic_id, lapic->flags,
                           enabled ? "" : " (disabled)");

            if (enabled && cpu_count < MAX_CPUS) {
                /* Check if this is the BSP (APIC ID already in cpus[0]) */
                bool is_bsp = (lapic->apic_id == cpus[0].apic_id);
                if (!is_bsp) {
                    cpu_t *cpu = &cpus[cpu_count];
                    cpu->apic_id = lapic->apic_id;
                    cpu->cpu_number = cpu_count;
                    cpu->online = false;
                    cpu->gdt = cpu->gdt_entries;
                    cpu->idt = NULL;
                    cpu_count++;
                    console_printf("[smp]   AP %u: APIC ID %u\n",
                                   cpu_count - 1, lapic->apic_id);
                }
            }
        } else if (type == ACPI_MADT_TYPE_INT_SRC_OVERRIDE) {
            /* Skip overrides for now (we just remap all 16 IRQs) */
        }

        pos += len;
    }

    console_printf("[smp] %u CPU(s) total (%u AP(s))\n", cpu_count, cpu_count - 1);
}

/* Copy trampoline to low memory */
static void smp_setup_trampoline(u32 ap_entry_phys) {
    /* Copy trampoline code to low page */
    u32 size = (u32)(_binary_build_smp_trampoline_bin_end - _binary_build_smp_trampoline_bin_start);
    memcpy((void *)(uintptr_t)TRAMPOLINE_PAGE,
           (const void *)_binary_build_smp_trampoline_bin_start, size);

    /* Fill in the mailbox */
    memset(mailbox, 0, sizeof(*mailbox));
    mailbox->entry_point = ap_entry_phys;
    mailbox->cr3 = paging_get_cr3();
    mailbox->go = 0;

    console_printf("[smp] trampoline at %x, size %u, mailbox at %x\n",
                   TRAMPOLINE_PAGE, size, MAILBOX_ADDR);
}

/* Send INIT-SIPI-SIPI sequence to wake an AP */
static void smp_wake_ap(u32 apic_id, u32 cpu_number) {
    cpu_t *cpu = &cpus[cpu_number];

    /* Allocate a stack for this AP */
    u32 *ap_stack = (u32 *)kmalloc_aligned(16384, 16);
    if (!ap_stack) {
        console_printf("[smp] failed to allocate stack for CPU %u\n", cpu_number);
        return;
    }

    /* Set up per-CPU data */
    cpu->kernel_stack_top = (u32)ap_stack + 16384;
    cpu->online = false;

    /* Set up AP's mailbox */
    mailbox->stack_top  = cpu->kernel_stack_top;
    mailbox->cpu_number = cpu_number;
    mailbox->go         = 1;

    /* Ensure mailbox writes are visible */
    __asm__ volatile ("" : : : "memory");

    /* Send INIT IPI */
    console_printf("[smp] sending INIT to APIC %u\n", apic_id);
    apic_send_init_ipi(apic_id);

    /* Wait 10ms (PIT-based delay) */
    for (volatile u32 d = 0; d < 100000; d++) cpu_pause();

    /* Send first SIPI */
    console_printf("[smp] sending SIPI to APIC %u (page %x)\n",
                   apic_id, TRAMPOLINE_PAGE_NUM);
    apic_send_startup_ipi(apic_id, TRAMPOLINE_PAGE_NUM);

    /* Wait ~200us (typical IPI delivery time) */
    for (volatile u32 d = 0; d < 2000; d++) cpu_pause();

    /* Send second SIPI */
    apic_send_startup_ipi(apic_id, TRAMPOLINE_PAGE_NUM);

    /* Wait for AP to signal ready (max timeout) */
    u32 timeout = 5000000;
    while (timeout--) {
        if (cpu->online) {
            console_printf("[smp] AP %u (APIC %u) is online\n",
                           cpu_number, apic_id);
            return;
        }
        cpu_pause();
    }

    console_printf("[smp] WARNING: AP %u (APIC %u) failed to come online\n",
                   cpu_number, apic_id);
}

/* AP initialization - runs on the AP after trampoline jump */
static void ap_entry_impl(u32 cpu_number) {
    cpu_t *cpu = &cpus[cpu_number];

    /* Initialize per-CPU GDT */
    gdt_init(cpu, cpu->kernel_stack_top);

    /* Load IDT (same as BSP - shared) */
    idt_load_current();

    /* Initialize per-CPU LAPIC */
    apic_cpu_init();

    /* Calibrate and start LAPIC timer on this AP */
    apic_write(LAPIC_TIMER_DIV, 0x0B);
    apic_write(LAPIC_LVT_TIMER, APIC_TIMER_VECTOR | LVT_RW_PERIODIC);
    apic_write(LAPIC_TIMER_INITCNT, 0);

    /* Signal BSP that we're alive */
    cpu->online = true;

    /* Enable interrupts */
    interrupts_enable();

    console_printf("[smp] AP %u (APIC %u) initialized\n",
                   cpu_number, cpu->apic_id);

    /* Idle loop */
    for (;;) {
        cpu_halt();
    }
}

/* Trampolines for the linker + NASM */

/* The ap_entry needs to be at a known physical address for the mailbox.
   We place it in the kernel .text section (accessible via higher-half). */

/* AP entry point - called from trampoline */
__attribute__((noreturn))
static void ap_entry(void) {
    u32 cpu_number = mailbox->cpu_number;

    /* We're now running in the kernel's page tables with our own stack */
    ap_entry_impl(cpu_number);

    /* Never reached */
    for (;;) cpu_halt();
}

void smp_init(void) {
    console_printf("[smp] starting SMP initialization\n");

    /* Enumerate CPUs from MADT */
    smp_enumerate_cpus();

    if (cpu_count <= 1) {
        console_printf("[smp] single-CPU system, skipping SMP\n");
        return;
    }

    /* Get entry point (virtual address, same page tables on AP) */
    u32 ap_entry_virt = (u32)(uintptr_t)&ap_entry;

    /* Set up trampoline in low memory */
    smp_setup_trampoline(ap_entry_virt);

    /* Wake each AP */
    for (u32 i = 1; i < cpu_count; i++) {
        smp_wake_ap(cpus[i].apic_id, i);
    }

    console_printf("[smp] SMP initialization complete: %u CPU(s) online\n", cpu_count);
}
