#include "paging.h"

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "string.h"
#include "task.h"
#include "swap.h"

/* Frame allocator state */
static frame_allocator_t g_frame_allocator;
static page_directory_t *kernel_page_directory = NULL;
static u32 kernel_page_directory_phys = 0;
static bool paging_active = false;

/* Convert virtual kernel address to physical (assumes identity-mapped during early boot) */
static inline u32 virt_to_phys_early(void *virt) {
    return (u32)(uintptr_t)virt;
}

/* Return a pointer to a page table at the given physical address.
 * Before paging is enabled (paging_init) the CPU uses physical addresses
 * directly, so we must use identity-mapped pointers.  After paging is
 * enabled we rely on the kernel's higher-half mapping
 * (KERNEL_VIRTUAL_BASE → phys 0 for 1 GB) which is present in every
 * page directory via the copied kernel PDEs at indices 768–1023. */
static inline page_table_t *ptable_ptr(u32 pt_phys) {
    if (paging_active) {
        return (page_table_t *)(KERNEL_VIRTUAL_BASE + pt_phys);
    }
    return (page_table_t *)(uintptr_t)pt_phys;
}

/*
 * Physical Frame Allocator
 * Uses bitmap to track free/used frames
 */

bool frame_alloc_init(u32 memory_start, u32 memory_end, u32 first_frame) {
    u32 total_memory = memory_end - memory_start;
    u32 num_frames = total_memory / PAGE_SIZE;
    u32 bitmap_size = ((num_frames + 31) / 32) * sizeof(u32);
    
    /* Allocate bitmap from kernel heap */
    g_frame_allocator.bitmap = kmalloc(bitmap_size);
    if (!g_frame_allocator.bitmap) {
        return false;
    }
    
    memset(g_frame_allocator.bitmap, 0, bitmap_size);
    g_frame_allocator.total_frames = num_frames;
    g_frame_allocator.used_frames = 0;
    g_frame_allocator.first_frame = memory_start / PAGE_SIZE;
    (void)first_frame;
    
    console_printf("[paging] frame allocator: %u frames (%u MiB), bitmap at %p\n",
                   num_frames, total_memory / (1024 * 1024), g_frame_allocator.bitmap);
    
    return true;
}

u32 frame_alloc(void) {
    for (int attempt = 0; attempt < 2; attempt++) {
        for (u32 i = 0; i < g_frame_allocator.total_frames; i++) {
            u32 idx = i / 32;
            u32 bit = i % 32;

            if (!(g_frame_allocator.bitmap[idx] & (1U << bit))) {
                g_frame_allocator.bitmap[idx] |= (1U << bit);
                g_frame_allocator.used_frames++;
                return (g_frame_allocator.first_frame + i) * PAGE_SIZE;
            }
        }
        if (attempt == 0) {
            if (!swap_try_evict()) break;
        }
    }
    return 0; /* Out of memory */
}

void frame_free(u32 phys_addr) {
    if (phys_addr == 0) return;
    
    u32 frame = phys_addr / PAGE_SIZE;
    if (frame < g_frame_allocator.first_frame || frame >= g_frame_allocator.first_frame + g_frame_allocator.total_frames) {
        return;
    }
    
    u32 i = frame - g_frame_allocator.first_frame;
    u32 idx = i / 32;
    u32 bit = i % 32;
    
    g_frame_allocator.bitmap[idx] &= ~(1U << bit);
    g_frame_allocator.used_frames--;
}

u32 frame_total(void) {
    return g_frame_allocator.total_frames;
}

u32 frame_used(void) {
    return g_frame_allocator.used_frames;
}

u32 frame_free_count(void) {
    return g_frame_allocator.total_frames - g_frame_allocator.used_frames;
}

/*
 * Page Directory/Table Management
 */

page_directory_t *page_directory_create(void) {
    u32 pd_phys = frame_alloc();
    if (!pd_phys) return NULL;

    page_directory_t *pd = (page_directory_t *)(KERNEL_VIRTUAL_BASE + pd_phys);
    memset(pd, 0, sizeof(page_directory_t));

    if (kernel_page_directory) {
        /*
         * Copy all kernel identity-mapped PDEs into the user PD so the
         * kernel heap (which may sit at physical addresses anywhere in
         * the 0-896 MB identity window) remains accessible when the user
         * page directory is active.  The PTEs themselves are supervisor-
         * only (no PTE_USER), so user code cannot touch kernel heap pages.
         * page_map() checks PTE_ALLOCATED and will allocate a fresh page
         * table if the PDE is a shared kernel table, preventing user page
         * mappings from corrupting kernel heap page tables.
         * Clear PTE_ALLOCATED so page_directory_destroy knows these page
         * tables belong to the kernel.
         */
        for (u32 i = 0; i < KERNEL_PD_INDEX; i++) {
            if (kernel_page_directory->entries[i] & PTE_PRESENT) {
                pd->entries[i] = kernel_page_directory->entries[i] & ~PTE_ALLOCATED;
            }
        }

        /* Copy kernel higher-half mappings (768-1023 = 3GB-4GB) */
        for (int i = KERNEL_PD_INDEX; i < ENTRIES_PER_PD; i++) {
            pd->entries[i] = kernel_page_directory->entries[i];
        }

        /*
         * Eagerly map physical page 0 with user read/write permission so
         * programs (gosh, Qt6, etc.) can read legacy IVT/BDA data without
         * triggering a NULL page fault.  We allocate a private page table
         * for PDE[0] (replacing the shared kernel identity PT), copy the
         * existing kernel identity PTEs, then set PTE_USER on PTE[0].
         */
        if ((pd->entries[0] & PTE_PRESENT) && !(pd->entries[0] & PTE_ALLOCATED)) {
            u32 old_pt_phys = entry_get_phys(pd->entries[0]);
            page_table_t *old_pt = ptable_ptr(old_pt_phys);

            u32 pt_phys = frame_alloc();
            if (pt_phys) {
                page_table_t *pt = ptable_ptr(pt_phys);
                memcpy(pt, old_pt, sizeof(page_table_t));
                pt->entries[0] = entry_create(0, PTE_PRESENT | PTE_USER | PTE_RW);
                pd->entries[0] = entry_create(pt_phys,
                    PTE_PRESENT | PTE_RW | PTE_ALLOCATED | PTE_USER);
            }
        }
    }

    return pd;
}

void page_directory_destroy(page_directory_t *pd) {
    if (!pd) return;
    
    /* Free user page tables (those allocated by this process, not shared kernel identity) */
    for (u32 i = 0; i < KERNEL_PD_INDEX; i++) {
        if (!(pd->entries[i] & PTE_PRESENT)) {
            continue;
        }
        
        /* Skip kernel-owned PDEs (identity-mapped, copied without PTE_ALLOCATED) */
        if (!(pd->entries[i] & PTE_ALLOCATED)) {
            continue;
        }
        
        u32 pt_phys = entry_get_phys(pd->entries[i]);
        
        /* Free all frames in this page table */
        page_table_t *pt = ptable_ptr( pt_phys);
        for (int j = 0; j < ENTRIES_PER_PT; j++) {
            if (pt->entries[j] & PTE_PRESENT) {
                u32 flags = entry_get_flags(pt->entries[j]);
                if (flags & PTE_ALLOCATED) {
                    frame_free(entry_get_phys(pt->entries[j]));
                }
            }
        }
        
        /* Free the page table itself */
        if (pd->entries[i] & PTE_ALLOCATED) {
            frame_free(pt_phys);
        }
    }
    
    u32 pd_phys = (u32)(uintptr_t)pd;
    if (pd_phys >= KERNEL_VIRTUAL_BASE) {
        pd_phys -= KERNEL_VIRTUAL_BASE;
    }
    frame_free(pd_phys);
}

void page_directory_switch(page_directory_t *pd) {
    if (!pd) return;
    u32 phys_addr = (u32)(uintptr_t)pd;
    if (phys_addr >= KERNEL_VIRTUAL_BASE) {
        phys_addr -= KERNEL_VIRTUAL_BASE;
    }
    paging_set_cr3(phys_addr);
}

page_directory_t *page_directory_get_current(void) {
    u32 phys = paging_get_cr3();
    if (kernel_page_directory && phys == kernel_page_directory_phys) {
        return kernel_page_directory;
    }
    return (page_directory_t *)(KERNEL_VIRTUAL_BASE + phys);
}

page_directory_t *page_directory_get_kernel(void) {
    return kernel_page_directory;
}

void page_directory_switch_to_kernel(void) {
    if (kernel_page_directory != NULL) {
        page_directory_switch(kernel_page_directory);
    }
}

/*
 * Page Mapping Operations
 */

bool page_map(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags) {
    if (!pd || !is_page_aligned(virt_addr) || !is_page_aligned(phys_addr)) {
        return false;
    }
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        /* Allocate new page table */
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;
        
        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else if (pd != kernel_page_directory && !(pd->entries[pd_idx] & PTE_ALLOCATED)) {
        /*
         * PDE was copied from the kernel (shared identity mapping).
         * Allocate a private page table so we don't corrupt kernel heap
         * page table entries with user mappings.
         *
         * IMPORTANT: Copy the existing kernel identity PTEs so the kernel
         * can still access physical memory via identity mapping in this
         * 4M window.  Without this, the CoW handler's memcpy((void*)phys)
         * faults when the physical address falls in a PDE whose shared
         * kernel identity PT was replaced with a private (mostly-zero) PT.
         */
        u32 old_pt_phys = entry_get_phys(pd->entries[pd_idx]);
        page_table_t *old_pt = ptable_ptr(old_pt_phys);

        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;
        
        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        
        pt = ptable_ptr( pt_phys);
        memcpy(pt, old_pt, sizeof(page_table_t));

        /* PDE topology changed — flush entire TLB (not just one page) to
         * discard stale identity-mapped entries for the whole 4M window.
         * A single invlpg is insufficient because the PDE changed for all
         * 1024 PTEs in this 4M region. */
        paging_flush_tlb();
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);
        
        /* Ensure PDE has user bit if mapping a user page */
        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
        
        /* Existing PDE — only this one PTE changed */
        paging_invalidate_tlb(virt_addr);
    }
    
    /* Set the page table entry */
    pt->entries[pt_idx] = entry_create(phys_addr, flags | PTE_PRESENT | PTE_ALLOCATED);
    
    return true;
}

bool page_map_existing(page_directory_t *pd, u32 virt_addr, u32 phys_addr, u16 flags) {
    if (!pd || !is_page_aligned(virt_addr) || !is_page_aligned(phys_addr)) {
        console_printf("[pme_fail] pd=%p v=%x p=%x\n", (void*)pd, virt_addr, phys_addr);
        return false;
    }

    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);

    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;

        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else if (pd != kernel_page_directory && !(pd->entries[pd_idx] & PTE_ALLOCATED)) {
        /* Shared kernel PDE in a user PD — allocate private page table */
        u32 old_pt_phys = entry_get_phys(pd->entries[pd_idx]);
        page_table_t *old_pt = ptable_ptr(old_pt_phys);

        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;

        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        pt = ptable_ptr( pt_phys);
        memcpy(pt, old_pt, sizeof(page_table_t));

        /* Full TLB flush — PDE topology changed for entire 4M window */
        paging_flush_tlb();
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);

        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
    }

    pt->entries[pt_idx] = entry_create(phys_addr, flags | PTE_PRESENT);
    
    /* Invalidate TLB for this page */
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

bool page_unmap(page_directory_t *pd, u32 virt_addr) {
    if (!pd || !is_page_aligned(virt_addr)) {
        return false;
    }
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr(pt_phys);
    u32 pte = pt->entries[pt_idx];

    if (!(pte & PTE_PRESENT)) {
        return false;
    }

    if (pte & PTE_ALLOCATED) {
        frame_free(entry_get_phys(pte));
    }
    pt->entries[pt_idx] = 0;
    paging_invalidate_tlb(virt_addr);

    return true;
}

u32 page_get_phys(page_directory_t *pd, u32 virt_addr) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return 0;
    }
    
    return entry_get_phys(pt->entries[pt_idx]) + page_offset(virt_addr);
}

bool page_is_present(page_directory_t *pd, u32 virt_addr) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    return (pt->entries[pt_idx] & PTE_PRESENT) != 0;
}

bool page_set_flags(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd) pd = page_directory_get_current();
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    /* Ensure PDE has user bit if setting user permission */
    if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
        pd->entries[pd_idx] |= PTE_USER;
    }
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pt->entries[pt_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 phys = entry_get_phys(pt->entries[pt_idx]);
    pt->entries[pt_idx] = entry_create(phys, flags | PTE_ALLOCATED);
    paging_invalidate_tlb(virt_addr);
    
    return true;
}

/*
 * Kernel Page Operations
 */

bool paging_map_kernel_range(page_directory_t *pd, u32 virt_start, 
                              u32 phys_start, size_t size, u16 flags) {
    if (!is_page_aligned(virt_start) || !is_page_aligned(phys_start)) {
        return false;
    }
    
    for (u32 offset = 0; offset < size; offset += PAGE_SIZE) {
        if (!page_map(pd, virt_start + offset, phys_start + offset, 
                      flags | PTE_GLOBAL)) {
            return false;
        }
    }
    return true;
}

void paging_map_kernel(page_directory_t *pd) {
    if (!pd) return;
    
    /* Copy all kernel directory entries */
    for (int i = KERNEL_PD_INDEX; i < ENTRIES_PER_PD; i++) {
        pd->entries[i] = kernel_page_directory->entries[i];
    }
}

/*
 * User Page Operations
 */

bool paging_alloc_user_page(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd) return false;
    
    /* Ensure address is in user space */
    if (virt_addr >= KERNEL_VIRTUAL_BASE) {
        return false;
    }
    
    /* Allocate physical frame */
    u32 phys = frame_alloc();
    if (!phys) return false;
    
    /* Map with user-accessible flag */
    if (!page_map(pd, virt_addr, phys, flags | PTE_USER)) {
        frame_free(phys);
        return false;
    }
    
    /* Zero the page to prevent information leaks and fix uninitialized
     * TLS data. Switch to the target PD so the virtual address resolves. */
    page_directory_t *old_pd = page_directory_get_current();
    if (old_pd != pd) {
        page_directory_switch(pd);
        paging_flush_tlb();
        memset((void *)(uintptr_t)virt_addr, 0, PAGE_SIZE);
        page_directory_switch(old_pd);
    } else {
        memset((void *)(uintptr_t)virt_addr, 0, PAGE_SIZE);
    }
    
    return true;
}

bool paging_free_user_page(page_directory_t *pd, u32 virt_addr) {
    if (!pd) return false;
    
    /* Ensure address is in user space */
    if (virt_addr >= KERNEL_VIRTUAL_BASE) {
        return false;
    }
    
    return page_unmap(pd, virt_addr);
}

/*
 * Copy-on-Write Page Copying
 */

bool paging_copy_page(page_directory_t *src_pd, page_directory_t *dst_pd, 
                       u32 virt_addr, bool cow) {
    if (!src_pd || !dst_pd) return false;
    if (virt_addr >= KERNEL_VIRTUAL_BASE) return false;
    
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    /* Check source page exists */
    if (!(src_pd->entries[pd_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 src_pt_phys = entry_get_phys(src_pd->entries[pd_idx]);
    page_table_t *src_pt = ptable_ptr( src_pt_phys);
    
    if (!(src_pt->entries[pt_idx] & PTE_PRESENT)) {
        return false;
    }
    
    u32 src_flags = entry_get_flags(src_pt->entries[pt_idx]);
    u32 src_phys = entry_get_phys(src_pt->entries[pt_idx]);
    
    if (cow) {
        /* Copy-on-write: share page read-only */
        
        /* Mark source as read-only if writable */
        if (src_flags & PTE_RW) {
            src_pt->entries[pt_idx] = entry_create(src_phys, 
                (src_flags & ~PTE_RW) | PTE_COW);
            paging_invalidate_tlb(virt_addr);
        }
        
        /* Map destination to same physical page */
        u32 dst_flags = src_flags | PTE_COW;
        if (!page_map(dst_pd, virt_addr, src_phys, dst_flags & ~PTE_RW)) {
            return false;
        }
    } else {
        /* Full copy: allocate new frame and copy data */
        u32 dst_phys = frame_alloc();
        if (!dst_phys) return false;
        
        /* Use identity mapping for direct physical memory access.
         * PDEs 0-767 (identity half) and the framebuffer at PDE 1012
         * are shared by all page directories.  KERNEL_VIRTUAL_BASE + phys
         * overflows for MMIO addresses above 1 GB (e.g. framebuffer at
         * 0xFD000000), so we use phys directly. */
        memcpy((void *)(uintptr_t)dst_phys,
               (void *)(uintptr_t)src_phys,
               PAGE_SIZE);
        
        /* Map in destination with original flags */
        if (!page_map(dst_pd, virt_addr, dst_phys, src_flags)) {
            frame_free(dst_phys);
            return false;
        }
    }
    
    return true;
}

/*
 * Demand Paging Support
 */

/* Set up a page for demand-zero allocation (anonymous) */
bool paging_setup_demand_anon(page_directory_t *pd, u32 virt_addr, u16 flags) {
    if (!pd || virt_addr >= KERNEL_VIRTUAL_BASE) return false;
    
    /* Just create the page table entry without allocating a frame */
    /* Mark as not present but with demand-zero flag */
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    /* Get or create page table */
    page_table_t *pt;
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        u32 pt_phys = frame_alloc();
        if (!pt_phys) return false;
        u16 pde_flags = PTE_PRESENT | PTE_RW | PTE_ALLOCATED;
        if (flags & PTE_USER) pde_flags |= PTE_USER;
        pd->entries[pd_idx] = entry_create(pt_phys, pde_flags);
        pt = ptable_ptr( pt_phys);
        memset(pt, 0, sizeof(page_table_t));
    } else {
        u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
        pt = ptable_ptr( pt_phys);
        
        if ((flags & PTE_USER) && !(pd->entries[pd_idx] & PTE_USER)) {
            pd->entries[pd_idx] |= PTE_USER;
        }
    }
    
    /* Set up as not-present with demand-zero flag */
    /* We store the desired flags in the entry, but mark as not present */
    pt->entries[pt_idx] = (flags | PTE_DEMAND_ZERO) & ~PTE_PRESENT;
    
    return true;
}

/* Handle demand-zero page fault - allocate and zero a page */
static bool handle_demand_anon(page_directory_t *pd, u32 virt_addr, bool for_write) {
    u32 pd_idx = virt_to_pd_index(virt_addr);
    u32 pt_idx = virt_to_pt_index(virt_addr);
    
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) return false;
    
    u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
    page_table_t *pt = ptable_ptr( pt_phys);
    
    u32 entry = pt->entries[pt_idx];
    if ((entry & PTE_PRESENT) || !(entry & PTE_DEMAND_ZERO)) return false;
    
    /* Extract desired flags */
    u16 desired_flags = entry_get_flags(entry) & ~PTE_DEMAND_ZERO;
    
    /* Allocate frame */
    u32 phys = frame_alloc();
    if (!phys) return false;
    
    /* Zero the page (demand-zero semantics) using higher-half mapping */
    memset((void *)(uintptr_t)(KERNEL_VIRTUAL_BASE + phys), 0, PAGE_SIZE);
    
    /* Update page table entry */
    pt->entries[pt_idx] = entry_create(phys, desired_flags | PTE_PRESENT | PTE_ALLOCATED);
    paging_invalidate_tlb(virt_addr);
    
    (void)for_write; /* for_write could adjust flags in future */
    return true;
}

/*
 * Page Fault Handler
 */

/* 
 * The God-forsaken Page Fault Handler.
 * If we're here, either the user screwed up, or the kernel is trying to do 
 * something clever (like COW or demand paging). Or, more likely, I've 
 * fundamentally misunderstood how x86 paging works today.
 */
void page_fault_handler(u32 virt_addr, u32 error_code, registers_t *regs) {
    bool present = error_code & PF_ERR_PRESENT;
    bool rw = error_code & PF_ERR_RW;
    bool user = error_code & PF_ERR_USER;
    bool reserved = error_code & PF_ERR_RESERVED;
    bool fetch = error_code & PF_ERR_FETCH;
    
    /* Pull the current PD. If this returns NULL, we're already dead. */
    page_directory_t *pd = page_directory_get_current();
    
    /* 
     * Copy-on-Write (COW) logic.
     * This is where we pretend we're efficient by sharing memory until someone 
     * actually tries to write to it. Then we scramble to allocate a new frame 
     * and copy the data before the CPU notices.
     */
    if (present && rw && !reserved) {
        u32 pd_idx = virt_to_pd_index(virt_addr);
        u32 pt_idx = virt_to_pt_index(virt_addr);
        
        if (pd->entries[pd_idx] & PTE_PRESENT) {
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr(pt_phys);
            
            if ((pt->entries[pt_idx] & PTE_PRESENT) && 
                (pt->entries[pt_idx] & PTE_COW)) {
                
                /* 
                 * The moment of truth. Someone wrote to a shared page.
                 * Allocate a fresh frame and get to copying.
                 */
                u32 old_phys = entry_get_phys(pt->entries[pt_idx]);
                u32 new_phys = frame_alloc();
                
                if (!new_phys) {
                    console_printf("[page_fault] Out of memory in COW handler. We are well and truly fucked.\n");
                    task_abort_from_trap(regs, "page fault (OOM)");
                    return;
                }
                
                /* 
                 * Access physical memory via the identity mapping.
                 * PDEs 0-767 and the framebuffer PDE 1012 are shared
                 * across all page directories.  KERNEL_VIRTUAL_BASE +
                 * phys overflows for MMIO addresses above 1 GB (e.g.
                 * framebuffer at 0xFD000000), so use phys directly.
                 */
                memcpy((void *)(uintptr_t)new_phys,
                       (void *)(uintptr_t)old_phys,
                       PAGE_SIZE);
                
                /* Upgrade the mapping to writable and mark it as ours. */
                u32 flags = entry_get_flags(pt->entries[pt_idx]);
                pt->entries[pt_idx] = entry_create(new_phys, 
                    (flags & ~PTE_COW) | PTE_RW | PTE_ALLOCATED);
                
                /* Tell the CPU to forget everything it knows about this address. */
                paging_invalidate_tlb(virt_addr);
                
                return;
            }
        }
    }
    
    /*
     * Legacy NULL page compatibility for programs like Qt6.
     * Some programs (notably Qt6 platform init) read from and write to
     * addresses 0x0000–0x0FFF (legacy IVT/BDA probe).  The kernel
     * identity-maps physical page 0 but without PTE_USER, so the first
     * user access faults.  We catch these faults and map the page with
     * full user read/write permission on demand.
     *
     * We map the REAL physical page 0 — the kernel does not rely on
     * legacy IVT/BDA data at runtime and uses its own GDT/IDT, so
     * allowing user writes to this page is safe on this system.
     *
     * IMPORTANT: The kernel is linked at 1 MB and runs identity-mapped
     * through PDE[0].  When we replace PDE[0] we must preserve ALL
     * existing kernel identity mappings (kernel code/data, etc.) —
     * otherwise the TLB will eventually evict the stale entries and
     * the next kernel instruction fetch blows up with a triple fault.
     */
    if (present && user && virt_addr < 0x1000) {
        console_printf("[page_fault] NULL page compat: va=%x pd=%p PDE=%x\n",
                       virt_addr, (void*)pd, pd->entries[0]);
        u32 null_pd_idx = virt_to_pd_index(virt_addr);
        if (!(pd->entries[null_pd_idx] & PTE_ALLOCATED)) {
            console_printf("[page_fault] NULL page: allocating private PT\n");
            u32 old_pt_phys = entry_get_phys(pd->entries[null_pd_idx]);
            page_table_t *old_pt = ptable_ptr(old_pt_phys);

            u32 new_pt_phys = frame_alloc();
            if (!new_pt_phys) {
                console_printf("[page_fault] NULL page: frame_alloc failed!\n");
                goto unhandled;
            }

            page_table_t *new_pt = ptable_ptr(new_pt_phys);
            memcpy(new_pt, old_pt, sizeof(page_table_t));

            /* Allow user read/write access to NULL page (physical page 0).
             * Don't set PTE_ALLOCATED — physical page 0 is not ours to free. */
            new_pt->entries[0] = entry_create(0, PTE_PRESENT | PTE_USER | PTE_RW);

            pd->entries[null_pd_idx] = entry_create(new_pt_phys,
                PTE_PRESENT | PTE_RW | PTE_ALLOCATED | PTE_USER);

            /* Full TLB flush — PDE[0] changed completely */
            console_printf("[page_fault] NULL page mapped (private PT, kernel mappings preserved)\n");
            paging_flush_tlb();
            return;
        }

        /* PDE[0] already has a private PT — ensure PTE[0] has user access.
         * This path handles the case where the eager mapping at PD creation
         * failed (OOM) or for PDs created before the fix was applied. */
        u32 pt_phys = entry_get_phys(pd->entries[null_pd_idx]);
        page_table_t *pt = ptable_ptr(pt_phys);
        if (!(pt->entries[0] & PTE_USER)) {
            pt->entries[0] = entry_create(0, PTE_PRESENT | PTE_USER | PTE_RW);
            paging_invalidate_tlb(virt_addr);
            console_printf("[page_fault] NULL page: PTE[0] upgraded to user\n");
        }
        return;
    }

    /*
     * User stack grows down through kernel identity-mapped pages.
     * When a user access hits a present-but-supervisor-only page
     * within the user stack range, allocate a fresh page and map
     * it with user permission so the stack can grow.
     *
     * IMPORTANT: If the PDE lacks PTE_ALLOCATED this is a SHARED
     * kernel identity-mapped page table.  We must create a private
     * copy (like the NULL-page fix does) so that fork's COW
     * mechanism handles these stack pages correctly.  Without this,
     * parent and child share the same physical stack pages and
     * scheduler interleaving corrupts gosh's stack.
     */
    if (present && user && !reserved && virt_addr >= USER_BASE &&
        virt_addr < USER_STACK_TOP) {
        u32 pd_idx = virt_to_pd_index(virt_addr);
        u32 pt_idx = virt_to_pt_index(virt_addr);

        if (pd->entries[pd_idx] & PTE_PRESENT) {
            if (!(pd->entries[pd_idx] & PTE_ALLOCATED)) {
                /* Shared kernel identity-mapped PDE — make a private copy */
                u32 old_pt_phys = entry_get_phys(pd->entries[pd_idx]);
                page_table_t *old_pt = ptable_ptr(old_pt_phys);

                u32 new_pt_phys = frame_alloc();
                if (!new_pt_phys) goto unhandled;
                page_table_t *new_pt = ptable_ptr(new_pt_phys);
                memcpy(new_pt, old_pt, sizeof(page_table_t));

                u32 new_phys = frame_alloc();
                if (!new_phys) { frame_free(new_pt_phys); goto unhandled; }
                new_pt->entries[pt_idx] = entry_create(new_phys,
                    PTE_PRESENT | PTE_RW | PTE_USER | PTE_ALLOCATED);

                pd->entries[pd_idx] = entry_create(new_pt_phys,
                    PTE_PRESENT | PTE_RW | PTE_ALLOCATED | PTE_USER);

                u32 page_va = virt_addr & PAGE_MASK;
                paging_flush_tlb();
                memset((void *)(uintptr_t)page_va, 0, PAGE_SIZE);
                return;
            }

            /* PDE already private — just replace the one PTE */
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr(pt_phys);
            u32 pte = pt->entries[pt_idx];

            if ((pte & PTE_PRESENT) && !(pte & PTE_USER)) {
                u32 new_phys = frame_alloc();
                if (new_phys) {
                    u32 page_va = virt_addr & PAGE_MASK;
                    pt->entries[pt_idx] = entry_create(new_phys,
                        PTE_PRESENT | PTE_RW | PTE_USER | PTE_ALLOCATED);
                    paging_invalidate_tlb(virt_addr);
                    memset((void *)(uintptr_t)page_va, 0, PAGE_SIZE);
                    return;
                }
            }
        }
    }

    /*
     * Identity-mapped kernel PTEs leaked into privatized page tables.
     * When a user PDE is privatized (ALLOCATED), the kernel identity
     * PTEs are memcpy'd into the new page table.  Any PTE that hasn't
     * been overwritten by a proper user mapping still carries the
     * original flags (PRESENT|RW|GLOBAL) without PTE_USER.  Catch
     * these and upgrade them.
     */
    if (present && user && virt_addr < KERNEL_VIRTUAL_BASE) {
        u32 pd_idx = virt_to_pd_index(virt_addr);
        u32 pt_idx = virt_to_pt_index(virt_addr);

        if ((pd->entries[pd_idx] & PTE_ALLOCATED) &&
            (pd->entries[pd_idx] & PTE_USER)) {
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr(pt_phys);
            u32 pte = pt->entries[pt_idx];

            if ((pte & PTE_PRESENT) && !(pte & PTE_USER)) {
                pt->entries[pt_idx] = entry_create(
                    entry_get_phys(pte),
                    (entry_get_flags(pte) & ~PTE_GLOBAL) | PTE_USER);
                paging_invalidate_tlb(virt_addr);
                return;
            }
        }
    }

unhandled:
    /* Handle demand-zero and swapped-out pages */
    if (!present && user && virt_addr < KERNEL_VIRTUAL_BASE) {
        u32 pd_idx = virt_to_pd_index(virt_addr);

        if (pd->entries[pd_idx] & PTE_PRESENT) {
            u32 pt_phys = entry_get_phys(pd->entries[pd_idx]);
            page_table_t *pt = ptable_ptr(pt_phys);
            u32 pt_idx = virt_to_pt_index(virt_addr);
            u32 pte = pt->entries[pt_idx];

            /* Demand-zero page */
            if ((pte & PTE_DEMAND_ZERO) && !(pte & PTE_PRESENT)) {
                if (handle_demand_anon(pd, virt_addr, rw)) {
                    return;
                }
            }

            /* Swapped-out page — read back from disk */
            if ((pte & PTE_SWAPPED) && !(pte & PTE_PRESENT)) {
                u32 slot = (pte >> 12) & 0xFFFFF;
                u16 orig_flags;
                u32 phys = frame_alloc();
                if (!phys) {
                    console_printf("[page_fault] OOM during swap-in\n");
                    goto unhandled_log;
                }

                if (!swap_in_page(slot, phys, &orig_flags)) {
                    frame_free(phys);
                    goto unhandled_log;
                }

                u16 restore_flags = orig_flags | PTE_PRESENT | PTE_ALLOCATED | PTE_ACCESSED;
                pt->entries[pt_idx] = entry_create(phys, restore_flags);
                paging_invalidate_tlb(virt_addr);
                return;
            }
        }

        /* Unmapped user address - this is a real fault */
        console_printf("[page_fault] unmapped user address %x\n", virt_addr);
    }

unhandled_log:
    
    /* Log the fault and kill the process */
    console_printf("[page_fault] %s at %x by %s (%s%s%s)\n",
                   present ? "protection violation" : "page not present",
                   virt_addr,
                   user ? "user" : "kernel",
                   rw ? "write " : "read ",
                   reserved ? "reserved-bit " : "",
                   fetch ? "fetch " : "");

    /* Dump PDE/PTE for debugging */
    {
        page_directory_t *dbg_pd = page_directory_get_current();
        u32 dbg_pd_idx = virt_to_pd_index(virt_addr);
        u32 dbg_pt_idx = virt_to_pt_index(virt_addr);
        console_printf("[page_fault] PD=%p idx=%u PDE=%x (phys=%x flags=%x)\n",
                       (void*)dbg_pd, dbg_pd_idx,
                       dbg_pd ? dbg_pd->entries[dbg_pd_idx] : 0,
                       dbg_pd ? entry_get_phys(dbg_pd->entries[dbg_pd_idx]) : 0,
                       dbg_pd ? entry_get_flags(dbg_pd->entries[dbg_pd_idx]) : 0);
        if (dbg_pd && (dbg_pd->entries[dbg_pd_idx] & PTE_PRESENT)) {
            u32 dbg_pt_phys = entry_get_phys(dbg_pd->entries[dbg_pd_idx]);
            page_table_t *dbg_pt = ptable_ptr(dbg_pt_phys);
            console_printf("[page_fault] PT phys=%x PTE[%u]=%x (phys=%x flags=%x)\n",
                           dbg_pt_phys, dbg_pt_idx,
                           dbg_pt->entries[dbg_pt_idx],
                           entry_get_phys(dbg_pt->entries[dbg_pt_idx]),
                           entry_get_flags(dbg_pt->entries[dbg_pt_idx]));
        }

        /* Dump instruction bytes at faulting EIP */
        if (regs) {
            u8 dbg_instr[16];
            u32 dbg_eip = regs->eip;
            for (int ii = 0; ii < 16; ii++) {
                if (page_is_present(NULL, dbg_eip + ii)) {
                    dbg_instr[ii] = *(volatile u8 *)(uintptr_t)(dbg_eip + ii);
                } else {
                    dbg_instr[ii] = 0;
                }
            }
            console_printf("[page_fault] instr at %x:", dbg_eip);
            for (int ii = 0; ii < 16; ii++) {
                console_printf(" %02x", dbg_instr[ii]);
            }
            console_printf("\n");
            console_printf("[page_fault] user_eax=%x ecx=%x edx=%x ebx=%x esi=%x edi=%x ebp=%x\n",
                           regs->eax, regs->ecx, regs->edx, regs->ebx,
                           regs->esi, regs->edi, regs->ebp);
        }
    }
    
    if (user && task_is_active()) {
        task_abort_from_trap(regs, "page fault");
    } else {
        panic("Kernel page fault at %x", virt_addr);
    }
}

/*
 * Initialization
 */

static u32 multiboot_modules_end(const multiboot_info_t *mbi) {
    u32 mods_end = 0;

    if (!mbi || !(mbi->flags & MULTIBOOT_FLAG_MODS) || mbi->mods_count == 0) {
        return mods_end;
    }

    const multiboot_module_t *mods = (const multiboot_module_t *)(uintptr_t)mbi->mods_addr;
    for (u32 i = 0; i < mbi->mods_count; ++i) {
        if (mods[i].mod_end > mods_end) {
            mods_end = mods[i].mod_end;
        }
    }
    return mods_end;
}

/* Parse multiboot memory map to find best region for user frames */
static bool find_best_memory_region(const multiboot_info_t *mbi, 
                                     u32 *out_start, u32 *out_end, 
                                     u32 kernel_end_phys) {
    u32 best_start = 0;
    u32 best_size = 0;
    
    if (!(mbi->flags & MULTIBOOT_FLAG_MMAP)) {
        /* Fallback to simple calculation */
        *out_start = (kernel_end_phys + 0xFFFFFF) & ~0xFFFFFF; /* Align to 16MB */
        if (*out_start < 0x1000000) *out_start = 0x1000000; /* Min 16MB */
        /* Use memory info from multiboot if available */
        if (mbi->flags & MULTIBOOT_FLAG_MEM) {
            *out_end = (mbi->mem_lower + mbi->mem_upper) * 1024;
        } else {
            *out_end = kernel_end_phys + 0x10000000; /* Assume 256MB */
        }
        return true;
    }
    
    /* Walk memory map to find largest available region above kernel */
    u32 mmap_addr = mbi->mmap_addr;
    u32 mmap_end = mbi->mmap_addr + mbi->mmap_length;
    
    while (mmap_addr < mmap_end) {
        multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)(uintptr_t)mmap_addr;
        
        if (entry->type == 1 && entry->addr_high == 0) {
            u32 region_start = entry->addr_low;
            u32 region_len = entry->len_low;
            u32 region_end = region_start + region_len;
            
            /* Skip low memory (below 1MB) */
            if (region_end <= 0x100000) {
                mmap_addr += entry->size + sizeof(entry->size);
                continue;
            }
            
            /* Skip region if it overlaps with kernel */
            if (region_start < kernel_end_phys && region_end > 0x100000) {
                region_start = kernel_end_phys;
            }
            
            /* Align start to page boundary */
            region_start = (region_start + PAGE_SIZE - 1) & PAGE_MASK;
            
            if (region_start < region_end) {
                u32 size = region_end - region_start;
                if (size > best_size) {
                    best_start = region_start;
                    best_size = size;
                }
            }
        }
        
        mmap_addr += entry->size + sizeof(entry->size);
    }
    
    if (best_size >= 4 * 1024 * 1024) { /* At least 4MB */
        *out_start = best_start;
        *out_end = best_start + best_size;
        return true;
    }
    
    /* Fallback */
    *out_start = 0x10000000; /* 256MB */
    *out_end = kernel_end_phys + 0x10000000; /* Assume 256MB available after kernel */
    if (*out_end < 0x2000000) *out_end = 0x2000000; /* At least 32MB */
    return true;
}

void paging_init(const multiboot_info_t *mbi, u32 total_memory_bytes, u32 kernel_end_phys) {
    console_write("[paging] initializing...\n");
    
    /* 
     * Kick off the paging nightmare.
     * We're about to slice up memory and hope the hardware actually honors 
     * our mappings. Spoiler: it often doesn't on real hardware.
     */
    u32 user_mem_start, user_mem_end;
    find_best_memory_region(mbi, &user_mem_start, &user_mem_end, kernel_end_phys);

    /* 
     * Don't touch the initrd module. 
     * If we start allocating user frames from the initrd, we'll overwrite 
     * the system binaries before we even run them. That would be suboptimal.
     */
    {
        u32 mods_end = multiboot_modules_end(mbi);
        if (mods_end > 0) {
            u32 after_mods = (mods_end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
            if (user_mem_start < after_mods) {
                user_mem_start = after_mods;
            }
        }
    }

    /* 
     * Shield the kernel heap. 
     * The frame allocator is greedy and will gobble up everything it sees. 
     * We have to force it to start after the kernel heap so we don't 
     * double-allocate physical frames. Double-allocation leads to silent 
     * data corruption and long nights of debugging.
     */
    {
        u32 heap_end = memory_heap_end();
        if (heap_end > 0 && user_mem_start < heap_end) {
            user_mem_start = (heap_end + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
            if (user_mem_start >= user_mem_end) {
                panic("Frame allocator region exhausted by kernel heap. Buy more RAM.");
            }
        }
    }

    /* 
     * Initialize the physical frame allocator.
     */
    if (!frame_alloc_init(user_mem_start, user_mem_end, 0)) {
        panic("Failed to initialize frame allocator");
    }

    /* 
     * Create the kernel page directory using physical addresses directly,
     * since paging isn't enabled yet (KERNEL_VIRTUAL_BASE won't work).
     */
    u32 pd_phys = frame_alloc();
    if (!pd_phys) panic("Failed to allocate kernel page directory");
    page_directory_t *pd = (page_directory_t *)(uintptr_t)pd_phys;
    memset(pd, 0, sizeof(page_directory_t));

    /* Set up identity + higher-half mapping for the lower 896MB (0 .. 0x38000000).
     * Each page table covers 4MB. The same page table is shared between the
     * identity PDE (i) and the higher-half PDE (KERNEL_PD_INDEX + i) so that
     * both virtual 0 and virtual 3GB map to the same physical pages.
     */
    u32 num_4mb = 0x38000000 / 0x400000;
    for (u32 pde = 0; pde < num_4mb; pde++) {
        u32 pt_phys = frame_alloc();
        if (!pt_phys) panic("Failed to allocate page table");
        page_table_t *pt = (page_table_t *)(uintptr_t)pt_phys;
        memset(pt, 0, sizeof(page_table_t));

        u32 phys_base = pde * 0x400000;
        for (u32 pte = 0; pte < 1024; pte++) {
            pt->entries[pte] = (phys_base + pte * PAGE_SIZE) |
                               PTE_PRESENT | PTE_RW | PTE_GLOBAL;
        }

        pd->entries[pde] = pt_phys | PTE_PRESENT | PTE_RW;
        pd->entries[KERNEL_PD_INDEX + pde] = pt_phys | PTE_PRESENT | PTE_RW;
    }

    /*
     * Identity-map the framebuffer so console writes don't page-fault.
     *
     * IMPORTANT: We ALWAYS allocate a new page table for the framebuffer PDE,
     * even if the PDE already exists from the identity+higher-half mapping loop
     * above. The existing PDE may be shared with an identity-mapped PDE (via
     * KERNEL_PD_INDEX + pde), and modifying its PTEs would corrupt the identity
     * mapping for that 4MB window. A dedicated page table avoids this.
     */
    if (mbi->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
        u64 fb_phys = mbi->framebuffer_addr;
        if (fb_phys != 0 && (fb_phys >> 32) == 0) {
            u32 fb_phys32 = (u32)fb_phys;
            u32 fb_size = mbi->framebuffer_pitch * mbi->framebuffer_height;
            if (fb_size > 0) {
                fb_size = (fb_size + PAGE_SIZE - 1u) & ~(PAGE_SIZE - 1u);
                u32 last_fb_pde = 0xFFFFFFFF;
                u32 fb_pt_phys = 0;
                for (u32 offset = 0; offset < fb_size; offset += PAGE_SIZE) {
                    u32 phys = fb_phys32 + offset;
                    u32 pde_idx = phys >> 22;
                    u32 pte_idx = (phys >> 12) & 0x3FF;

                    if (pde_idx != last_fb_pde) {
                        fb_pt_phys = frame_alloc();
                        if (!fb_pt_phys) panic("Failed to allocate FB page table");
                        page_table_t *fb_pt = (page_table_t *)(uintptr_t)fb_pt_phys;
                        memset(fb_pt, 0, sizeof(page_table_t));
                        pd->entries[pde_idx] = fb_pt_phys | PTE_PRESENT | PTE_RW;
                        last_fb_pde = pde_idx;
                    }

                    page_table_t *pt = (page_table_t *)(uintptr_t)fb_pt_phys;
                    pt->entries[pte_idx] = phys | PTE_PRESENT | PTE_RW;
                }
            }
        }
    }

    /* Store kernel page directory and enable paging */
    kernel_page_directory = pd;
    kernel_page_directory_phys = pd_phys;
    paging_set_cr3(pd_phys);
    paging_enable();
    paging_active = true;
    
    console_printf("[paging] kernel page directory at %x (phys %x)\n",
                   (u32)kernel_page_directory, kernel_page_directory_phys);
    console_printf("[paging] PDEs 0-15:");
    for (int i = 0; i < 16; i++) {
        console_printf(" %x", kernel_page_directory->entries[i]);
    }
    console_write("\n");
    console_printf("[paging] ready - %u frames total, %u free\n", 
                   frame_total(), frame_free_count());
}

/* Helper to get kernel page directory for copying */
page_directory_t *paging_get_kernel_pd(void) {
    return kernel_page_directory;
}

bool paging_memory_pressure(void) {
    u32 free = frame_free_count();
    u32 total = frame_total();
    if (total == 0) return true;
    return (free < total / 20) || (free < 32);
}

u32 paging_available_memory(void) {
    return frame_free_count() * (PAGE_SIZE / 1024);
}

void paging_evict_pages(u32 target_kb) {
    u32 freed = 0;
    u32 target = (target_kb + 3) / 4;
    while (freed < target) {
        if (!swap_try_evict()) break;
        freed++;
    }
}
