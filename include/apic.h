#ifndef OHOS_APIC_H
#define OHOS_APIC_H

#include "spinlock.h"
#include "types.h"

/* IA32_APIC_BASE MSR */
#define IA32_APIC_BASE_MSR      0x1B
#define APIC_BASE_BSP           (1u << 8)
#define APIC_BASE_X2APIC        (1u << 10)
#define APIC_BASE_ENABLE        (1u << 11)

/* Local APIC MMIO offsets */
#define LAPIC_ID                0x020
#define LAPIC_VERSION           0x030
#define LAPIC_TPR               0x080
#define LAPIC_APR               0x090
#define LAPIC_PPR               0x0A0
#define LAPIC_EOI               0x0B0
#define LAPIC_LDR               0x0D0
#define LAPIC_DFR               0x0E0
#define LAPIC_SPURIOUS          0x0F0
#define LAPIC_ISR_BASE          0x100
#define LAPIC_TMR_BASE          0x180
#define LAPIC_IRR_BASE          0x200
#define LAPIC_ESR               0x280
#define LAPIC_ICR_LOW           0x300
#define LAPIC_ICR_HIGH          0x310
#define LAPIC_LVT_TIMER         0x320
#define LAPIC_LVT_THERMAL       0x330
#define LAPIC_LVT_PERFMON       0x340
#define LAPIC_LVT_LINT0         0x350
#define LAPIC_LVT_LINT1         0x360
#define LAPIC_LVT_ERROR         0x370
#define LAPIC_TIMER_INITCNT     0x380
#define LAPIC_TIMER_CURCNT      0x390
#define LAPIC_TIMER_DIV         0x3E0

/* Spurious vector register bits */
#define LAPIC_SPURIOUS_ENABLE   (1u << 8)
#define LAPIC_SPURIOUS_FOCUS    (1u << 9)

/* ICR bits */
#define ICR_DELIVERY_FIXED      0
#define ICR_DELIVERY_LOWPRI     (1u << 8)
#define ICR_DELIVERY_SMI        (2u << 8)
#define ICR_DELIVERY_NMI        (4u << 8)
#define ICR_DELIVERY_INIT       (5u << 8)
#define ICR_DELIVERY_STARTUP    (6u << 8)

#define ICR_DESTMODE_PHYSICAL   0
#define ICR_DESTMODE_LOGICAL    (1u << 11)

#define ICR_STATUS_PENDING      (1u << 12)
#define ICR_STATUS_DELIVER      (1u << 14)

#define ICR_LEVEL_ASSERT        0
#define ICR_LEVEL_DEASSERT      (1u << 14)

#define ICR_TRIGGER_EDGE        0
#define ICR_TRIGGER_LEVEL       (1u << 15)

#define ICR_DEST_SHIFT          24
#define ICR_DEST_SELF           0x40000
#define ICR_DEST_ALL            0x80000
#define ICR_DEST_ALL_BUT_SELF   0xC0000

/* LVT timer bits */
#define LVT_MASKED              (1u << 16)
#define LVT_TRIGGER_LEVEL       (1u << 15)
#define LVT_RW_PERIODIC         (1u << 17)
#define LVT_RW_TSCDEADLINE      (2u << 17)

/* LVT delivery modes */
#define LVT_DELIVERY_FIXED      0
#define LVT_DELIVERY_SMI        (2u << 8)
#define LVT_DELIVERY_NMI        (4u << 8)
#define LVT_DELIVERY_EXTINT     (7u << 8)

/* IO APIC registers */
#define IOAPIC_IOREGSEL         0x00
#define IOAPIC_IOWIN            0x10
#define IOAPIC_ID               0x00
#define IOAPIC_VERSION          0x01
#define IOAPIC_ARB              0x02
#define IOAPIC_REDIR_TBL_BASE   0x10

/* IO APIC redirection entry bits */
#define IOAPIC_REDIR_MASKED     (1u << 16)
#define IOAPIC_REDIR_PHYSICAL   0
#define IOAPIC_REDIR_LOGICAL    (1u << 11)

/* APIC vectors */
#define APIC_SPURIOUS_VECTOR    0xFF
#define APIC_ERROR_VECTOR       0xFE
#define APIC_TIMER_VECTOR       0xFD
#define APIC_IPI_RESCHED_VECTOR 0xFC
#define APIC_IPI_TLB_VECTOR     0xFB

/* CPU maximum */
#define MAX_CPUS                8

/* Per-CPU structure */
typedef struct {
    u32 apic_id;
    u32 cpu_number;
    bool online;
    u32 kernel_stack_top;
    u32 tss_esp0;
    /* Scheduler state */
    u32 active_task_slot;
    u32 scheduler_rr_cursor;
    int fpu_owner_slot;
    /* Per-CPU GDT/IDT pointers (point into this struct) */
    void *gdt;
    void *idt;
    /* GDT entries (8 entries × 8 bytes = 64) + TSS */
    u8 gdt_entries[64];
    u8 tss[104];
    /* Idle task kernel stack */
    u8 idle_stack[4096];
} __attribute__((aligned(64))) cpu_t;

/* Global CPU array */
extern cpu_t cpus[MAX_CPUS];
extern u32 cpu_count;
extern u32 current_cpu_id;

/* Get current CPU pointer (single-CPU: return &cpus[0] directly) */
static inline cpu_t *this_cpu(void) {
    return &cpus[0];
}

/* Get current CPU number */
static inline u32 this_cpu_id(void) {
    return this_cpu()->cpu_number;
}

/* APIC MMIO access (uses current CPU's mapped LAPIC base) */
void apic_write(u32 reg, u32 val);
u32 apic_read(u32 reg);
void apic_eoi(void);

/* APIC initialization */
void apic_init(void);
void apic_cpu_init(void);

/* IO APIC */
void ioapic_init(void);

/* IPI */
void apic_send_ipi(u32 apic_id, u32 vector);
void apic_send_init_ipi(u32 apic_id);
void apic_send_startup_ipi(u32 apic_id, u32 trampoline_page);

/* LAPIC timer */
void lapic_timer_init(u32 ticks);
void lapic_timer_calibrate(void);

/* SMP boot */
void smp_init(void);

/* Spinlock for BSP/AP coordination */
extern spinlock_t smp_boot_lock;

#endif
