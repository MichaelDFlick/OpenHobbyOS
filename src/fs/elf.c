#include "elf.h"

#include "memory.h"
#include "paging.h"
#include "string.h"
#include "vfs.h"

typedef struct PACKED {
    u8 ident[16];
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
} elf_header_t;

typedef struct PACKED {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
} elf_program_header_t;

static u32 align_up(u32 value, u32 alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

bool elf_load_image(const initrd_file_t *file, u32 user_base, u32 user_limit, elf_image_t *image) {
    const elf_header_t *header;
    u32 load_min = 0xFFFFFFFF;
    u32 load_max = 0;

    (void)user_base;
    (void)user_limit;

    if (!file || file->size < sizeof(elf_header_t) || !image) {
        return false;
    }

    header = (const elf_header_t *)file->data;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' || header->ident[2] != 'L' || header->ident[3] != 'F') {
        return false;
    }
    if (header->ident[4] != 1 || header->ident[5] != 1) {
        return false;
    }
    if (header->machine != 3 || header->version != 1) {
        return false;
    }
    if (header->type != 2 && header->type != 3) {
        return false;
    }
    if (header->phoff + header->phnum * sizeof(elf_program_header_t) > file->size) {
        return false;
    }

    image->interp_path[0] = '\0';
    image->phdr_vaddr = 0;
    image->phnum = header->phnum;

    for (u16 i = 0; i < header->phnum; ++i) {
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(file->data + header->phoff + i * sizeof(elf_program_header_t));

        if (ph->type == 3) {
            if (ph->filesz > 0 && ph->filesz < sizeof(image->interp_path)) {
                const char *src = (const char *)(file->data + ph->offset);
                memcpy(image->interp_path, src, ph->filesz);
                image->interp_path[ph->filesz] = '\0';
            }
            continue;
        }
        if (ph->type == 6) {
            image->phdr_vaddr = ph->vaddr;
            continue;
        }
        if (ph->type == 7) { /* PT_TLS */
            image->tls_memsz = ph->memsz;
            image->tls_filesz = ph->filesz;
            image->tls_vaddr = ph->vaddr;
            continue;
        }
        if (ph->type != 1) {
            continue;
        }
        if (ph->offset + ph->filesz > file->size) {
            return false;
        }
        if (ph->vaddr + ph->memsz < ph->vaddr) {
            return false;
        }

        /* Load at ELF-specified address (must be above 1MB to avoid kernel) */
        if (ph->vaddr < 0x100000) {
            return false;
        }

        /* Fixed-address loading keeps the ABI surface real without pretending we already have paging. */
        memset((void *)(uintptr_t)ph->vaddr, 0, ph->memsz);
        memcpy((void *)(uintptr_t)ph->vaddr, file->data + ph->offset, ph->filesz);

        if (ph->vaddr < load_min) {
            load_min = ph->vaddr;
        }
        if (ph->vaddr + ph->memsz > load_max) {
            load_max = ph->vaddr + ph->memsz;
        }
    }

    if (header->entry < load_min || header->entry >= load_max) {
        return false;
    }

    image->entry = header->entry;
    image->image_end = align_up(load_max, 4096);
    return true;
}

bool elf_load_vfs_node(const vfs_node_t *node, page_directory_t *pd,
                       u32 user_base, u32 user_limit, elf_image_t *image) {
    const initrd_file_t *backing;
    const u8 *bytes = NULL;
    u32 size = 0;
    u8 *owned_bytes = NULL;
    const elf_header_t *header;
    u32 load_min = 0xFFFFFFFF;
    u32 load_max = 0;

    if (!node || !pd || !image) {
        return false;
    }

    memset(image, 0, sizeof(*image));

    backing = vfs_backing_file(node);
    if (backing) {
        bytes = backing->data;
        size = backing->size;
    }
    if (!bytes) {
        if (vfs_is_dir(node)) {
            return false;
        }

        size = vfs_file_size(node);
        if (size == 0 || size > user_limit - user_base) {
            return false;
        }

        owned_bytes = kmalloc(size);
        if (!owned_bytes) {
            return false;
        }

        if (vfs_read(node, 0, owned_bytes, size) != (ssize_t)size) {
            kfree(owned_bytes);
            return false;
        }
        bytes = owned_bytes;
    }

    if (!bytes || size < sizeof(elf_header_t)) {
        kfree(owned_bytes);
        return false;
    }

    header = (const elf_header_t *)bytes;
    if (header->ident[0] != 0x7F || header->ident[1] != 'E' || header->ident[2] != 'L' || header->ident[3] != 'F') {
        kfree(owned_bytes);
        return false;
    }
    if (header->ident[4] != 1 || header->ident[5] != 1) {
        kfree(owned_bytes);
        return false;
    }
    if (header->machine != 3 || header->version != 1) {
        kfree(owned_bytes);
        return false;
    }
    if (header->type != 2 && header->type != 3) {
        kfree(owned_bytes);
        return false;
    }
    if (header->phoff + header->phnum * sizeof(elf_program_header_t) > size) {
        kfree(owned_bytes);
        return false;
    }

    image->interp_path[0] = '\0';
    image->phdr_vaddr = 0;
    image->phnum = header->phnum;

    for (u16 i = 0; i < header->phnum; ++i) {
        const elf_program_header_t *ph =
            (const elf_program_header_t *)(bytes + header->phoff + i * sizeof(elf_program_header_t));

        if (ph->type == 3) {
            if (ph->filesz > 0 && ph->filesz < sizeof(image->interp_path)) {
                const char *src = (const char *)(bytes + ph->offset);
                memcpy(image->interp_path, src, ph->filesz);
                image->interp_path[ph->filesz] = '\0';
            }
            continue;
        }
        if (ph->type == 6) {
            image->phdr_vaddr = ph->vaddr;
            continue;
        }
        if (ph->type == 7) { /* PT_TLS */
            image->tls_memsz = ph->memsz;
            image->tls_filesz = ph->filesz;
            image->tls_vaddr = ph->vaddr;
            continue;
        }
        if (ph->type != 1) {
            continue;
        }
        if (ph->offset + ph->filesz > size) {
            kfree(owned_bytes);
            return false;
        }
        if (ph->vaddr + ph->memsz < ph->vaddr) {
            kfree(owned_bytes);
            return false;
        }
        if (ph->vaddr < user_base || ph->vaddr + ph->memsz > user_limit) {
            kfree(owned_bytes);
            return false;
        }
        if (ph->vaddr < 0x100000) {
            kfree(owned_bytes);
            return false;
        }

        {
            u32 vaddr_start = ph->vaddr & ~0xFFFu;
            u32 vaddr_end = align_up(ph->vaddr + ph->memsz, 4096u);
            page_directory_t *old_pd = page_directory_get_current();

            for (u32 va = vaddr_start; va < vaddr_end; va += 4096u) {
                if (!paging_alloc_user_page(pd, va, PTE_RW | PTE_USER)) {
                    kfree(owned_bytes);
                    return false;
                }
            }

            page_directory_switch(pd);
            paging_flush_tlb();
            memset((void *)(uintptr_t)ph->vaddr, 0, ph->memsz);
            memcpy((void *)(uintptr_t)ph->vaddr, bytes + ph->offset, ph->filesz);
            page_directory_switch(old_pd);
        }

        if (ph->vaddr < load_min) {
            load_min = ph->vaddr;
        }
        if (ph->vaddr + ph->memsz > load_max) {
            load_max = ph->vaddr + ph->memsz;
        }
    }

    if (header->entry < load_min || header->entry >= load_max) {
        kfree(owned_bytes);
        return false;
    }

    image->entry = header->entry;
    image->image_end = align_up(load_max, 4096u);

    kfree(owned_bytes);
    return true;
}
