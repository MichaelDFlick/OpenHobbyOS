#include "ext2.h"

#include "blkdev.h"
#include "console.h"
#include "memory.h"
#include "panic.h"
#include "string.h"

#define EXT2_BLOCK_SIZE(fs) (1024 << (fs)->sb.s_log_block_size)
#define EXT2_INODES_PER_BLOCK(fs) ((fs)->block_size / (fs)->sb.s_inode_size)
#define EXT2_SECTOR_FROM_BLOCK(fs, block) ((fs)->partition_start + (block) * (fs)->sectors_per_block)

bool ext2_read_block(struct ext2_fs *fs, u32 block_num, void *buffer) {
    u32 lba = EXT2_SECTOR_FROM_BLOCK(fs, block_num);
    u32 sectors = fs->sectors_per_block;
    
    if (blkdev_read(fs->blkdev_id, lba, sectors, buffer) != 0) {
        return false;
    }
    return true;
}

bool ext2_is_valid(u32 blkdev_id, u32 partition_start) {
    if (!blkdev_present(blkdev_id)) {
        return false;
    }
    
    static struct ext2_superblock sb;
    u32 sb_offset = partition_start + 2;
    
    if (blkdev_read(blkdev_id, sb_offset, 2, &sb) != 0) {
        return false;
    }
    
    return sb.s_magic == EXT2_MAGIC;
}

bool ext2_mount(u32 blkdev_id, u32 partition_start, struct ext2_fs *fs) {
    memset(fs, 0, sizeof(struct ext2_fs));
    
    fs->blkdev_id = blkdev_id;
    fs->partition_start = partition_start;
    
    u32 sb_offset = partition_start + 2;
    if (blkdev_read(blkdev_id, sb_offset, 2, &fs->sb) != 0) {
        return false;
    }
    
    if (fs->sb.s_magic != EXT2_MAGIC) {
        return false;
    }
    
    fs->block_size = EXT2_BLOCK_SIZE(fs);
    fs->sectors_per_block = fs->block_size / BLKDEV_SECTOR_SIZE;
    fs->inodes_per_block = EXT2_INODES_PER_BLOCK(fs);
    
    fs->groups_count = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;
    
    u32 bgdt_block = (fs->sb.s_first_data_block == 0) ? 2 : 1;
    if (fs->block_size == 1024) {
        bgdt_block = 2;
    }
    
    u32 bgdt_size = fs->groups_count * sizeof(struct ext2_block_group_desc);
    u32 bgdt_blocks = (bgdt_size + fs->block_size - 1) / fs->block_size;
    
    fs->bgdt = kmalloc(bgdt_blocks * fs->block_size);
    if (!fs->bgdt) {
        return false;
    }
    
    for (u32 i = 0; i < bgdt_blocks; i++) {
        if (!ext2_read_block(fs, bgdt_block + i, (u8 *)fs->bgdt + i * fs->block_size)) {
            kfree(fs->bgdt);
            return false;
        }
    }
    
    fs->mounted = true;
    
    console_printf("[ext2] mounted: %u blocks, %u inodes, %u block size\n",
                   fs->sb.s_blocks_count, fs->sb.s_inodes_count, fs->block_size);
    
    return true;
}

void ext2_unmount(struct ext2_fs *fs) {
    if (fs->bgdt) {
        kfree(fs->bgdt);
        fs->bgdt = NULL;
    }
    fs->mounted = false;
}

bool ext2_read_inode(struct ext2_fs *fs, u32 inode_num, struct ext2_inode *inode) {
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) {
        return false;
    }
    
    u32 group = (inode_num - 1) / fs->sb.s_inodes_per_group;
    u32 index = (inode_num - 1) % fs->sb.s_inodes_per_group;
    
    if (group >= fs->groups_count) {
        return false;
    }
    
    u32 inode_table_block = fs->bgdt[group].bg_inode_table;
    u32 block_offset = index / fs->inodes_per_block;
    u32 inode_offset = index % fs->inodes_per_block;
    
    u8 *block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) {
        return false;
    }
    
    if (!ext2_read_block(fs, inode_table_block + block_offset, block_buffer)) {
        kfree(block_buffer);
        return false;
    }
    
    struct ext2_inode *inode_table = (struct ext2_inode *)block_buffer;
    memcpy(inode, &inode_table[inode_offset], sizeof(struct ext2_inode));
    
    kfree(block_buffer);
    return true;
}

bool ext2_read_inode_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx, void *buffer) {
    u32 block_num = 0;
    
    if (block_idx < 12) {
        block_num = inode->i_block[block_idx];
    } else if (block_idx < 12 + 256) {
        u32 indirect_idx = block_idx - 12;
        u8 *indirect_buf = kmalloc(fs->block_size);
        if (!indirect_buf) return false;
        
        if (!ext2_read_block(fs, inode->i_block[12], indirect_buf)) {
            kfree(indirect_buf);
            return false;
        }
        
        u32 *indirect = (u32 *)indirect_buf;
        block_num = indirect[indirect_idx];
        kfree(indirect_buf);
    } else {
        return false;
    }
    
    if (block_num == 0) {
        memset(buffer, 0, fs->block_size);
        return true;
    }
    
    return ext2_read_block(fs, block_num, buffer);
}

u32 ext2_dir_lookup(struct ext2_fs *fs, struct ext2_inode *dir_inode, const char *name) {
    u8 *block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) {
        return 0;
    }
    
    u32 num_blocks = (dir_inode->i_size + fs->block_size - 1) / fs->block_size;
    
    for (u32 i = 0; i < num_blocks; i++) {
        if (!ext2_read_inode_block(fs, dir_inode, i, block_buffer)) {
            continue;
        }
        
        u32 offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_buffer + offset);
            
            if (entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }
            
            char entry_name[EXT2_NAME_LEN + 1];
            u32 name_len = entry->name_len;
            if (name_len > EXT2_NAME_LEN) {
                name_len = EXT2_NAME_LEN;
            }
            memcpy(entry_name, entry->name, name_len);
            entry_name[name_len] = '\0';
            
            if (strcmp(entry_name, name) == 0) {
                u32 result = entry->inode;
                kfree(block_buffer);
                return result;
            }
            
            if (entry->rec_len == 0) {
                break;
            }
            offset += entry->rec_len;
        }
    }
    
    kfree(block_buffer);
    return 0;
}

u32 ext2_find_inode(struct ext2_fs *fs, const char *path) {
    if (!fs->mounted) {
        return 0;
    }
    
    if (path[0] != '/') {
        return 0;
    }
    
    if (path[1] == '\0') {
        return 2;
    }
    
    u32 current_inode = 2;
    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';
    
    char *token = strtok(path_copy + 1, "/");
    struct ext2_inode inode;
    
    while (token != NULL) {
        if (!ext2_read_inode(fs, current_inode, &inode)) {
            return 0;
        }
        
        if (!ext2_is_dir(&inode)) {
            return 0;
        }
        
        current_inode = ext2_dir_lookup(fs, &inode, token);
        if (current_inode == 0) {
            return 0;
        }
        
        token = strtok(NULL, "/");
    }
    
    return current_inode;
}

bool ext2_opendir(struct ext2_fs *fs, u32 inode_num, ext2_file_t *dir) {
    if (!ext2_read_inode(fs, inode_num, &dir->inode)) {
        return false;
    }
    
    if (!ext2_is_dir(&dir->inode)) {
        return false;
    }
    
    dir->fs = fs;
    dir->inode_num = inode_num;
    dir->offset = 0;
    dir->current_block = 0xFFFFFFFF;
    dir->block_buffer = kmalloc(fs->block_size);
    
    return dir->block_buffer != NULL;
}

bool ext2_readdir(ext2_file_t *dir, struct ext2_dir_entry *entry, char *name_buf) {
    struct ext2_fs *fs = dir->fs;
    
    if (!fs || !fs->mounted) {
        return false;
    }
    
    u32 block_size = fs->block_size;
    u32 num_blocks = (dir->inode.i_size + block_size - 1) / block_size;
    
    while (dir->offset < dir->inode.i_size) {
        u32 block_idx = dir->offset / block_size;
        u32 block_offset = dir->offset % block_size;
        
        if (block_idx >= num_blocks) {
            return false;
        }
        
        if (block_idx != dir->current_block) {
            if (!ext2_read_inode_block(fs, &dir->inode, block_idx, dir->block_buffer)) {
                return false;
            }
            dir->current_block = block_idx;
        }
        
        struct ext2_dir_entry *de = (struct ext2_dir_entry *)(dir->block_buffer + block_offset);
        
        if (de->inode != 0) {
            memcpy(entry, de, sizeof(struct ext2_dir_entry));
            
            u32 name_len = de->name_len;
            if (name_len > EXT2_NAME_LEN) {
                name_len = EXT2_NAME_LEN;
            }
            memcpy(name_buf, de->name, name_len);
            name_buf[name_len] = '\0';
            
            dir->offset += de->rec_len;
            return true;
        }
        
        if (de->rec_len == 0) {
            return false;
        }
        
        dir->offset += de->rec_len;
    }
    
    return false;
}

void ext2_closedir(ext2_file_t *dir) {
    if (dir->block_buffer) {
        kfree(dir->block_buffer);
        dir->block_buffer = NULL;
    }
}

bool ext2_open(struct ext2_fs *fs, u32 inode_num, ext2_file_t *file) {
    if (!ext2_read_inode(fs, inode_num, &file->inode)) {
        return false;
    }
    
    file->fs = fs;
    file->inode_num = inode_num;
    file->offset = 0;
    file->current_block = 0xFFFFFFFF;
    file->block_buffer = kmalloc(fs->block_size);
    
    return file->block_buffer != NULL;
}

ssize_t ext2_read(ext2_file_t *file, void *buffer, size_t size) {
    struct ext2_fs *fs = file->fs;
    
    if (!fs || !fs->mounted) {
        return -1;
    }
    
    u32 file_size = file->inode.i_size;
    if (file->offset >= file_size) {
        return 0;
    }
    
    if (file->offset + size > file_size) {
        size = file_size - file->offset;
    }
    
    u8 *buf = buffer;
    size_t total_read = 0;
    u32 block_size = fs->block_size;
    
    while (size > 0 && file->offset < file_size) {
        u32 block_idx = file->offset / block_size;
        u32 block_offset = file->offset % block_size;
        u32 to_read = block_size - block_offset;
        
        if (to_read > size) {
            to_read = size;
        }
        
        if (file->offset + to_read > file_size) {
            to_read = file_size - file->offset;
        }
        
        if (block_idx != file->current_block) {
            if (!ext2_read_inode_block(fs, &file->inode, block_idx, file->block_buffer)) {
                break;
            }
            file->current_block = block_idx;
        }
        
        memcpy(buf, file->block_buffer + block_offset, to_read);
        
        buf += to_read;
        file->offset += to_read;
        total_read += to_read;
        size -= to_read;
    }
    
    return total_read;
}

void ext2_close(ext2_file_t *file) {
    if (file->block_buffer) {
        kfree(file->block_buffer);
        file->block_buffer = NULL;
    }
}

u32 ext2_file_size(struct ext2_inode *inode) {
    return inode->i_size;
}

bool ext2_is_dir(struct ext2_inode *inode) {
    return (inode->i_mode & 0xF000) == EXT2_S_IFDIR;
}

bool ext2_is_reg(struct ext2_inode *inode) {
    return (inode->i_mode & 0xF000) == EXT2_S_IFREG;
}

bool ext2_write_block(struct ext2_fs *fs, u32 block_num, const void *buffer) {
    u32 lba = EXT2_SECTOR_FROM_BLOCK(fs, block_num);
    u32 sectors = fs->sectors_per_block;
    return blkdev_write(fs->blkdev_id, lba, sectors, buffer) == 0;
}

bool ext2_write_inode(struct ext2_fs *fs, u32 inode_num, struct ext2_inode *inode) {
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) return false;

    u32 group = (inode_num - 1) / fs->sb.s_inodes_per_group;
    u32 index = (inode_num - 1) % fs->sb.s_inodes_per_group;
    if (group >= fs->groups_count) return false;

    u32 inode_table_block = fs->bgdt[group].bg_inode_table;
    u32 block_offset = index / fs->inodes_per_block;
    u32 inode_offset = index % fs->inodes_per_block;

    u8 *block_buffer = kmalloc(fs->block_size);
    if (!block_buffer) return false;

    bool ok = false;
    if (ext2_read_block(fs, inode_table_block + block_offset, block_buffer)) {
        memcpy(((struct ext2_inode *)block_buffer) + inode_offset, inode, sizeof(struct ext2_inode));
        ok = ext2_write_block(fs, inode_table_block + block_offset, block_buffer);
    }

    kfree(block_buffer);
    return ok;
}

u32 ext2_alloc_block(struct ext2_fs *fs) {
    for (u32 g = 0; g < fs->groups_count; g++) {
        if (fs->bgdt[g].bg_free_blocks_count == 0) continue;

        u32 bitmap_block = fs->bgdt[g].bg_block_bitmap;
        u8 *bitmap = kmalloc(fs->block_size);
        if (!bitmap) return 0;

        u32 block_num = 0;
        if (ext2_read_block(fs, bitmap_block, bitmap)) {
            u32 blocks_per_group = fs->sb.s_blocks_per_group;
            for (u32 i = 0; i < blocks_per_group && i < fs->block_size * 8; i++) {
                u32 byte_idx = i / 8;
                u32 bit_idx = i % 8;
                if (!(bitmap[byte_idx] & (1u << bit_idx))) {
                    bitmap[byte_idx] |= (1u << bit_idx);
                    if (ext2_write_block(fs, bitmap_block, bitmap)) {
                        block_num = g * blocks_per_group + i;
                        fs->bgdt[g].bg_free_blocks_count--;
                        fs->sb.s_free_blocks_count--;
                    }
                    break;
                }
            }
        }
        kfree(bitmap);
        if (block_num) return block_num;
    }
    return 0;
}

u32 ext2_alloc_inode(struct ext2_fs *fs) {
    for (u32 g = 0; g < fs->groups_count; g++) {
        if (fs->bgdt[g].bg_free_inodes_count == 0) continue;

        u32 bitmap_block = fs->bgdt[g].bg_inode_bitmap;
        u8 *bitmap = kmalloc(fs->block_size);
        if (!bitmap) return 0;

        u32 inode_num = 0;
        if (ext2_read_block(fs, bitmap_block, bitmap)) {
            u32 inodes_per_group = fs->sb.s_inodes_per_group;
            for (u32 i = 0; i < inodes_per_group && i < fs->block_size * 8; i++) {
                u32 byte_idx = i / 8;
                u32 bit_idx = i % 8;
                if (!(bitmap[byte_idx] & (1u << bit_idx))) {
                    bitmap[byte_idx] |= (1u << bit_idx);
                    if (ext2_write_block(fs, bitmap_block, bitmap)) {
                        inode_num = g * inodes_per_group + i + 1;
                        fs->bgdt[g].bg_free_inodes_count--;
                        fs->sb.s_free_inodes_count--;
                    }
                    break;
                }
            }
        }
        kfree(bitmap);
        if (inode_num) return inode_num;
    }
    return 0;
}

bool ext2_free_block(struct ext2_fs *fs, u32 block_num) {
    u32 blocks_per_group = fs->sb.s_blocks_per_group;
    u32 g = block_num / blocks_per_group;
    u32 idx = block_num % blocks_per_group;
    if (g >= fs->groups_count) return false;

    u32 bitmap_block = fs->bgdt[g].bg_block_bitmap;
    u8 *bitmap = kmalloc(fs->block_size);
    if (!bitmap) return false;

    bool ok = false;
    if (ext2_read_block(fs, bitmap_block, bitmap)) {
        u32 byte_idx = idx / 8;
        u32 bit_idx = idx % 8;
        bitmap[byte_idx] &= ~(1u << bit_idx);
        if (ext2_write_block(fs, bitmap_block, bitmap)) {
            fs->bgdt[g].bg_free_blocks_count++;
            fs->sb.s_free_blocks_count++;
            ok = true;
        }
    }
    kfree(bitmap);
    return ok;
}

bool ext2_free_inode(struct ext2_fs *fs, u32 inode_num) {
    if (inode_num == 0 || inode_num > fs->sb.s_inodes_count) return false;
    u32 inodes_per_group = fs->sb.s_inodes_per_group;
    u32 g = (inode_num - 1) / inodes_per_group;
    u32 idx = (inode_num - 1) % inodes_per_group;
    if (g >= fs->groups_count) return false;

    u32 bitmap_block = fs->bgdt[g].bg_inode_bitmap;
    u8 *bitmap = kmalloc(fs->block_size);
    if (!bitmap) return false;

    bool ok = false;
    if (ext2_read_block(fs, bitmap_block, bitmap)) {
        u32 byte_idx = idx / 8;
        u32 bit_idx = idx % 8;
        bitmap[byte_idx] &= ~(1u << bit_idx);
        if (ext2_write_block(fs, bitmap_block, bitmap)) {
            fs->bgdt[g].bg_free_inodes_count++;
            fs->sb.s_free_inodes_count++;
            ok = true;
        }
    }
    kfree(bitmap);
    return ok;
}

bool ext2_sync_super(struct ext2_fs *fs) {
    u32 sb_sector = fs->partition_start + 2;
    return blkdev_write(fs->blkdev_id, sb_sector, 2, &fs->sb) == 0;
}

bool ext2_sync_bgdt(struct ext2_fs *fs) {
    u32 bgdt_block = (fs->sb.s_first_data_block == 0) ? 2 : 1;
    if (fs->block_size == 1024) bgdt_block = 2;

    u32 bgdt_size = fs->groups_count * sizeof(struct ext2_block_group_desc);
    u32 bgdt_blocks = (bgdt_size + fs->block_size - 1) / fs->block_size;

    for (u32 i = 0; i < bgdt_blocks; i++) {
        if (!ext2_write_block(fs, bgdt_block + i,
                              (u8 *)fs->bgdt + i * fs->block_size)) {
            return false;
        }
    }
    return true;
}

bool ext2_write_inode_block(struct ext2_fs *fs, struct ext2_inode *inode, u32 block_idx, const void *buffer) {
    u32 block_num = 0;

    if (block_idx < 12) {
        block_num = inode->i_block[block_idx];
    } else if (block_idx < 12 + 256) {
        u32 indirect_idx = block_idx - 12;
        u8 *indirect_buf = kmalloc(fs->block_size);
        if (!indirect_buf) return false;

        if (!ext2_read_block(fs, inode->i_block[12], indirect_buf)) {
            kfree(indirect_buf);
            return false;
        }

        u32 *indirect = (u32 *)indirect_buf;
        block_num = indirect[indirect_idx];
        kfree(indirect_buf);
    } else {
        return false;
    }

    if (block_num == 0) return false;
    return ext2_write_block(fs, block_num, buffer);
}

bool ext2_format(u32 blkdev_id, u32 partition_start, u32 total_sectors) {
    u32 block_size = 1024;
    u32 sectors_per_block = block_size / 512;
    u32 blocks_count = total_sectors / sectors_per_block;
    if (blocks_count < 64) return false;

    u32 inodes_per_group = 128;
    u32 blocks_per_group = 8192;
    u32 groups_count = (blocks_count + blocks_per_group - 1) / blocks_per_group;

    struct ext2_superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.s_inodes_count = groups_count * inodes_per_group;
    sb.s_blocks_count = blocks_count;
    sb.s_r_blocks_count = 0;
    sb.s_free_blocks_count = blocks_count;
    sb.s_free_inodes_count = sb.s_inodes_count;
    sb.s_first_data_block = 1;
    sb.s_log_block_size = 0;
    sb.s_log_frag_size = 0;
    sb.s_blocks_per_group = blocks_per_group;
    sb.s_frags_per_group = blocks_per_group;
    sb.s_inodes_per_group = inodes_per_group;
    sb.s_magic = EXT2_MAGIC;
    sb.s_state = 1;
    sb.s_errors = 1;
    sb.s_minor_rev_level = 0;
    sb.s_rev_level = 1;
    sb.s_def_resuid = 0;
    sb.s_def_resgid = 0;
    sb.s_first_ino = 11;
    sb.s_inode_size = 128;
    sb.s_block_group_nr = 0;
    sb.s_feature_compat = 0;
    sb.s_feature_incompat = 0;
    sb.s_feature_ro_compat = 0;

    u32 bgdt_blocks = (groups_count * sizeof(struct ext2_block_group_desc) + block_size - 1) / block_size;
    u32 inode_table_blocks = (inodes_per_group * (u32)sb.s_inode_size + block_size - 1) / block_size;
    u32 bgdt_block = 2;

    struct ext2_fs fs;
    memset(&fs, 0, sizeof(fs));
    fs.blkdev_id = blkdev_id;
    fs.partition_start = partition_start;
    memcpy(&fs.sb, &sb, sizeof(sb));
    fs.block_size = block_size;
    fs.sectors_per_block = sectors_per_block;
    fs.inodes_per_block = block_size / sb.s_inode_size;
    fs.groups_count = groups_count;
    fs.mounted = true;

    u32 bgdt_size = groups_count * sizeof(struct ext2_block_group_desc);
    u32 bgdt_blocks_needed = (bgdt_size + block_size - 1) / block_size;
    fs.bgdt = kmalloc(bgdt_blocks_needed * block_size);
    if (!fs.bgdt) return false;

    u8 *zero_block = kmalloc(block_size);
    if (!zero_block) { kfree(fs.bgdt); return false; }
    memset(zero_block, 0, block_size);

    for (u32 g = 0; g < groups_count; g++) {
        u32 base = g * blocks_per_group;
        u32 reserved_count = 3 + bgdt_blocks + 1 + 1 + inode_table_blocks;

        for (u32 i = 0; i < reserved_count; i++) {
            if (!ext2_write_block(&fs, base + i, zero_block)) {
                kfree(zero_block);
                kfree(fs.bgdt);
                return false;
            }
        }

        u32 bbit = base + 3 + bgdt_blocks;
        u32 ibit = bbit + 1;
        u32 itable = ibit + 1;
        u32 first_data = itable + inode_table_blocks;
        u32 group_blocks = (g == groups_count - 1) ? (blocks_count - base) : blocks_per_group;

        struct ext2_block_group_desc bgd;
        memset(&bgd, 0, sizeof(bgd));
        bgd.bg_block_bitmap = bbit;
        bgd.bg_inode_bitmap = ibit;
        bgd.bg_inode_table = itable;
        bgd.bg_free_blocks_count = group_blocks - first_data;
        bgd.bg_free_inodes_count = (g == 0) ? inodes_per_group - 1 : inodes_per_group;
        bgd.bg_used_dirs_count = (g == 0) ? 1 : 0;

        memcpy((u8 *)fs.bgdt + g * sizeof(bgd), &bgd, sizeof(bgd));
    }

    if (blkdev_write(blkdev_id, partition_start + 2, 2, &sb) != 0) {
        kfree(zero_block);
        kfree(fs.bgdt);
        return false;
    }

    for (u32 i = 0; i < bgdt_blocks_needed; i++) {
        if (!ext2_write_block(&fs, bgdt_block + i, (u8 *)fs.bgdt + i * block_size)) {
            kfree(zero_block);
            kfree(fs.bgdt);
            return false;
        }
    }

    for (u32 g = 0; g < groups_count; g++) {
        u32 base = g * blocks_per_group;
        u8 *bmp = kmalloc(block_size);
        if (!bmp) { kfree(zero_block); kfree(fs.bgdt); return false; }

        memset(bmp, 0, block_size);
        u32 group_blocks = (g == groups_count - 1) ? (blocks_count - base) : blocks_per_group;
        u32 reserved_count = 3 + bgdt_blocks + 1 + 1 + inode_table_blocks;

        for (u32 i = 0; i < group_blocks && i < block_size * 8; i++) {
            if (i < reserved_count) {
                bmp[i / 8] |= (1u << (i % 8));
            }
        }
        if (!ext2_write_block(&fs, fs.bgdt[g].bg_block_bitmap, bmp)) {
            kfree(bmp); kfree(zero_block); kfree(fs.bgdt); return false;
        }

        if (g == 0) {
            memset(bmp, 0, block_size);
            bmp[0] = 0x03;
        } else {
            memset(bmp, 0, block_size);
        }
        if (!ext2_write_block(&fs, fs.bgdt[g].bg_inode_bitmap, bmp)) {
            kfree(bmp); kfree(zero_block); kfree(fs.bgdt); return false;
        }
        kfree(bmp);
    }

    kfree(zero_block);

    struct ext2_inode root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.i_mode = EXT2_S_IFDIR | 0755;
    root_inode.i_uid = 0;
    root_inode.i_gid = 0;
    root_inode.i_links_count = 2;
    root_inode.i_size = block_size;

    u32 root_data_block = ext2_alloc_block(&fs);
    if (!root_data_block) { kfree(fs.bgdt); return false; }
    root_inode.i_block[0] = root_data_block;
    root_inode.i_blocks = sectors_per_block / 2;

    if (!ext2_write_inode(&fs, 2, &root_inode)) {
        kfree(fs.bgdt);
        return false;
    }

    u8 *root_data = kmalloc(block_size);
    if (!root_data) { kfree(fs.bgdt); return false; }
    memset(root_data, 0, block_size);

    struct ext2_dir_entry *dot = (struct ext2_dir_entry *)root_data;
    dot->inode = 2;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';

    struct ext2_dir_entry *dotdot = (struct ext2_dir_entry *)(root_data + 12);
    dotdot->inode = 2;
    dotdot->rec_len = block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    if (!ext2_write_block(&fs, root_data_block, root_data)) {
        kfree(root_data);
        kfree(fs.bgdt);
        return false;
    }
    kfree(root_data);

    kfree(fs.bgdt);
    return true;
}

bool ext2_mkdir(struct ext2_fs *fs, u32 parent_inode, const char *name) {
    struct ext2_inode parent;
    if (!ext2_read_inode(fs, parent_inode, &parent)) return false;
    if (!ext2_is_dir(&parent)) return false;

    u32 child = ext2_alloc_inode(fs);
    if (!child) return false;

    u32 data_block = ext2_alloc_block(fs);
    if (!data_block) {
        ext2_free_inode(fs, child);
        return false;
    }

    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFDIR | 0755;
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 2;
    inode.i_size = fs->block_size;
    inode.i_block[0] = data_block;
    inode.i_blocks = fs->sectors_per_block / 2;

    if (!ext2_write_inode(fs, child, &inode)) {
        ext2_free_inode(fs, child);
        ext2_free_block(fs, data_block);
        return false;
    }

    u8 *data = kmalloc(fs->block_size);
    if (!data) return false;
    memset(data, 0, fs->block_size);

    struct ext2_dir_entry *dot = (struct ext2_dir_entry *)data;
    dot->inode = child;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = 2;
    dot->name[0] = '.';

    struct ext2_dir_entry *dotdot = (struct ext2_dir_entry *)(data + 12);
    dotdot->inode = parent_inode;
    dotdot->rec_len = fs->block_size - 12;
    dotdot->name_len = 2;
    dotdot->file_type = 2;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    if (!ext2_write_block(fs, data_block, data)) {
        kfree(data);
        return false;
    }
    kfree(data);

    parent.i_links_count++;
    if (!ext2_write_inode(fs, parent_inode, &parent)) return false;
    fs->bgdt[(parent_inode - 1) / fs->sb.s_inodes_per_group].bg_used_dirs_count++;

    u32 name_len = strlen(name);
    if (name_len > EXT2_NAME_LEN) name_len = EXT2_NAME_LEN;

    u32 entry_size = sizeof(struct ext2_dir_entry) + name_len;
    if (entry_size < 12) entry_size = 12;
    u32 rec_len = (entry_size + 3) & ~3;
    if (rec_len < 12) rec_len = 12;

    u8 *parent_data = kmalloc(fs->block_size);
    if (!parent_data) return false;

    struct ext2_inode parent_inode_data;
    if (!ext2_read_inode(fs, parent_inode, &parent_inode_data)) {
        kfree(parent_data);
        return false;
    }

    u32 num_blocks = (parent_inode_data.i_size + fs->block_size - 1) / fs->block_size;
    bool entry_added = false;

    for (u32 b = 0; b < num_blocks && !entry_added; b++) {
        if (!ext2_read_inode_block(fs, &parent_inode_data, b, parent_data)) {
            kfree(parent_data);
            return false;
        }

        u32 offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(parent_data + offset);
            u32 needed = rec_len;
            u32 available = entry->rec_len;

            if (entry->inode == 0) {
                if (available >= needed) {
                    entry->inode = child;
                    entry->name_len = name_len;
                    entry->file_type = 2;
                    memcpy(entry->name, name, name_len);
                    if (available > needed) {
                        entry->rec_len = needed;
                        struct ext2_dir_entry *next = (struct ext2_dir_entry *)(parent_data + offset + needed);
                        next->inode = 0;
                        next->rec_len = available - needed;
                    }
                    if (ext2_write_inode_block(fs, &parent_inode_data, b, parent_data)) {
                        entry_added = true;
                    }
                    break;
                }
                break;
            }

            u32 waste = available - (sizeof(struct ext2_dir_entry) + entry->name_len);
            if (waste >= needed) {
                entry->rec_len = sizeof(struct ext2_dir_entry) + entry->name_len;
                struct ext2_dir_entry *next = (struct ext2_dir_entry *)(parent_data + offset + entry->rec_len);
                next->inode = child;
                next->rec_len = available - entry->rec_len;
                next->name_len = name_len;
                next->file_type = 2;
                memcpy(next->name, name, name_len);
                if (ext2_write_inode_block(fs, &parent_inode_data, b, parent_data)) {
                    entry_added = true;
                }
                break;
            }

            offset += entry->rec_len;
        }
    }

    kfree(parent_data);
    return entry_added;
}

bool ext2_create_file(struct ext2_fs *fs, u32 parent_inode, const char *name) {
    struct ext2_inode parent;
    if (!ext2_read_inode(fs, parent_inode, &parent)) return false;
    if (!ext2_is_dir(&parent)) return false;

    u32 child = ext2_alloc_inode(fs);
    if (!child) return false;

    struct ext2_inode inode;
    memset(&inode, 0, sizeof(inode));
    inode.i_mode = EXT2_S_IFREG | 0644;
    inode.i_uid = 0;
    inode.i_gid = 0;
    inode.i_links_count = 1;
    inode.i_size = 0;

    if (!ext2_write_inode(fs, child, &inode)) {
        ext2_free_inode(fs, child);
        return false;
    }

    u32 name_len = strlen(name);
    if (name_len > EXT2_NAME_LEN) name_len = EXT2_NAME_LEN;

    u32 entry_size = sizeof(struct ext2_dir_entry) + name_len;
    if (entry_size < 12) entry_size = 12;
    u32 rec_len = (entry_size + 3) & ~3;
    if (rec_len < 12) rec_len = 12;

    u8 *parent_data = kmalloc(fs->block_size);
    if (!parent_data) return false;

    struct ext2_inode parent_inode_data;
    if (!ext2_read_inode(fs, parent_inode, &parent_inode_data)) {
        kfree(parent_data);
        return false;
    }

    u32 num_blocks = (parent_inode_data.i_size + fs->block_size - 1) / fs->block_size;
    bool entry_added = false;

    for (u32 b = 0; b <= num_blocks && !entry_added; b++) {
        bool is_new = (b == num_blocks);
        u8 *block_data;

        if (is_new) {
            if (b >= 12) break; /* beyond direct blocks, not implemented */

            u32 new_block = ext2_alloc_block(fs);
            if (!new_block) break;

            parent_inode_data.i_block[b] = new_block;
            parent_inode_data.i_size += fs->block_size;
            parent_inode_data.i_blocks += fs->sectors_per_block / 2;

            block_data = kmalloc(fs->block_size);
            if (!block_data) break;
            memset(block_data, 0, fs->block_size);

            struct ext2_dir_entry *blank = (struct ext2_dir_entry *)block_data;
            blank->inode = 0;
            blank->rec_len = fs->block_size;

            if (!ext2_write_inode_block(fs, &parent_inode_data, b, block_data)) {
                kfree(block_data);
                break;
            }
        } else {
            block_data = kmalloc(fs->block_size);
            if (!block_data) break;

            if (!ext2_read_inode_block(fs, &parent_inode_data, b, block_data)) {
                kfree(block_data);
                break;
            }
        }

        u32 offset = 0;
        while (offset < fs->block_size) {
            struct ext2_dir_entry *entry = (struct ext2_dir_entry *)(block_data + offset);

            if (entry->inode == 0) {
                if (entry->rec_len >= rec_len) {
                    entry->inode = child;
                    entry->name_len = name_len;
                    entry->file_type = 1;
                    memcpy(entry->name, name, name_len);
                    if (entry->rec_len > rec_len) {
                        entry->rec_len = rec_len;
                        struct ext2_dir_entry *next = (struct ext2_dir_entry *)(block_data + offset + rec_len);
                        next->inode = 0;
                        next->rec_len = entry->rec_len - rec_len;
                    }
                    if (ext2_write_inode_block(fs, &parent_inode_data, b, block_data)) {
                        if (is_new) {
                            if (!ext2_write_inode(fs, parent_inode, &parent_inode_data)) {
                                kfree(block_data);
                                break;
                            }
                        }
                        entry_added = true;
                    }
                    break;
                }
                break;
            }

            if (entry->rec_len == 0) break;
            offset += entry->rec_len;
        }

        kfree(block_data);
        if (entry_added) break;
    }

    kfree(parent_data);
    if (!entry_added) {
        ext2_free_inode(fs, child);
    }
    return entry_added;
}

bool ext2_write_file_data(struct ext2_fs *fs, u32 inode_num, const void *data, u32 size) {
    struct ext2_inode inode;
    if (!ext2_read_inode(fs, inode_num, &inode)) return false;

    u8 *block_data = kmalloc(fs->block_size);
    if (!block_data) return false;

    u32 old_blocks = (inode.i_size + fs->block_size - 1) / fs->block_size;
    u32 new_blocks = (size + fs->block_size - 1) / fs->block_size;

    for (u32 i = new_blocks; i < old_blocks; i++) {
        if (i < 12 && inode.i_block[i]) {
            ext2_free_block(fs, inode.i_block[i]);
            inode.i_block[i] = 0;
        }
    }

    const u8 *src = (const u8 *)data;
    u32 remaining = size;

    for (u32 b = 0; remaining > 0; b++) {
        if (b >= 12) break;

        u32 block_num = inode.i_block[b];
        if (block_num == 0) {
            block_num = ext2_alloc_block(fs);
            if (!block_num) { kfree(block_data); return false; }
            inode.i_block[b] = block_num;
        }

        u32 chunk = remaining < fs->block_size ? remaining : fs->block_size;
        memset(block_data, 0, fs->block_size);
        memcpy(block_data, src, chunk);

        if (!ext2_write_block(fs, block_num, block_data)) {
            kfree(block_data);
            return false;
        }

        src += chunk;
        remaining -= chunk;
    }

    inode.i_size = size;
    inode.i_blocks = (size + fs->block_size - 1) / fs->block_size * (fs->sectors_per_block / 2);

    bool ok = ext2_write_inode(fs, inode_num, &inode);
    kfree(block_data);
    return ok;
}

bool ext2_mkdir_recursive(struct ext2_fs *fs, const char *path) {
    if (!path || path[0] != '/') return false;

    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    u32 current = 2;
    char *token = strtok(path_copy + 1, "/");

    while (token) {
        struct ext2_inode inode;
        if (!ext2_read_inode(fs, current, &inode)) return false;

        u32 child = ext2_dir_lookup(fs, &inode, token);
        if (child == 0) {
            if (!ext2_mkdir(fs, current, token)) return false;
            child = ext2_dir_lookup(fs, &inode, token);
            if (child == 0) return false;
        }
        current = child;
        token = strtok(NULL, "/");
    }

    return true;
}

u32 ext2_create_file_by_path(struct ext2_fs *fs, const char *path) {
    if (!path || path[0] != '/') return 0;

    char path_copy[256];
    strncpy(path_copy, path, 255);
    path_copy[255] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash || last_slash == path_copy) return 0;

    *last_slash = '\0';
    char *filename = last_slash + 1;

    u32 parent = 2;
    if (path_copy[0] != '\0') {
        parent = ext2_find_inode(fs, path_copy);
        if (parent == 0) {
            parent = 2;
        }
    }

    if (!ext2_create_file(fs, parent, filename)) return 0;

    struct ext2_inode pinode;
    if (!ext2_read_inode(fs, parent, &pinode)) return 0;
    u32 child = ext2_dir_lookup(fs, &pinode, filename);

    return child;
}
