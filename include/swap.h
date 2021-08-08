#ifndef OHOS_SWAP_H
#define OHOS_SWAP_H

#include "paging.h"
#include "types.h"

/* Swap flags saved per slot (original PTE bits to restore on swap-in) */
#define SWAP_SAVE_FLAGS (PTE_USER | PTE_RW | PTE_COW | PTE_GLOBAL)

bool swap_init(u32 blkdev_id, u32 start_lba, u32 sector_count);

u32 swap_out_page(u32 phys_addr, u16 pte_flags);
bool swap_in_page(u32 slot, u32 phys_addr, u16 *out_flags);
void swap_free_slot(u32 slot);

u32 swap_total_slots(void);
u32 swap_used_slots(void);
u32 swap_free_slots(void);

bool swap_try_evict(void);

#endif
