#include "swap.h"

#include "blkdev.h"
#include "console.h"
#include "memory.h"
#include "paging.h"
#include "string.h"
#include "task.h"

#define SWAP_SECTORS_PER_PAGE 8
#define SWAP_SLOT_INVALID     0xFFFFFFFFu

static struct {
    u32 blkdev_id;
    u32 start_lba;
    u32 total_sectors;
    u32 total_slots;
    u32 used_slots;
    u8 *slot_bitmap;
    u8 *slot_flags;
    bool active;
} swap_dev;

static bool swap_slot_is_free(u32 slot) {
    u32 byte_idx = slot / 8;
    u32 bit_idx = slot % 8;
    return !(swap_dev.slot_bitmap[byte_idx] & (1u << bit_idx));
}

static void swap_slot_mark_used(u32 slot) {
    u32 byte_idx = slot / 8;
    u32 bit_idx = slot % 8;
    swap_dev.slot_bitmap[byte_idx] |= (1u << bit_idx);
}

static void swap_slot_mark_free(u32 slot) {
    u32 byte_idx = slot / 8;
    u32 bit_idx = slot % 8;
    swap_dev.slot_bitmap[byte_idx] &= ~(1u << bit_idx);
}

bool swap_init(u32 blkdev_id, u32 start_lba, u32 sector_count) {
    u32 bitmap_bytes;

    memset(&swap_dev, 0, sizeof(swap_dev));

    if (!blkdev_present(blkdev_id)) {
        console_write("[swap] no block device, swap disabled\n");
        return false;
    }

    sector_count = (sector_count / SWAP_SECTORS_PER_PAGE) * SWAP_SECTORS_PER_PAGE;
    if (sector_count < SWAP_SECTORS_PER_PAGE) {
        console_write("[swap] too few sectors, swap disabled\n");
        return false;
    }

    swap_dev.blkdev_id = blkdev_id;
    swap_dev.start_lba = start_lba;
    swap_dev.total_sectors = sector_count;
    swap_dev.total_slots = sector_count / SWAP_SECTORS_PER_PAGE;
    swap_dev.used_slots = 0;

    bitmap_bytes = (swap_dev.total_slots + 7) / 8;
    swap_dev.slot_bitmap = (u8 *)kmalloc(bitmap_bytes);
    if (!swap_dev.slot_bitmap) {
        console_write("[swap] failed to allocate bitmap\n");
        return false;
    }
    memset(swap_dev.slot_bitmap, 0, bitmap_bytes);

    swap_dev.slot_flags = (u8 *)kmalloc(swap_dev.total_slots);
    if (!swap_dev.slot_flags) {
        kfree(swap_dev.slot_bitmap);
        console_write("[swap] failed to allocate flags array\n");
        return false;
    }
    memset(swap_dev.slot_flags, 0, swap_dev.total_slots);

    swap_dev.active = true;

    console_printf("[swap] initialized: dev=%u lba=%u sectors=%u slots=%u\n",
                   blkdev_id, start_lba, sector_count, swap_dev.total_slots);
    return true;
}

static u32 swap_alloc_slot(u16 pte_flags) {
    u32 slot;

    if (!swap_dev.active) return SWAP_SLOT_INVALID;

    for (slot = 0; slot < swap_dev.total_slots; slot++) {
        if (swap_slot_is_free(slot)) {
            swap_slot_mark_used(slot);
            swap_dev.slot_flags[slot] = (u8)(pte_flags & SWAP_SAVE_FLAGS);
            swap_dev.used_slots++;
            return slot;
        }
    }
    return SWAP_SLOT_INVALID;
}

void swap_free_slot(u32 slot) {
    if (!swap_dev.active || slot >= swap_dev.total_slots) return;
    if (swap_slot_is_free(slot)) return;

    swap_slot_mark_free(slot);
    swap_dev.slot_flags[slot] = 0;
    swap_dev.used_slots--;
}

u32 swap_out_page(u32 phys_addr, u16 pte_flags) {
    u32 slot;
    u32 lba;
    int ret;

    slot = swap_alloc_slot(pte_flags);
    if (slot == SWAP_SLOT_INVALID) return SWAP_SLOT_INVALID;

    lba = swap_dev.start_lba + slot * SWAP_SECTORS_PER_PAGE;
    ret = blkdev_write(swap_dev.blkdev_id, lba, SWAP_SECTORS_PER_PAGE,
                       (const void *)(uintptr_t)phys_addr);
    if (ret != 0) {
        swap_free_slot(slot);
        return SWAP_SLOT_INVALID;
    }

    return slot;
}

bool swap_in_page(u32 slot, u32 phys_addr, u16 *out_flags) {
    u32 lba;
    int ret;

    if (!swap_dev.active || slot >= swap_dev.total_slots) return false;
    if (swap_slot_is_free(slot)) return false;

    lba = swap_dev.start_lba + slot * SWAP_SECTORS_PER_PAGE;
    ret = blkdev_read(swap_dev.blkdev_id, lba, SWAP_SECTORS_PER_PAGE,
                      (void *)(uintptr_t)phys_addr);
    if (ret != 0) return false;

    if (out_flags) {
        *out_flags = swap_dev.slot_flags[slot];
    }

    swap_free_slot(slot);
    return true;
}

u32 swap_total_slots(void) {
    return swap_dev.active ? swap_dev.total_slots : 0;
}

u32 swap_used_slots(void) {
    return swap_dev.active ? swap_dev.used_slots : 0;
}

u32 swap_free_slots(void) {
    return swap_dev.active ? (swap_dev.total_slots - swap_dev.used_slots) : 0;
}

bool swap_try_evict(void) {
    int slot_idx;
    u32 pd_idx, pt_idx;
    page_directory_t *pd;
    u32 pt_phys;
    page_table_t *pt;
    u32 pte;
    u32 phys;
    u16 flags;
    u32 swap_slot;
    u32 virt;

    if (!swap_dev.active) return false;

    slot_idx = task_active_slot_index();
    if (slot_idx < 0) return false;

    pd = (page_directory_t *)task_slot_page_dir(slot_idx);
    if (!pd) return false;

    page_directory_switch(pd);

    for (pd_idx = 0; pd_idx < KERNEL_PD_INDEX; pd_idx++) {
        if (!(pd->entries[pd_idx] & PTE_PRESENT)) continue;
        if (!(pd->entries[pd_idx] & PTE_ALLOCATED)) continue;

        pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = (page_table_t *)(KERNEL_VIRTUAL_BASE + pt_phys);

        for (pt_idx = 0; pt_idx < ENTRIES_PER_PT; pt_idx++) {
            pte = pt->entries[pt_idx];
            if (!(pte & PTE_PRESENT)) continue;
            if (!(pte & PTE_ALLOCATED)) continue;
            if (pte & PTE_COW) continue;

            flags = entry_get_flags(pte);

            if (!(flags & PTE_ACCESSED)) {
                goto found_eviction_candidate;
            }

            pt->entries[pt_idx] = pte & ~PTE_ACCESSED;
            paging_invalidate_tlb(
                (pd_idx << 22) | (pt_idx << 12));
        }
    }

    return false;

found_eviction_candidate:
    virt = (pd_idx << 22) | (pt_idx << 12);
    phys = entry_get_phys(pte);

    if (phys == 0) return false;

    swap_slot = swap_out_page(phys, (u16)(flags & ~PTE_PRESENT));
    if (swap_slot == SWAP_SLOT_INVALID) return false;

    pt->entries[pt_idx] = PTE_SWAPPED | ((swap_slot & 0xFFFFF) << 12)
                          | (flags & (PTE_USER | PTE_RW));
    paging_invalidate_tlb(virt);

    frame_free(phys);

    return true;
}
