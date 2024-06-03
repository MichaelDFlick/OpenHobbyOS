#include "apic.h"
#include "console.h"
#include "cpuid.h"
#include "io.h"
#include "memory.h"
#include "paging.h"
#include "panic.h"
#include "pic.h"
#include "string.h"

/* CPU array */
cpu_t cpus[MAX_CPUS];
u32 cpu_count = 1;
u32 current_cpu_id = 0;

/* Boot-time coordination */
spinlock_t smp_boot_lock = SPINLOCK_INIT;

/* LAPIC base physical address */
static u32 lapic_base_phys = 0;

/* IO APIC base physical address */
static u32 ioapic_base_phys = 0;

/* Map LAPIC MMIO into virtual address space */
static volatile u32 *lapic_mmio = NULL;

/* IO APIC MMIO virtual address */
static volatile u32 *ioapic_mmio = NULL;

void apic_write(u32 reg, u32 val) {
    if (lapic_mmio) {
        lapic_mmio[reg / 4] = val;
    }
}

u32 apic_read(u32 reg) {
    if (lapic_mmio) return lapic_mmio[reg / 4];
    return 0;
}

void apic_eoi(void) {
    apic_write(LAPIC_EOI, 0);
}

static u32 ioapic_read(u32 reg) {
    if (!ioapic_mmio) return 0;
    ioapic_mmio[0] = reg;
    return ioapic_mmio[4];
}

static void ioapic_write(u32 reg, u32 val) {
    if (!ioapic_mmio) return;
    ioapic_mmio[0] = reg;
    ioapic_mmio[4] = val;
}

static void ioapic_set_redirect(u8 irq, u32 vector, u8 apic_dest, bool masked) {
    u32 redir_low = vector | IOAPIC_REDIR_PHYSICAL;
    u32 redir_high = (u32)apic_dest << 24;
    if (masked) redir_low |= IOAPIC_REDIR_MASKED;
    ioapic_write(IOAPIC_REDIR_TBL_BASE + irq * 2, redir_low);
    ioapic_write(IOAPIC_REDIR_TBL_BASE + irq * 2 + 1, redir_high);
}

void apic_init(void) {
    u32 eax, ebx, ecx, edx;
    u64 apic_base_msr;

    /* Check for APIC via CPUID */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (!(edx & CPUID_FEATURE_EDX_APIC)) {
        console_printf("[apic] no local APIC found, staying with PIC\n");
        return;
    }

    /* Get LAPIC base from MSR */
    apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    lapic_base_phys = (u32)(apic_base_msr & 0xFFFFF000);
    console_printf("[apic] LAPIC base phys: %x\n", lapic_base_phys);

    /* Map LAPIC MMIO (one page) into kernel virtual space.
     * 0xFF800000 is a safe hole above the kernel's 896MB identity/higher-half
     * mapping (0xC0000000-0xF8000000). Cannot use KERNEL_VIRTUAL_BASE + phys
     * because phys is 0xFEE00000 — the 32-bit sum wraps and the resulting
     * virtual address isn't mapped by the page tables. */
    page_map(page_directory_get_kernel(), 0xFF800000, lapic_base_phys,
             PTE_PRESENT | PTE_RW | PTE_GLOBAL);
    lapic_mmio = (volatile u32 *)0xFF800000;

    /* Enable APIC in MSR */
    wrmsr(IA32_APIC_BASE_MSR, apic_base_msr | APIC_BASE_ENABLE);

    /* Set spurious vector and enable APIC */
    apic_write(LAPIC_SPURIOUS, APIC_SPURIOUS_VECTOR | LAPIC_SPURIOUS_ENABLE);

    /* Set LVT error handler */
    apic_write(LAPIC_LVT_ERROR, APIC_ERROR_VECTOR);

    /* Mask LINT0/LINT1 (they route PIC interrupts by default) */
    apic_write(LAPIC_LVT_LINT0, LVT_MASKED);
    apic_write(LAPIC_LVT_LINT1, LVT_MASKED);

    /* Set LDR (logical destination) and DFR (flat model) */
    apic_write(LAPIC_LDR, 0);
    apic_write(LAPIC_DFR, 0xFFFFFFFF);

    /* Clear error status by writing then reading */
    apic_write(LAPIC_ESR, 0);
    apic_read(LAPIC_ESR);

    /* Read APIC ID */
    u32 lapic_id = apic_read(LAPIC_ID) >> 24;
    cpus[0].apic_id = lapic_id;
    cpus[0].cpu_number = 0;

    console_printf("[apic] BSP APIC ID: %u\n", lapic_id);
    console_printf("[apic] APIC version: %x\n", apic_read(LAPIC_VERSION));
}

static void find_ioapic_via_madt(void) {
    /* Walk the RSDP to find MADT, then walk MADT entries */
    /* For now, probe common IOAPIC physical addresses using page mappings.
     * IOAPIC access requires writing the register index to offset 0,
     * then reading the value from offset 0x10 (ioapic_mmio[4]). */
    u32 candidates[] = { 0xFEC00000, 0xFEC01000, 0xFEC02000 };
    for (int i = 0; i < 3; i++) {
        page_map(page_directory_get_kernel(), 0xFF802000, candidates[i],
                 PTE_PRESENT | PTE_RW | PTE_GLOBAL);
        volatile u32 *mmio = (volatile u32 *)0xFF802000;
        mmio[0] = 0x01;  /* IOAPICVER register index */
        u32 version = mmio[4];
        mmio[0] = 0x00;  /* IOAPICID register index */
        u32 id = mmio[4];
        if (version != 0 && version != 0xFFFFFFFF) {
            ioapic_base_phys = candidates[i];
            console_printf("[ioapic] probed id=%u version=%x at %x\n",
                           id, version, candidates[i]);
            return;
        }
    }
}

void ioapic_init(void) {
    find_ioapic_via_madt();

    if (!ioapic_base_phys) {
        console_printf("[ioapic] no IOAPIC found, falling back to PIC via LINT0\n");
        /* Re-enable LINT0 in ExtInt mode so PIC interrupts reach the LAPIC */
        apic_write(LAPIC_LVT_LINT0, LVT_DELIVERY_EXTINT);
        return;
    }

    ioapic_mmio = (volatile u32 *)0xFF801000;
    page_map(page_directory_get_kernel(), 0xFF801000, ioapic_base_phys,
             PTE_PRESENT | PTE_RW | PTE_GLOBAL);

    u32 ioapic_id = ioapic_read(IOAPIC_ID);
    u32 ioapic_ver = ioapic_read(IOAPIC_VERSION);
    u32 max_redirect = (ioapic_ver >> 16) & 0xFF;

    console_printf("[ioapic] id=%u version=%x max_redirect=%u\n",
                   ioapic_id, ioapic_ver, max_redirect);

    /* Mask all IOAPIC redirects initially */
    for (u32 i = 0; i <= max_redirect; i++) {
        ioapic_set_redirect(i, 0, 0, true);
    }

    /* Disable the legacy PIC since we're switching to IOAPIC */
    pic_disable();

    /* Route legacy IRQs via IOAPIC to the same vectors (32-47) */
    for (int i = 0; i < 16; i++) {
        ioapic_set_redirect(i, 32 + i, 0, false);
    }
}

void apic_send_ipi(u32 apic_id, u32 vector) {
    /* Wait for previous send to complete */
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) {
        cpu_pause();
    }

    apic_write(LAPIC_ICR_HIGH, apic_id << ICR_DEST_SHIFT);
    apic_write(LAPIC_ICR_LOW, vector | ICR_DELIVERY_FIXED | ICR_DESTMODE_PHYSICAL);

    /* Wait for delivery */
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) {
        cpu_pause();
    }
}

void apic_send_init_ipi(u32 apic_id) {
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) cpu_pause();
    apic_write(LAPIC_ICR_HIGH, apic_id << ICR_DEST_SHIFT);
    apic_write(LAPIC_ICR_LOW, ICR_DELIVERY_INIT | ICR_DESTMODE_PHYSICAL | ICR_LEVEL_ASSERT);
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) cpu_pause();
}

void apic_send_startup_ipi(u32 apic_id, u32 trampoline_page) {
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) cpu_pause();
    apic_write(LAPIC_ICR_HIGH, apic_id << ICR_DEST_SHIFT);
    apic_write(LAPIC_ICR_LOW, ICR_DELIVERY_STARTUP | ICR_DESTMODE_PHYSICAL | trampoline_page);
    while (apic_read(LAPIC_ICR_LOW) & ICR_STATUS_PENDING) cpu_pause();
}

void lapic_timer_calibrate(void) {
    /* Use PIT (channel 2, mode 0) to calibrate LAPIC timer */
    /* Set PIT to count down from 0xFFFF at ~1.19318 MHz */
    outb(0x43, 0xB0);
    outb(0x42, 0xFF);
    outb(0x42, 0xFF);

    /* Ensure PIT channel 2 gate is enabled (port 0x61 bit 0) */
    u8 speaker = inb(0x61);
    outb(0x61, speaker | 0x01);

    /* Start LAPIC timer at max divide, zero counter */
    apic_write(LAPIC_TIMER_DIV, 0x0B);
    apic_write(LAPIC_TIMER_INITCNT, 0xFFFFFFFF);

    /* Wait for PIT channel 2 OUT pin to go high (counter reaches 0).
     * Port 0x61 bit 5 is the PIT channel 2 OUT status.
     * Timeout after ~200ms to prevent infinite hang if PIT is broken. */
    u32 timeout = 200000;
    while (((inb(0x61) & 0x20) == 0) && --timeout) {
        cpu_pause();
    }

    u32 ticks_elapsed;
    if (timeout == 0) {
        /* PIT didn't respond — use a reasonable fallback */
        ticks_elapsed = 5500000;  /* ~100 MHz bus for 55ms */
    } else {
        ticks_elapsed = 0xFFFFFFFF - apic_read(LAPIC_TIMER_CURCNT);
    }

    /* Stop the timer */
    apic_write(LAPIC_TIMER_INITCNT, 0);

    /* Calculate bus frequency in KHz: ticks / 55ms = bus frequency in Hz / 1000 */
    u32 bus_freq_khz = ticks_elapsed / 55;
    if (bus_freq_khz < 100) bus_freq_khz = 100000; /* fallback for broken calibration */

    console_printf("[lapic] calibration: %u ticks in 55ms, bus ~%u KHz\n",
                   ticks_elapsed, bus_freq_khz);

    /* Store calibrated ticks per ms */
    u32 ticks_per_ms = (ticks_elapsed * 1000) / 55;

    /* Set up LAPIC timer for 100 Hz (10ms interval) */
    lapic_timer_init(ticks_per_ms * 10);
}

void lapic_timer_init(u32 ticks) {
    apic_write(LAPIC_TIMER_DIV, 0x0B);            /* divide by 1 */
    apic_write(LAPIC_LVT_TIMER, APIC_TIMER_VECTOR | LVT_RW_PERIODIC);
    apic_write(LAPIC_TIMER_INITCNT, ticks);
}

void apic_cpu_init(void) {
    u32 lapic_id = (apic_read(LAPIC_ID) >> 24);

    cpu_t *cpu = NULL;
    for (u32 i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == lapic_id) {
            cpu = &cpus[i];
            break;
        }
    }

    if (!cpu) {
        console_printf("[apic] WARNING: unknown APIC ID %u, using slot %u\n",
                       lapic_id, cpu_count);
        cpu = &cpus[cpu_count++];
        cpu->apic_id = lapic_id;
    }

    cpu->online = true;

    /* Set up per-CPU LAPIC */
    apic_write(LAPIC_SPURIOUS, APIC_SPURIOUS_VECTOR | LAPIC_SPURIOUS_ENABLE);
    apic_write(LAPIC_LVT_ERROR, APIC_ERROR_VECTOR);
    apic_write(LAPIC_LVT_LINT0, LVT_MASKED);
    apic_write(LAPIC_LVT_LINT1, LVT_MASKED);
    apic_write(LAPIC_ESR, 0);
    apic_read(LAPIC_ESR);
}
